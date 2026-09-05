#include "ui_menu.h"
#include "sdkconfig.h"

#if CONFIG_BOARD_T_EMBED_CC1101

#include "app_settings.h"
#include "board.h"
#include "display.h"
#include "status_led.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_menu";

#define ENC_A   CONFIG_BOARD_ENCODER_A_GPIO
#define ENC_B   CONFIG_BOARD_ENCODER_B_GPIO
#define ENC_BTN CONFIG_BOARD_ENCODER_BTN_GPIO

#define UI_STATUS 0
#define UI_ROOT   1
#define UI_WIFI   2
#define UI_DISP   3
#define UI_LED    4
#define UI_EDIT   5
#define UI_POWER  6
#define UI_STORAGE 7

#define EDIT_SSID 0
#define EDIT_PASS 1

static int s_screen = UI_STATUS;
static int s_sel;
static int s_adjust;
static int s_fmt_confirm;
static int s_edit_kind;
static char s_edit[65];
static int s_edit_pos;
static int s_enc_prev;
static int s_enc_accum;
static int s_btn_down;
static TickType_t s_btn_at;
static TickType_t s_idle_at;

static const char *const k_led_short[] = {
    "Off", "Green", "Blue", "Amber", "Breathe G", "Status",
    "Red", "White", "Purple", "Cyan", "Pink", "Yellow",
    "Breathe B", "Breathe A", "Breathe R", "Rainbow",
    "Heartbeat", "Blink slow", "Blink fast", "Alternate", "Sparkle",
};
_Static_assert(sizeof(k_led_short) / sizeof(k_led_short[0]) == STATUS_LED_MODE_MAX + 1,
               "LED short labels must match STATUS_LED_MODE_MAX");

static const char *k_root[] = {
    "Wi-Fi", "Display", "LED pattern", "Theme", "Storage", "Power", "Home",
};
#define ROOT_N 7
#define ROOT_VIS 5

static const char *k_wifi[] = {
    "SSID", "Password", "Channel", "Max clients", "Save Wi-Fi (reboot)", "Back",
};
#define WIFI_N 6

static const char *k_disp[] = {
    "Screen", "Contrast", "Colors", "Rotate", "Scroll", "Back",
};
#define DISP_N 6

static const char *k_power[] = {
    "Reboot", "Shut down", "Back",
};
#define POWER_N 3

static const char *k_storage[] = {
    "SD card", "Format SD", "Back",
};
#define STORAGE_N 3
#define LED_BACK (STATUS_LED_MODE_MAX + 1)
#define LED_N    (STATUS_LED_MODE_MAX + 2)

