/*
 * Vibe Pocket File Server
 *
 * - Stores files on SD/microSD card (SPI).
 * - HTTP server: browse, download, upload, delete via web UI.
 * - Runs as WiFi Access Point (softAP); connect to the device's network, no router needed.
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <lwip/sockets.h>
#include <lwip/err.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "nvs_flash.h"
#include "driver/spi_common.h"

static const char *TAG = "file_server";

/* ========== Configuration (change pins to match your wiring) ========== */
#define WIFI_AP_SSID          "Vibe Pocket"
#define WIFI_AP_PASSWORD      "esp32files"
#define WIFI_AP_CHANNEL       1
#define WIFI_AP_MAX_CONN      4

#define SD_MOUNT_POINT        "/sdcard"
#define WEB_ROOT_DIR          "files"   /* Site opens in /sdcard/files; uploads go here by default */
#define SD_WEB_ROOT           SD_MOUNT_POINT "/" WEB_ROOT_DIR
/* ESP32-S3 SD SPI: MISO=12, MOSI=11, SCK=13, CS=5. GND→GND, 3.3V→3.3V */
#define PIN_NUM_MISO          12
#define PIN_NUM_MOSI          11
#define PIN_NUM_CLK           13
#define PIN_NUM_CS            5

#define FILE_PATH_MAX         (256)
#define SCRATCH_BUFSIZE       (8192)
#define TEMPLATE_BUF_SIZE     (4096)
#define WEB_TEMPLATE_PATH     SD_MOUNT_POINT "/www/index.html"
#define PLACEHOLDER_PATH        "{{CURRENT_PATH}}"
#define PLACEHOLDER_PATH_VALUE  "{{CURRENT_PATH_VALUE}}"
#define PLACEHOLDER_FILE_LIST   "{{FILE_LIST}}"
#define DNS_PORT                53
#define CAPTIVE_PORTAL_URL      "http://192.168.4.1/"
#define MAX_FILE_SIZE         (4 * 1024 * 1024)  /* 4 MB max upload */
#define MAX_FILE_SIZE_STR     "4MB"

/* ========== Server context ========== */
struct file_server_data {
    char base_path[64];
    char scratch[SCRATCH_BUFSIZE];
};

/* Fallback when SD template is missing; also used for head/tail around {{FILE_LIST}} */
static const char PAGE_HEAD[] =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Vibe Pocket File Server</title>"
    "<style>"
    "body{font-family:sans-serif;margin:1rem;background:#1a1a2e;color:#eee;}"
    "h1{color:#0f3460;}"
    "a{color:#e94560;}"
    "table{border-collapse:collapse;width:100%;margin:1rem 0;} th,td{border:1px solid #333;padding:0.5rem;text-align:left;}"
    "th{background:#16213e;} .dir{font-weight:bold;} .del{color:#e94560;cursor:pointer;}"
    "form{margin:1rem 0;} input[type=file],button{padding:0.5rem;} button{background:#0f3460;color:#fff;border:none;cursor:pointer;border-radius:4px;}"
    ".path{color:#888;margin:0.5rem 0;} #progressWrap{display:none;margin:1rem 0;} #progressWrap.visible{display:block;}"
    "#progressTrack{background:#333;height:22px;border-radius:4px;overflow:hidden;} #progressBar{background:#e94560;height:100%;width:0%;transition:width 0.15s ease;} #progressPct{margin:0.25rem 0 0;color:#888;font-size:0.9rem;}"
    "</style></head><body><h1>Vibe Pocket File Server</h1>"
    "<form id=\"upform\">Upload: <input type=\"file\" name=\"file\" id=\"file\" required>"
    "<button type=\"submit\">Upload</button></form>"
    "<div id=\"progressWrap\"><div id=\"progressTrack\"><div id=\"progressBar\"></div></div><p id=\"progressPct\">0%</p></div>"
    "<script>document.getElementById('upform').onsubmit=function(e){e.preventDefault();var f=document.getElementById('file').files[0];if(!f)return;"
    "var w=document.getElementById('progressWrap'),b=document.getElementById('progressBar'),p=document.getElementById('progressPct');w.classList.add('visible');b.style.width='0%';p.textContent='0%';"
    "var xhr=new XMLHttpRequest();xhr.upload.onprogress=function(ev){if(ev.lengthComputable){var n=Math.round(100*ev.loaded/ev.total);b.style.width=n+'%';p.textContent=n+'%';}};"
    "xhr.onload=function(){if(xhr.status>=200&&xhr.status<300)window.location.replace('/');else{w.classList.remove('visible');alert('Error: '+xhr.responseText);}};"
    "xhr.onerror=function(){w.classList.remove('visible');alert('Upload failed');};xhr.open('POST','/upload/'+encodeURIComponent(f.name));xhr.send(f);};</script>"
    "<p class=\"path\">Path: ";