static const char k_ssid_cs[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_.";
static const char k_pass_cs[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_.!@#$%^&*?";

static uint16_t col_bg(void) { return display_rgb(0x12, 0x12, 0x22); }
static uint16_t col_fg(void) { return display_rgb(0xf2, 0xf2, 0xf4); }
static uint16_t col_hi(void) { return display_rgb(0x0f, 0x34, 0x60); }
static uint16_t col_ac(void) { return display_rgb(0xff, 0x7a, 0x8a); }
static uint16_t col_hd(void) { return display_rgb(0x16, 0x21, 0x3e); }
static uint16_t col_mu(void) { return display_rgb(0x9a, 0x9a, 0xaa); }

static int window_first(int sel, int count, int vis)
{
    int first = sel - vis / 2;
    if (first < 0) {
        first = 0;
    }
    if (count > vis && first > count - vis) {
        first = count - vis;
    }
    if (first < 0) {
        first = 0;
    }
    return first;
}

static void draw_header(const char *title)
{
    int fh = display_font_h();
    display_fill_rect(0, 0, 320, fh + 4, col_hd());
    display_text(8, 2, title, col_ac(), col_hd());
}

static void draw_footer(const char *hint)
{
    int fh = display_font_h();
    display_fill_rect(0, 170 - fh - 2, 320, fh + 2, col_bg());
    display_text(8, 170 - fh - 2, hint, col_mu(), col_bg());
}

static void draw_row(int index, int selected, const char *left, const char *right)
{
    int fh = display_font_h();
    int y = fh + 6 + index * fh;
    if (y + fh > 170 - fh) {
        return;
    }
    uint16_t bg = selected ? col_hi() : col_bg();
    uint16_t fg = selected ? display_rgb(0xff, 0xff, 0xff) : col_fg();
    display_fill_rect(0, y, 320, fh, bg);
    display_text(8, y, left, fg, bg);
    if (right && right[0]) {
        int n = (int)strlen(right);
        int x = 320 - 8 - n * display_font_w();
        if (x < 140) {
            x = 140;
        }
        display_text(x, y, right, selected ? col_ac() : col_mu(), bg);
    }
}

static void wifi_value(int item, char *out, size_t n)
{
    app_config_t *c = app_config_get();
    switch (item) {
    case 0:
        snprintf(out, n, "%.12s", c->wifi_ssid);
        break;
    case 1:
        snprintf(out, n, c->wifi_pass[0] ? "********" : "(empty)");
        break;
    case 2:
        snprintf(out, n, "%u", (unsigned)c->wifi_channel);
        break;
    case 3:
        snprintf(out, n, "%u", (unsigned)c->wifi_max_conn);
        break;
    default:
        out[0] = '\0';
        break;
    }
}

static void disp_value(int item, char *out, size_t n)
{
    app_config_t *c = app_config_get();
    switch (item) {
    case 0:
        snprintf(out, n, c->disp_on ? "On" : "Off");
        break;
    case 1:
        snprintf(out, n, c->disp_contrast == 0 ? "Low" : (c->disp_contrast == 2 ? "High" : "Med"));
        break;
    case 2:
        snprintf(out, n, c->disp_invert ? "Inverted" : "Normal");
        break;
    case 3:
        snprintf(out, n, c->disp_rotate ? "180 deg" : "Normal");
        break;
    case 4:
        snprintf(out, n, c->enc_rev ? "Reverse" : "Normal");
        break;
    default:
        out[0] = '\0';
        break;
    }
}

static void paint(void)
{
    char val[24];
    display_clear(col_bg());
    if (s_screen == UI_ROOT) {
        draw_header("Settings");
        int first = window_first(s_sel, ROOT_N, ROOT_VIS);
        for (int row = 0; row < ROOT_VIS; row++) {
            int i = first + row;
            if (i >= ROOT_N) {
                break;
            }
            if (i == 3) {
                snprintf(val, sizeof(val), "%s", app_config_get()->theme ? "Light" : "Dark");
                draw_row(row, i == s_sel, k_root[i], val);
            } else {
                draw_row(row, i == s_sel, k_root[i], (i == 6) ? "" : ">");
            }
        }
        draw_footer(s_adjust ? "Turn to change  Click to keep" : "Click to open  Hold = Home");
    } else if (s_screen == UI_WIFI) {
        draw_header("Wi-Fi");
        for (int i = 0; i < WIFI_N; i++) {
            wifi_value(i, val, sizeof(val));
            draw_row(i, i == s_sel, k_wifi[i], val);
        }
        draw_footer(s_adjust ? "Turn to change  Click to keep" : "Click to select  Hold = Back");
    } else if (s_screen == UI_DISP) {
        draw_header("Display");
        for (int i = 0; i < DISP_N; i++) {
            disp_value(i, val, sizeof(val));
            draw_row(i, i == s_sel, k_disp[i], val);
        }
        draw_footer(s_adjust ? "Turn to change  Click to keep" : "Click to adjust  Hold = Back");
    } else if (s_screen == UI_POWER) {
        draw_header("Power");
        for (int i = 0; i < POWER_N; i++) {
            draw_row(i, i == s_sel, k_power[i], "");
        }
        draw_footer("Click to select  Hold = Back");
    } else if (s_screen == UI_STORAGE) {
        draw_header("Storage");
        draw_row(0, s_sel == 0, k_storage[0],
                 app_sd_mounted() ? "OK" : (app_sd_needs_format() ? "No FAT" : "Missing"));
        draw_row(1, s_sel == 1, s_fmt_confirm ? "ERASE now?" : k_storage[1],
                 s_fmt_confirm ? "!!!" : "");
        draw_row(2, s_sel == 2, k_storage[2], "");
        draw_footer(s_fmt_confirm ? "Click to erase ALL files" : "Format: FAT32 + files/");
    } else if (s_screen == UI_LED) {
        draw_header("LED pattern");
        app_config_t *c = app_config_get();
        int first = s_sel - 2;
        if (first < 0) {
            first = 0;
        }
        if (first > LED_N - 5) {
            first = LED_N - 5;
        }
        if (first < 0) {
            first = 0;
        }
        for (int row = 0; row < 5; row++) {
            int mode = first + row;
            if (mode >= LED_N) {
                break;
            }
            if (mode == LED_BACK) {
                draw_row(row, mode == s_sel, "Back", "");
            } else {
                draw_row(row, mode == s_sel, k_led_short[mode], mode == c->led_mode ? "*" : "");
            }
        }
        draw_footer("Click to apply  Hold = Back");
    } else if (s_screen == UI_EDIT) {
        draw_header(s_edit_kind == EDIT_SSID ? "Edit SSID" : "Edit password");
        char shown[28];
        int start = 0;
        int vis = 22;
        if (s_edit_pos >= vis) {
            start = s_edit_pos - vis + 1;
        }
        snprintf(shown, sizeof(shown), "%s", s_edit + start);
        draw_row(0, 0, shown, "");
        int caret_x = 8 + (s_edit_pos - start) * display_font_w();
        display_fill_rect(caret_x, display_font_h() + 6 + display_font_h(),
                          display_font_w(), 2, col_ac());
        char cur[20];
        unsigned char ch = (unsigned char)s_edit[s_edit_pos];
        if (ch == 0) {
            snprintf(cur, sizeof(cur), "char: [end]");
        } else if (ch == ' ') {
            snprintf(cur, sizeof(cur), "char: space");
        } else {
            snprintf(cur, sizeof(cur), "char: %c", ch);
        }
        draw_row(2, 0, cur, "");
        draw_footer("Turn=char  Click=next  Hold=ok");
    }
}

static void enter_menu(void)
{
    display_ui_set_busy(true);
    s_screen = UI_ROOT;
    s_sel = 0;
    s_adjust = 0;
    s_fmt_confirm = 0;
    s_idle_at = xTaskGetTickCount();
    paint();
}

static void leave_menu(void)
{
    s_adjust = 0;
    s_fmt_confirm = 0;
    s_screen = UI_STATUS;
    display_ui_set_busy(false);
    app_display_refresh();
}

static void save_live(void)
{
    app_settings_apply_live();
    (void)app_settings_save();
}

static void start_edit(int kind)
{
    app_config_t *c = app_config_get();
    s_edit_kind = kind;
    if (kind == EDIT_SSID) {
        snprintf(s_edit, sizeof(s_edit), "%s", c->wifi_ssid);
    } else {
        snprintf(s_edit, sizeof(s_edit), "%s", c->wifi_pass);
    }
    s_edit_pos = (int)strlen(s_edit);
    if (s_edit_pos >= (int)sizeof(s_edit) - 1) {
        s_edit_pos = (int)sizeof(s_edit) - 2;
    }
    s_screen = UI_EDIT;
    paint();
}

static const char *charset(void)
{
    return s_edit_kind == EDIT_SSID ? k_ssid_cs : k_pass_cs;
}

static void cycle_char(int dir)
{
    const char *cs = charset();
    int ncs = (int)strlen(cs);
    int max = (s_edit_kind == EDIT_SSID) ? 32 : 64;
    unsigned char cur = (unsigned char)s_edit[s_edit_pos];
    int idx = 0;
    if (cur == 0) {
        /* Append a new character. */
        if (s_edit_pos >= max) {
            return;
        }
        idx = (dir > 0) ? 0 : ncs - 1;
        s_edit[s_edit_pos] = cs[idx];
        s_edit[s_edit_pos + 1] = '\0';
        return;
    }
    const char *p = strchr(cs, (char)cur);
    idx = p ? (int)(p - cs) : 0;
    idx += dir;
    if (idx < 0) {
        idx = ncs - 1;
    }
    if (idx >= ncs) {
        idx = 0;
    }
    s_edit[s_edit_pos] = cs[idx];
}

static void finish_edit(void)
{
    app_config_t *c = app_config_get();
    if (s_edit_kind == EDIT_SSID) {
        strncpy(c->wifi_ssid, s_edit, sizeof(c->wifi_ssid) - 1);
        c->wifi_ssid[sizeof(c->wifi_ssid) - 1] = '\0';
    } else {
        strncpy(c->wifi_pass, s_edit, sizeof(c->wifi_pass) - 1);
        c->wifi_pass[sizeof(c->wifi_pass) - 1] = '\0';
    }
    s_screen = UI_WIFI;
    s_sel = s_edit_kind;
    paint();
}

static void go_back(void)
{
    s_adjust = 0;
    s_fmt_confirm = 0;
    if (s_screen == UI_STATUS) {
        return;
    }
    if (s_screen == UI_ROOT) {
        leave_menu();
        return;
    }
    if (s_screen == UI_WIFI) {
        s_screen = UI_ROOT;
        s_sel = 0;
    } else if (s_screen == UI_DISP) {
        s_screen = UI_ROOT;
        s_sel = 1;
    } else if (s_screen == UI_LED) {
        s_screen = UI_ROOT;
        s_sel = 2;
    } else if (s_screen == UI_STORAGE) {
        s_screen = UI_ROOT;
        s_sel = 4;
    } else if (s_screen == UI_POWER) {
        s_screen = UI_ROOT;
        s_sel = 5;
    } else if (s_screen == UI_EDIT) {
        s_screen = UI_WIFI;
        s_sel = s_edit_kind;
    }
    paint();
}

static void adjust_value(int dir)
{
    app_config_t *c = app_config_get();
    if (s_screen == UI_ROOT && s_sel == 3) {
        c->theme = c->theme ? 0 : 1;
        save_live();
        return;
    }
    if (s_screen == UI_WIFI) {
        if (s_sel == 2) {
            int ch = (int)c->wifi_channel + dir;
            if (ch < 1) {
                ch = 13;
            }
            if (ch > 13) {
                ch = 1;
            }
            c->wifi_channel = (uint8_t)ch;
        } else if (s_sel == 3) {
            int n = (int)c->wifi_max_conn + dir;
            if (n < 1) {
                n = 10;
            }
            if (n > 10) {
                n = 1;
            }
            c->wifi_max_conn = (uint8_t)n;
        }
        return;
    }
    if (s_screen == UI_DISP) {
        if (s_sel == 0) {
            c->disp_on = c->disp_on ? 0 : 1;
        } else if (s_sel == 1) {
            c->disp_contrast = (uint8_t)((c->disp_contrast + (dir > 0 ? 1 : 2)) % 3);
        } else if (s_sel == 2) {
            c->disp_invert = c->disp_invert ? 0 : 1;
        } else if (s_sel == 3) {
            c->disp_rotate = c->disp_rotate ? 0 : 1;
        } else if (s_sel == 4) {
            c->enc_rev = c->enc_rev ? 0 : 1;
        }
        save_live();
        display_ui_set_busy(true);
    }
}

static void on_turn(int dir)
{
    s_idle_at = xTaskGetTickCount();
    if (s_screen == UI_STATUS) {
        return;
    }
    s_fmt_confirm = 0;
    if (app_config_get()->enc_rev) {
        dir = -dir;
    }
    if (s_screen == UI_EDIT) {
        cycle_char(dir);
        paint();
        return;
    }
    if (s_adjust) {
        adjust_value(dir);
        paint();
        return;
    }
    int max = 0;
    if (s_screen == UI_ROOT) {
        max = ROOT_N;
    } else if (s_screen == UI_WIFI) {
        max = WIFI_N;
    } else if (s_screen == UI_DISP) {
        max = DISP_N;
    } else if (s_screen == UI_LED) {
        max = LED_N;
    } else if (s_screen == UI_POWER) {
        max = POWER_N;
    } else if (s_screen == UI_STORAGE) {
        max = STORAGE_N;
    }
    s_sel += dir;
    if (s_sel < 0) {
        s_sel = max - 1;
    }
    if (s_sel >= max) {
        s_sel = 0;
    }
    paint();
}

static void on_click(void)
{
    s_idle_at = xTaskGetTickCount();
    if (s_screen == UI_STATUS) {
        enter_menu();
        return;
    }
    if (s_screen == UI_EDIT) {
        int max = (s_edit_kind == EDIT_SSID) ? 32 : 64;
        if (s_edit[s_edit_pos] == '\0') {
            if (s_edit_pos < max) {
                s_edit[s_edit_pos] = 'A';
                s_edit[s_edit_pos + 1] = '\0';
            }
        } else if (s_edit_pos < max - 1) {
            s_edit_pos++;
        }
        paint();
        return;
    }
    if (s_adjust) {
        s_adjust = 0;
        if (s_screen == UI_WIFI && (s_sel == 2 || s_sel == 3)) {
            /* Channel / max clients apply on Save Wi-Fi. */
        } else {
            save_live();
            display_ui_set_busy(true);
        }
        paint();
        return;
    }
    if (s_screen == UI_ROOT) {
        switch (s_sel) {
        case 0:
            s_screen = UI_WIFI;
            s_sel = 0;
            break;
        case 1:
            s_screen = UI_DISP;
            s_sel = 0;
            break;
        case 2:
            s_screen = UI_LED;
            s_sel = app_config_get()->led_mode;
            break;
        case 3:
            s_adjust = 1;
            break;
        case 4:
            s_screen = UI_STORAGE;
            s_sel = 0;
            s_fmt_confirm = 0;
            break;
        case 5:
            s_screen = UI_POWER;
            s_sel = 0;
            break;
        default:
            leave_menu();
            return;
        }
        paint();
        return;
    }
    if (s_screen == UI_WIFI) {
        switch (s_sel) {
        case 0:
            start_edit(EDIT_SSID);
            return;
        case 1:
            start_edit(EDIT_PASS);
            return;
        case 2:
        case 3:
            s_adjust = 1;
            break;
        case 4:
            (void)app_settings_save();
            draw_header("Rebooting Wi-Fi...");
            vTaskDelay(pdMS_TO_TICKS(400));
            esp_restart();
            break;
        default:
            go_back();
            return;
        }
        paint();
        return;
    }
    if (s_screen == UI_DISP) {
        if (s_sel >= 5) {
            go_back();
            return;
        }
        s_adjust = 1;
        paint();
        return;
    }
    if (s_screen == UI_POWER) {
        if (s_sel == 0) {
            (void)app_settings_save();
            draw_header("Rebooting...");
            vTaskDelay(pdMS_TO_TICKS(400));
            esp_restart();
        } else if (s_sel == 1) {
            (void)app_settings_save();
            status_led_set_mode(STATUS_LED_MODE_OFF);
            display_clear(display_rgb(0x12, 0x12, 0x22));
            draw_header("Shutting down");
            vTaskDelay(pdMS_TO_TICKS(500));
            board_power_off();
        } else {
            go_back();
        }
        return;
    }
    if (s_screen == UI_STORAGE) {
        if (s_sel == 1) {
            if (!s_fmt_confirm) {
                s_fmt_confirm = 1;
                paint();
                return;
            }
            display_clear(col_bg());
            draw_header("Formatting SD");
            draw_footer("Keep power on...");
            esp_err_t err = app_sd_format_init();
            if (err != ESP_OK) {
                s_fmt_confirm = 0;
                display_clear(col_bg());
                draw_header("Format failed");
                draw_footer("Hold = Back");
                vTaskDelay(pdMS_TO_TICKS(2000));
                paint();
                return;
            }
            display_clear(col_bg());
            draw_header("SD ready");
            draw_footer("Rebooting...");
            vTaskDelay(pdMS_TO_TICKS(800));
            esp_restart();
            return;
        }
        s_fmt_confirm = 0;
        if (s_sel == 2) {
            go_back();
        } else {
            paint();
        }
        return;
    }
    if (s_screen == UI_LED) {
        if (s_sel == LED_BACK) {
            go_back();
            return;
        }
        app_config_get()->led_mode = (uint8_t)s_sel;
        save_live();
        display_ui_set_busy(true);
        paint();
    }
}

static void on_hold(void)
{
    s_idle_at = xTaskGetTickCount();
    if (s_screen == UI_STATUS) {
        enter_menu();
        return;
    }
    if (s_screen == UI_EDIT) {
        finish_edit();
        return;
    }
    if (s_adjust) {
        s_adjust = 0;
        paint();
        return;
    }
    if (s_fmt_confirm) {
        s_fmt_confirm = 0;
        paint();
        return;
    }
    go_back();
}

static int enc_state(void)
{
    return (gpio_get_level((gpio_num_t)ENC_A) << 1) | gpio_get_level((gpio_num_t)ENC_B);
}

static void ui_task(void *arg)
{
    (void)arg;
    s_enc_prev = enc_state();
    s_idle_at = xTaskGetTickCount();
    while (1) {
        int st = enc_state();
        if (st != s_enc_prev) {
            /* 00->01->11->10->00 is CW for many T-Embed boards. */
            int trans = (s_enc_prev << 2) | st;
            int step = 0;
            if (trans == 0x1 || trans == 0x7 || trans == 0xe || trans == 0x8) {
                step = 1;
            } else if (trans == 0x2 || trans == 0xb || trans == 0xd || trans == 0x4) {
                step = -1;
            }
            s_enc_prev = st;
            if (step) {
                s_enc_accum += step;
                if (s_enc_accum >= 2 || s_enc_accum <= -2) {
                    int dir = s_enc_accum > 0 ? 1 : -1;
                    s_enc_accum = 0;
                    on_turn(dir);
                }
            }
        }

        int btn = gpio_get_level((gpio_num_t)ENC_BTN) == 0;
        TickType_t now = xTaskGetTickCount();
        if (btn && !s_btn_down) {
            s_btn_down = 1;
            s_btn_at = now;
        } else if (!btn && s_btn_down) {
            s_btn_down = 0;
            TickType_t held = now - s_btn_at;
            if (held >= pdMS_TO_TICKS(650)) {
                on_hold();
            } else if (held >= pdMS_TO_TICKS(30)) {
                on_click();
            }
        }

        if (s_screen != UI_STATUS && s_screen != UI_EDIT &&
            (now - s_idle_at) > pdMS_TO_TICKS(30000)) {
            leave_menu();
        }
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}

void ui_menu_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << ENC_A) | (1ULL << ENC_B) | (1ULL << ENC_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    xTaskCreate(ui_task, "ui_menu", 8192, NULL, 4, NULL);
    ESP_LOGI(TAG, "Encoder A=%d B=%d BTN=%d (click for settings)", ENC_A, ENC_B, ENC_BTN);
}

#else /* !CONFIG_BOARD_T_EMBED_CC1101 */

void ui_menu_init(void)
{
}

#endif