#define MKDIR_FORM_PREFIX "<form method=\"POST\" action=\"/mkdir\"><input type=\"hidden\" name=\"path\" value=\""
#define MKDIR_FORM_SUFFIX "\">New folder: <input type=\"text\" name=\"name\" required placeholder=\"Name\"><button type=\"submit\">Create</button></form>"
#define MKDIR_FORM_BUF_SIZE 512   /* prefix + escaped path (up to FILE_PATH_MAX) + suffix */
static char s_mkdir_form_buf[MKDIR_FORM_BUF_SIZE];

/* Escape string for HTML attribute value ( & " < > ) into buf, return buf */
static char *html_escape_attr(const char *src, char *buf, size_t bufsize)
{
    if (!buf || bufsize == 0) return buf;
    size_t i = 0;
    for (; *src && i + 6 < bufsize; src++) {
        if (*src == '&') { memcpy(buf + i, "&amp;", 5); i += 5; }
        else if (*src == '"') { memcpy(buf + i, "&quot;", 6); i += 6; }
        else if (*src == '<') { memcpy(buf + i, "&lt;", 4); i += 4; }
        else if (*src == '>') { memcpy(buf + i, "&gt;", 4); i += 4; }
        else buf[i++] = *src;
    }
    buf[i] = '\0';
    return buf;
}

static const char *mkdir_form_html(const char *uri_path)
{
    const char *p = (uri_path && uri_path[0]) ? uri_path : "/";
    char escaped[FILE_PATH_MAX];
    html_escape_attr(p, escaped, sizeof(escaped));
    snprintf(s_mkdir_form_buf, MKDIR_FORM_BUF_SIZE, "%s%s%s", MKDIR_FORM_PREFIX, escaped, MKDIR_FORM_SUFFIX);
    return s_mkdir_form_buf;
}

static const char TABLE_HEAD[] =
    "</p><table><tr><th>Name</th><th>Type</th><th>Size</th><th></th></tr>";

static const char PAGE_TAIL[] =
    "</table></body></html>";

/* Shown when SD card is not mounted (server still runs, this page is served for GET /) */
static const char PAGE_NO_SD[] =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Vibe Pocket File Server</title>"
    "<style>body{font-family:sans-serif;margin:2rem;background:#1a1a2e;color:#eee;} h1{color:#e94560;} .msg{color:#888;}</style></head><body>"
    "<h1>No SD card mounted</h1>"
    "<p class=\"msg\">Insert an SD card (FAT32) and reset the device to use the file server.</p>"
    "</body></html>";

/* Buffer for template read from SD (one request at a time) */
static char s_template_buf[TEMPLATE_BUF_SIZE];
/* Buffer for path breadcrumb HTML (links for each directory in the path) */
#define BREADCRUMB_BUF_SIZE 384
static char s_breadcrumb_buf[BREADCRUMB_BUF_SIZE];

/* Return 1 if URI is a known captive-portal detection path so we can redirect to our page */
static int is_captive_detection_uri(const char *uri)
{
    if (!uri) return 0;
    if (strcmp(uri, "/generate_204") == 0) return 1;
    if (strcmp(uri, "/hotspot-detect.html") == 0) return 1;
    if (strcmp(uri, "/connectivitycheck") == 0) return 1;
    if (strcmp(uri, "/success.txt") == 0) return 1;
    if (strcmp(uri, "/ncsi.txt") == 0) return 1;
    if (strncmp(uri, "/gen_204", 8) == 0) return 1;
    return 0;
}

/* Build path as clickable links, e.g. "/ subfolder / docs" -> <a href="/">/</a> <a href="/subfolder">subfolder</a> ... */
static const char *path_to_breadcrumb_html(const char *uri_path)
{
    if (!uri_path || uri_path[0] == '\0') uri_path = "/";
    size_t len = 0;
    len += (size_t)snprintf(s_breadcrumb_buf, BREADCRUMB_BUF_SIZE, "<a href=\"/\">/</a>");
    if (len >= BREADCRUMB_BUF_SIZE) return s_breadcrumb_buf;
    if (uri_path[0] == '/' && uri_path[1] == '\0') return s_breadcrumb_buf;
    char path[FILE_PATH_MAX];
    path[0] = '/';
    path[1] = '\0';
    const char *p = uri_path + 1;
    while (p && *p) {
        const char *slash = strchr(p, '/');
        size_t seg_len = slash ? (size_t)(slash - p) : strlen(p);
        if (seg_len == 0) { p = slash ? slash + 1 : NULL; continue; }
        size_t path_len = strlen(path);
        if (path_len + seg_len + 2 > (size_t)FILE_PATH_MAX) break;
        memcpy(path + path_len, p, seg_len + 1);
        path[path_len + seg_len] = '\0';
        int n = snprintf(s_breadcrumb_buf + len, BREADCRUMB_BUF_SIZE - len, " <a href=\"%s\">%.*s</a>", path, (int)seg_len, p);
        if (n < 0 || (len + (size_t)n) >= BREADCRUMB_BUF_SIZE) break;
        len += (size_t)n;
        p = slash ? slash + 1 : NULL;
    }
    return s_breadcrumb_buf;
}

/* Send only the <tr>...</tr> rows for the directory listing. */
static esp_err_t send_dir_rows(httpd_req_t *req, const char *dirpath, const char *base_path)
{
    char entrypath[FILE_PATH_MAX];
    char entrysize[24];
    size_t dirpath_len = strlen(dirpath);
    struct dirent *entry;
    struct stat entry_stat;
    DIR *dir = opendir(dirpath);
    if (!dir) return ESP_FAIL;
    strncpy(entrypath, dirpath, sizeof(entrypath) - 1);
    entrypath[sizeof(entrypath) - 1] = '\0';

    while ((entry = readdir(dir)) != NULL) {
        const char *entrytype = (entry->d_type == DT_DIR) ? "directory" : "file";
        if (dirpath_len + strlen(entry->d_name) + 2 > (size_t)FILE_PATH_MAX) continue;
        strcpy(entrypath + dirpath_len, entry->d_name);
        if (entry->d_type == DT_DIR) {
            size_t n = strlen(entrypath);
            if (n + 1 < sizeof(entrypath)) { entrypath[n] = '/'; entrypath[n + 1] = '\0'; }
        }
        if (stat(entrypath, &entry_stat) != 0) continue;
        snprintf(entrysize, sizeof(entrysize), "%ld", (long)entry_stat.st_size);
        const char *uri = entrypath + strlen(base_path);
        if (httpd_resp_sendstr_chunk(req, "<tr><td class=\"") != ESP_OK ||
            httpd_resp_sendstr_chunk(req, entry->d_type == DT_DIR ? "dir\">" : "\">") != ESP_OK ||
            httpd_resp_sendstr_chunk(req, "<a href=\"") != ESP_OK ||
            httpd_resp_sendstr_chunk(req, uri) != ESP_OK ||
            httpd_resp_sendstr_chunk(req, "\">") != ESP_OK ||
            httpd_resp_sendstr_chunk(req, entry->d_name) != ESP_OK ||
            (entry->d_type == DT_DIR && httpd_resp_sendstr_chunk(req, "/") != ESP_OK) ||
            httpd_resp_sendstr_chunk(req, "</a></td><td>") != ESP_OK ||
            httpd_resp_sendstr_chunk(req, entrytype) != ESP_OK ||
            httpd_resp_sendstr_chunk(req, "</td><td>") != ESP_OK ||
            httpd_resp_sendstr_chunk(req, entrysize) != ESP_OK ||
            httpd_resp_sendstr_chunk(req, "</td><td>") != ESP_OK) {
            closedir(dir);
            return ESP_FAIL;
        }
        if (entry->d_type != DT_DIR) {
            if (httpd_resp_sendstr_chunk(req, "<form style=\"display:inline\" method=\"POST\" action=\"/delete") != ESP_OK ||
                httpd_resp_sendstr_chunk(req, uri) != ESP_OK ||
                httpd_resp_sendstr_chunk(req, "\"><button class=\"del\" type=\"submit\">Delete</button></form>") != ESP_OK) {
                closedir(dir);
                return ESP_FAIL;
            }
        }
        if (httpd_resp_sendstr_chunk(req, "</td></tr>\n") != ESP_OK) {
            closedir(dir);
            return ESP_FAIL;
        }
    }
    closedir(dir);
    return ESP_OK;
}

/* ========== WiFi softAP ========== */
static void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = (uint8_t)strlen(WIFI_AP_SSID),
            .password = WIFI_AP_PASSWORD,
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    if (strlen(WIFI_AP_PASSWORD) < 8) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif && esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "AP started. SSID:%s IP:" IPSTR, WIFI_AP_SSID, IP2STR(&ip_info.ip));
    } else {
        ESP_LOGI(TAG, "AP started. SSID:%s (connect and open http://192.168.4.1)", WIFI_AP_SSID);
    }
}

/* ========== Captive portal DNS server (port 53) ========== */
#define CAPTIVE_AP_IP         "192.168.4.1"
#define DNS_QUERY_BUF_SIZE     256
#define DNS_RESPONSE_BUF_SIZE  512

static void dns_server_task(void *pvParameters)
{
    (void)pvParameters;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket create failed");
        vTaskDelete(NULL);
        return;
    }
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DNS_PORT);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "DNS bind port 53 failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Captive DNS server on port %d", DNS_PORT);

    uint8_t query_buf[DNS_QUERY_BUF_SIZE];
    uint8_t resp_buf[DNS_RESPONSE_BUF_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    while (1) {
        int n = recvfrom(sock, query_buf, sizeof(query_buf), 0, (struct sockaddr *)&from_addr, &from_len);
        if (n < 12) continue;  /* minimum DNS header */

        /* Copy request header and set response flags (QR=1, AA=1, RCODE=0) */
        memcpy(resp_buf, query_buf, 12);
        resp_buf[2] = 0x81;  /* QR=1, Opcode=0, AA=1, TC=0, RD=0 */
        resp_buf[3] = 0x80;  /* RA=1, Z=0, RCODE=0 */
        resp_buf[6] = 0; resp_buf[7] = 0;  /* ANCOUNT = 0 for now; we'll set after building answer */
        resp_buf[8] = 0; resp_buf[9] = 0;
        resp_buf[10] = 0; resp_buf[11] = 0;

        /* Skip question: copy QNAME (labels) + QTYPE + QCLASS into response */
        size_t resp_len = 12;
        const uint8_t *q = query_buf + 12;
        const uint8_t *q_end = query_buf + n;
        while (q < q_end && *q != 0) {
            if (*q > 63) break;  /* invalid label */
            size_t label_len = (size_t)(*q + 1);
            if (q + label_len > q_end || resp_len + label_len + 2 + 2 > sizeof(resp_buf)) break;
            memcpy(resp_buf + resp_len, q, label_len);
            resp_len += label_len;
            q += label_len;
        }
        if (q >= q_end || *q != 0 || q + 4 > q_end) continue;
        resp_buf[resp_len++] = 0;
        resp_buf[resp_len++] = q[0]; resp_buf[resp_len++] = q[1];  /* QTYPE */
        resp_buf[resp_len++] = q[2]; resp_buf[resp_len++] = q[3];  /* QCLASS */
        q += 4;

        /* Answer only A (1) with our AP IP so all hostnames resolve to the device */
        uint16_t qtype = (uint16_t)(query_buf[n - 4] << 8) | query_buf[n - 3];
        if (qtype == 1) {
            resp_buf[resp_len++] = 0xc0;
            resp_buf[resp_len++] = 0x0c;  /* pointer to question name */
            resp_buf[resp_len++] = 0x00;
            resp_buf[resp_len++] = 0x01;  /* TYPE A */
            resp_buf[resp_len++] = 0x00;
            resp_buf[resp_len++] = 0x01;  /* CLASS IN */
            resp_buf[resp_len++] = 0x00; resp_buf[resp_len++] = 0x00;
            resp_buf[resp_len++] = 0x00; resp_buf[resp_len++] = 0x3c;  /* TTL 60 */
            resp_buf[resp_len++] = 0x00;
            resp_buf[resp_len++] = 0x04;  /* RDLENGTH 4 */
            resp_buf[resp_len++] = 192;
            resp_buf[resp_len++] = 168;
            resp_buf[resp_len++] = 4;
            resp_buf[resp_len++] = 1;
            resp_buf[6] = 0;
            resp_buf[7] = 1;  /* ANCOUNT = 1 */
        }

        sendto(sock, resp_buf, (size_t)resp_len, 0, (struct sockaddr *)&from_addr, from_len);
    }
}

/* ========== SD card mount ========== */
static esp_err_t sdcard_mount(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_card_t *card = NULL;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(ret));
        spi_bus_free(host.slot);
        return ret;
    }
    ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
    (void)card;
    return ESP_OK;
}

/* ========== HTTP helpers ========== */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Decode %XX to bytes in place. */
static void uri_decode_in_place(char *s)
{
    char *w = s;
    while (*s) {
        if (s[0] == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            int a = isdigit((unsigned char)s[1]) ? (s[1] - '0') : (tolower((unsigned char)s[1]) - 'a' + 10);
            int b = isdigit((unsigned char)s[2]) ? (s[2] - '0') : (tolower((unsigned char)s[2]) - 'a' + 10);
            *w++ = (char)((a << 4) | b);
            s += 3;
        } else {
            *w++ = *s++;
        }
    }
    *w = '\0';
}

/* Replace FAT-invalid and non-ASCII chars with underscore so fopen doesn't get EINVAL. */
static void sanitize_path_for_fat(char *s)
{
    for (; *s; s++) {
        if (*s == '/') continue;
        if ((unsigned char)*s < 32 || (unsigned char)*s == 127)  /* control chars and DEL */
            *s = '_';
        else if (*s == '\\' || *s == ':' || *s == '*' || *s == '?' || *s == '"' || *s == '<' || *s == '>' || *s == '|')
            *s = '_';
        else if ((unsigned char)*s > 127)
            *s = '_';
    }
}

/* Make last path component 8.3-safe (8-char name + dot + 3-char ext) for FAT without LFN. In place. */
static void path_to_83_last_component(char *path)
{
    char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    size_t len = strlen(base);
    if (len == 0) return;
    char *dot = strrchr(base, '.');
    size_t name_len = dot ? (size_t)(dot - base) : len;
    size_t ext_len = dot && dot[1] ? strlen(dot + 1) : 0;
    if (ext_len > 3) ext_len = 3;
    if (name_len <= 8 && ext_len <= 3) return;  /* already 8.3 */
    char buf[13];  /* 8 + 1 + 3 + null */
    size_t i = 0;
    for (; i < 8 && i < name_len && base[i]; i++)
        buf[i] = (char)toupper((unsigned char)base[i]);
    for (; i < 8; i++) buf[i] = '\0';
    if (dot && dot[1]) {
        buf[8] = '.';
        for (i = 0; i < ext_len && dot[1 + i]; i++)
            buf[9 + i] = (char)toupper((unsigned char)dot[1 + i]);
        buf[9 + i] = '\0';
    } else {
        buf[8] = '\0';
    }
    strcpy(base, buf);
}

static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    size_t base_len = strlen(base_path);
    size_t pathlen = strlen(uri);
    const char *q = strchr(uri, '?');
    if (q) pathlen = MIN(pathlen, (size_t)(q - uri));
    const char *h = strchr(uri, '#');
    if (h) pathlen = MIN(pathlen, (size_t)(h - uri));
    if (base_len + pathlen + 1 > destsize) return NULL;
    strcpy(dest, base_path);
    strncpy(dest + base_len, uri, pathlen);
    dest[base_len + pathlen] = '\0';
    return dest + base_len;
}

#define IS_FILE_EXT(filename, ext) \
    (strlen(filename) >= sizeof(ext) - 1 && strcasecmp(filename + strlen(filename) - (sizeof(ext) - 1), (ext)) == 0)

static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) return httpd_resp_set_type(req, "application/pdf");
    if (IS_FILE_EXT(filename, ".html")) return httpd_resp_set_type(req, "text/html");
    if (IS_FILE_EXT(filename, ".htm"))  return httpd_resp_set_type(req, "text/html");
    if (IS_FILE_EXT(filename, ".css"))  return httpd_resp_set_type(req, "text/css");
    if (IS_FILE_EXT(filename, ".js"))   return httpd_resp_set_type(req, "application/javascript");
    if (IS_FILE_EXT(filename, ".png"))  return httpd_resp_set_type(req, "image/png");
    if (IS_FILE_EXT(filename, ".jpg"))  return httpd_resp_set_type(req, "image/jpeg");
    if (IS_FILE_EXT(filename, ".jpeg")) return httpd_resp_set_type(req, "image/jpeg");
    if (IS_FILE_EXT(filename, ".gif"))  return httpd_resp_set_type(req, "image/gif");
    if (IS_FILE_EXT(filename, ".ico"))  return httpd_resp_set_type(req, "image/x-icon");
    if (IS_FILE_EXT(filename, ".svg"))  return httpd_resp_set_type(req, "image/svg+xml");
    if (IS_FILE_EXT(filename, ".json")) return httpd_resp_set_type(req, "application/json");
    return httpd_resp_set_type(req, "application/octet-stream");
}

/* Send directory listing HTML. Uses /sdcard/www/index.html if present and contains {{FILE_LIST}}. */
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    const char *base_path = ((struct file_server_data *)req->user_ctx)->base_path;
    const char *uri_path = dirpath + strlen(base_path);
    if (uri_path[0] == '\0') uri_path = "/";

    FILE *tf = fopen(WEB_TEMPLATE_PATH, "r");
    if (tf) {
        size_t n = fread(s_template_buf, 1, TEMPLATE_BUF_SIZE - 1, tf);
        fclose(tf);
        s_template_buf[n] = '\0';
        const char *pl_path = strstr(s_template_buf, PLACEHOLDER_PATH);
        const char *pl_path_value = strstr(s_template_buf, PLACEHOLDER_PATH_VALUE);
        const char *pl_list = strstr(s_template_buf, PLACEHOLDER_FILE_LIST);
        if (pl_list) {
            size_t len_path = strlen(PLACEHOLDER_PATH);
            size_t len_path_value = strlen(PLACEHOLDER_PATH_VALUE);
            size_t len_list = strlen(PLACEHOLDER_FILE_LIST);
            esp_err_t send_ret = ESP_OK;
            if (pl_path && pl_path < pl_list) {
                if (httpd_resp_send_chunk(req, s_template_buf, (size_t)(pl_path - s_template_buf)) != ESP_OK ||
                    httpd_resp_sendstr_chunk(req, path_to_breadcrumb_html(uri_path)) != ESP_OK ||
                    httpd_resp_send_chunk(req, pl_path + len_path, (size_t)(pl_path_value && pl_path_value > pl_path && pl_path_value < pl_list ? (pl_path_value - (pl_path + len_path)) : (pl_list - (pl_path + len_path)))) != ESP_OK)
                    send_ret = ESP_FAIL;
                if (send_ret == ESP_OK && pl_path_value && pl_path_value > pl_path && pl_path_value < pl_list) {
                    char esc_path[FILE_PATH_MAX];
                    html_escape_attr((uri_path[0] ? uri_path : "/"), esc_path, sizeof(esc_path));
                    if (httpd_resp_sendstr_chunk(req, esc_path) != ESP_OK ||
                        httpd_resp_send_chunk(req, pl_path_value + len_path_value, (size_t)(pl_list - (pl_path_value + len_path_value))) != ESP_OK)
                        send_ret = ESP_FAIL;
                }
            } else {
                if (httpd_resp_send_chunk(req, s_template_buf, (size_t)(pl_list - s_template_buf)) != ESP_OK)
                    send_ret = ESP_FAIL;
            }
            if (send_ret == ESP_OK && send_dir_rows(req, dirpath, base_path) != ESP_OK)
                send_ret = ESP_FAIL;
            if (send_ret == ESP_OK && httpd_resp_send_chunk(req, pl_list + len_list, strlen(pl_list + len_list)) != ESP_OK)
                send_ret = ESP_FAIL;
            httpd_resp_sendstr_chunk(req, NULL);
            return send_ret;
        }
    }
    /* Fallback: embedded HTML */
    if (httpd_resp_sendstr_chunk(req, PAGE_HEAD) != ESP_OK ||
        httpd_resp_sendstr_chunk(req, path_to_breadcrumb_html(uri_path)) != ESP_OK ||
        httpd_resp_sendstr_chunk(req, mkdir_form_html(uri_path)) != ESP_OK ||
        httpd_resp_sendstr_chunk(req, TABLE_HEAD) != ESP_OK) {
        return ESP_FAIL;
    }
    if (send_dir_rows(req, dirpath, base_path) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory not found");
        return ESP_FAIL;
    }
    if (httpd_resp_sendstr_chunk(req, PAGE_TAIL) != ESP_OK) {
        return ESP_FAIL;
    }
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

/* GET handler: serve file or directory listing */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    /* Captive portal: redirect detection URIs to our file server */
    if (is_captive_detection_uri(req->uri)) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", CAPTIVE_PORTAL_URL);
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    struct file_server_data *ctx = (struct file_server_data *)req->user_ctx;
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    if (ctx->base_path[0] == '\0') {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_sendstr(req, PAGE_NO_SD);
        return ESP_OK;
    }

    const char *filename = get_path_from_uri(filepath, ctx->base_path, req->uri, sizeof(filepath));
    if (!filename) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Path too long");
        return ESP_FAIL;
    }
    uri_decode_in_place((char *)filename);
    sanitize_path_for_fat(filepath);

    if (filename[0] == '\0' || (strlen(filename) > 0 && filename[strlen(filename) - 1] == '/')) {
        return http_resp_dir_html(req, filepath);
    }

    if (stat(filepath, &file_stat) != 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    if (S_ISDIR(file_stat.st_mode)) {
        return http_resp_dir_html(req, filepath);
    }

    fd = fopen(filepath, "rb");
    if (!fd) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filename);
    size_t chunksize;
    do {
        chunksize = fread(ctx->scratch, 1, SCRATCH_BUFSIZE, fd);
        if (chunksize > 0 && httpd_resp_send_chunk(req, ctx->scratch, chunksize) != ESP_OK) {
            fclose(fd);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    } while (chunksize != 0);
    fclose(fd);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* POST upload handler: expects raw body (use JS: fetch('/upload/filename', { method:'POST', body: file })). */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    struct file_server_data *ctx = (struct file_server_data *)req->user_ctx;
    char filepath[FILE_PATH_MAX];
    const char *target = "/upload";

    if (ctx->base_path[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No SD card mounted");
        return ESP_FAIL;
    }

    const char *filename = get_path_from_uri(filepath, ctx->base_path, req->uri + strlen(target), sizeof(filepath));
    if (!filename || filename[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid path");
        return ESP_FAIL;
    }
    uri_decode_in_place((char *)filename);
    sanitize_path_for_fat(filepath);
    /* Trim trailing slash */
    size_t flen = strlen(filename);
    if (flen > 0 && filename[flen - 1] == '/') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }
    if ((size_t)req->content_len > MAX_FILE_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File too large (max " MAX_FILE_SIZE_STR ")");
        return ESP_FAIL;
    }

    /* Create parent directories if needed (e.g. /upload/folder/file.txt) */
    for (char *p = filepath + strlen(ctx->base_path) + 1; (p = strchr(p, '/')) != NULL; p++) {
        *p = '\0';
        if (mkdir(filepath, 0755) != 0 && errno != EEXIST) {
            ESP_LOGE(TAG, "Failed to create directory: %s (errno %d)", filepath, errno);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directory");
            return ESP_FAIL;
        }
        *p = '/';
    }

    FILE *fd = fopen(filepath, "wb");
    if (!fd && errno == EINVAL) {
        path_to_83_last_component(filepath);
        fd = fopen(filepath, "wb");
        if (fd)
            ESP_LOGI(TAG, "Created with 8.3 name: %s", filepath);
    }
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file: %s (errno %d: %s)", filepath, errno, strerror(errno));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    int remaining = (int)req->content_len;
    char *buf = ctx->scratch;

    while (remaining > 0) {
        int to_recv = remaining > (int)SCRATCH_BUFSIZE ? (int)SCRATCH_BUFSIZE : remaining;
        int received = httpd_req_recv(req, buf, to_recv);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            fclose(fd);
            remove(filepath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }
        remaining -= received;
        if (fwrite(buf, 1, (size_t)received, fd) != (size_t)received) {
            fclose(fd);
            remove(filepath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            return ESP_FAIL;
        }
    }
    if (fclose(fd) != 0) {
        remove(filepath);
        ESP_LOGE(TAG, "Failed to close file after upload: %s (errno %d)", filepath, errno);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save file");
        return ESP_FAIL;
    }
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "Uploaded. <a href=\"/\">Back to list</a>");
    return ESP_OK;
}

/* POST delete handler: delete file */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    struct file_server_data *ctx = (struct file_server_data *)req->user_ctx;
    char filepath[FILE_PATH_MAX];
    const char *prefix = "/delete";

    if (ctx->base_path[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No SD card mounted");
        return ESP_FAIL;
    }

    const char *filename = get_path_from_uri(filepath, ctx->base_path, req->uri + strlen(prefix), sizeof(filepath));
    if (!filename || filename[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid path");
        return ESP_FAIL;
    }
    uri_decode_in_place((char *)filename);
    sanitize_path_for_fat(filepath);
    size_t flen = strlen(filename);
    if (flen > 0 && filename[flen - 1] == '/') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Cannot delete directory");
        return ESP_FAIL;
    }
    struct stat st;
    if (stat(filepath, &st) != 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    if (remove(filepath) != 0) {
        ESP_LOGE(TAG, "Failed to delete file: %s (errno %d: %s)", filepath, errno, strerror(errno));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
        return ESP_FAIL;
    }
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "Deleted. <a href=\"/\">Back to list</a>");
    return ESP_OK;
}

/* Parse urlencoded body (null-terminated): find "key=" and copy value until '&' or end into out. Return 1 if found. */
static int parse_form_value(const char *body, const char *key, char *out, size_t out_size)
{
    size_t key_len = strlen(key);
    char search[32];
    if (key_len + 2 >= sizeof(search)) return 0;
    snprintf(search, sizeof(search), "%s=", key);
    const char *p = strstr(body, search);
    if (!p) return 0;
    p += strlen(search);
    const char *v_end = strchr(p, '&');
    size_t v_len = v_end ? (size_t)(v_end - p) : strlen(p);
    if (v_len >= out_size) v_len = out_size - 1;
    memcpy(out, p, v_len);
    out[v_len] = '\0';
    return 1;
}

/* POST mkdir handler: create directory and redirect back to current path */
static esp_err_t mkdir_post_handler(httpd_req_t *req)
{
    struct file_server_data *ctx = (struct file_server_data *)req->user_ctx;
    char filepath[FILE_PATH_MAX];
    char path_param[FILE_PATH_MAX];
    char name_param[256];

    if (ctx->base_path[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No SD card mounted");
        return ESP_FAIL;
    }

    size_t body_len = req->content_len;
    if (body_len == 0 || body_len > sizeof(ctx->scratch)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }
    int r = httpd_req_recv(req, ctx->scratch, body_len);
    if (r <= 0 || (size_t)r != body_len) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    ctx->scratch[body_len] = '\0';

    if (!parse_form_value(ctx->scratch, "path", path_param, sizeof(path_param)) ||
        !parse_form_value(ctx->scratch, "name", name_param, sizeof(name_param))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing path or name");
        return ESP_FAIL;
    }

    uri_decode_in_place(path_param);
    uri_decode_in_place(name_param);
    if (name_param[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Folder name required");
        return ESP_FAIL;
    }

    size_t base_len = strlen(ctx->base_path);
    if (base_len + strlen(path_param) + strlen(name_param) + 3 > (size_t)FILE_PATH_MAX) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Path too long");
        return ESP_FAIL;
    }
    strncpy(filepath, ctx->base_path, sizeof(filepath) - 1);
    filepath[sizeof(filepath) - 1] = '\0';
    size_t n = strlen(filepath);
    if (path_param[0] == '/' && path_param[1] != '\0')
        n += (size_t)snprintf(filepath + n, sizeof(filepath) - n, "%s", path_param);
    else if (path_param[0] != '\0')
        n += (size_t)snprintf(filepath + n, sizeof(filepath) - n, "/%s", path_param);
    if (n + strlen(name_param) + 2 > (size_t)FILE_PATH_MAX) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Path too long");
        return ESP_FAIL;
    }
    if (filepath[n - 1] != '/') filepath[n++] = '/';
    n += (size_t)snprintf(filepath + n, sizeof(filepath) - n, "%s", name_param);
    filepath[n] = '\0';
    sanitize_path_for_fat(filepath);
    /* Sanitize folder name into the last segment only */
    char *last_slash = strrchr(filepath, '/');
    if (last_slash && last_slash[1]) {
        char *seg = last_slash + 1;
        for (; *seg; seg++)
            if (!isalnum((unsigned char)*seg) && *seg != '_' && *seg != '-' && *seg != '.')
                *seg = '_';
    }
    if (mkdir(filepath, 0755) != 0) {
        ESP_LOGE(TAG, "mkdir %s failed: errno %d", filepath, errno);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create folder");
        return ESP_FAIL;
    }
    /* Redirect to parent path so the new folder appears in the listing */
    const char *uri_path = filepath + base_len;
    const char *last_slash_uri = strrchr(uri_path, '/');
    size_t parent_len = last_slash_uri && last_slash_uri > uri_path ? (size_t)(last_slash_uri - uri_path) : 0;
    char loc[FILE_PATH_MAX + 32];
    if (parent_len > 0) {
        memcpy(loc, "http://192.168.4.1", 18);
        memcpy(loc + 18, uri_path, parent_len);
        loc[18 + parent_len] = '\0';
    } else {
        snprintf(loc, sizeof(loc), "http://192.168.4.1/");
    }
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", loc);
    httpd_resp_sendstr(req, "Created. <a href=\"/\">Back to list</a>");
    return ESP_OK;
}

static esp_err_t start_http_file_server(const char *base_path)
{
    static struct file_server_data *server_data = NULL;
    if (server_data) return ESP_ERR_INVALID_STATE;
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) return ESP_ERR_NO_MEM;
    if (base_path) {
        strncpy(server_data->base_path, base_path, sizeof(server_data->base_path) - 1);
        server_data->base_path[sizeof(server_data->base_path) - 1] = '\0';
    } else {
        server_data->base_path[0] = '\0';
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 10;

    if (httpd_start(&server, &config) != ESP_OK) {
        free(server_data);
        server_data = NULL;
        return ESP_FAIL;
    }

    httpd_uri_t get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = server_data,
    };
    httpd_register_uri_handler(server, &get_uri);

    httpd_uri_t upload_uri = {
        .uri = "/upload/*",
        .method = HTTP_POST,
        .handler = upload_post_handler,
        .user_ctx = server_data,
    };
    httpd_register_uri_handler(server, &upload_uri);

    httpd_uri_t delete_uri = {
        .uri = "/delete/*",
        .method = HTTP_POST,
        .handler = delete_post_handler,
        .user_ctx = server_data,
    };
    httpd_register_uri_handler(server, &delete_uri);

    httpd_uri_t mkdir_uri = {
        .uri = "/mkdir",
        .method = HTTP_POST,
        .handler = mkdir_post_handler,
        .user_ctx = server_data,
    };
    httpd_register_uri_handler(server, &mkdir_uri);

    ESP_LOGI(TAG, "HTTP server on port %d, base path %s", config.server_port, base_path ? base_path : "(no SD)");
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Vibe Pocket File Server (WiFi AP)");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_softap();

    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);

    ret = sdcard_mount();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card not mounted. Server will run with \"No SD card\" page.");
    } else {
        if (mkdir(SD_WEB_ROOT, 0755) != 0 && errno != EEXIST) {
            ESP_LOGW(TAG, "Could not create web root %s (errno %d)", SD_WEB_ROOT, errno);
        }
    }

    ret = start_http_file_server(ret == ESP_OK ? SD_WEB_ROOT : NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    ESP_LOGI(TAG, "Ready. Connect to WiFi \"%s\", then open http://192.168.4.1", WIFI_AP_SSID);
}
