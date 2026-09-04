#include "ui_menu.h"
#include "sdkconfig.h"

#if CONFIG_BOARD_T_EMBED_CC1101

#include "app_settings.h"
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

#define EDIT_SSID 0
#define EDIT_PASS 1

static int s_screen = UI_STATUS;
static int s_sel;
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
    "Wi-Fi", "Display", "LED pattern", "Theme", "Reboot", "Back",
};
#define ROOT_N 6

static const char *k_wifi[] = {
    "SSID", "Password", "Channel", "Max clients", "Save Wi-Fi (reboot)", "Back",
};
#define WIFI_N 6

static const char *k_disp[] = {
    "Screen", "Contrast", "Colors", "Rotate", "Back",
};
#define DISP_N 5

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
        for (int i = 0; i < ROOT_N; i++) {
            if (i == 3) {
                snprintf(val, sizeof(val), "%s", app_config_get()->theme ? "Light" : "Dark");
                draw_row(i, i == s_sel, k_root[i], val);
            } else {
                draw_row(i, i == s_sel, k_root[i], i < 3 ? ">" : "");
            }
        }
        draw_footer("Turn  Click  Hold=back");
    } else if (s_screen == UI_WIFI) {
        draw_header("Wi-Fi");
        for (int i = 0; i < WIFI_N; i++) {
            wifi_value(i, val, sizeof(val));
            draw_row(i, i == s_sel, k_wifi[i], val);
        }
        draw_footer("SSID/pass: click to edit");
    } else if (s_screen == UI_DISP) {
        draw_header("Display");
        for (int i = 0; i < DISP_N; i++) {
            disp_value(i, val, sizeof(val));
            draw_row(i, i == s_sel, k_disp[i], val);
        }
        draw_footer("Click to change");
    } else if (s_screen == UI_LED) {
        draw_header("LED pattern");
        app_config_t *c = app_config_get();
        int first = s_sel - 2;
        if (first < 0) {
            first = 0;
        }
        if (first > STATUS_LED_MODE_MAX - 4) {
            first = STATUS_LED_MODE_MAX - 4;
        }
        if (first < 0) {
            first = 0;
        }
        for (int row = 0; row < 5; row++) {
            int mode = first + row;
            if (mode > STATUS_LED_MODE_MAX) {
                break;
            }
            draw_row(row, mode == s_sel, k_led_short[mode], mode == c->led_mode ? "*" : "");
        }
        draw_footer("Click to apply");
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
    s_idle_at = xTaskGetTickCount();
    paint();
}

static void leave_menu(void)
{
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

static void on_turn(int dir)
{
    s_idle_at = xTaskGetTickCount();
    if (s_screen == UI_STATUS) {
        return;
    }
    if (s_screen == UI_EDIT) {
        cycle_char(dir);
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
        max = STATUS_LED_MODE_MAX + 1;
    }
    s_sel += dir;
    if (s_sel < 0) {
        s_sel = max - 1;
    }
    if (s_sel >= max) {
        s_sel = 0;
    }
    if (s_screen == UI_LED) {
        app_config_get()->led_mode = (uint8_t)s_sel;
        status_led_set_mode((uint8_t)s_sel);
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
            if (s_edit[s_edit_pos] == '\0') {
                /* stay on terminator so turn can append */
            }
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
            app_config_get()->theme = app_config_get()->theme ? 0 : 1;
            save_live();
            break;
        case 4:
            (void)app_settings_save();
            esp_restart();
            break;
        default:
            leave_menu();
            return;
        }
        paint();
        return;
    }
    if (s_screen == UI_WIFI) {
        app_config_t *c = app_config_get();
        switch (s_sel) {
        case 0:
            start_edit(EDIT_SSID);
            return;
        case 1:
            start_edit(EDIT_PASS);
            return;
        case 2:
            c->wifi_channel = (uint8_t)(c->wifi_channel >= 13 ? 1 : c->wifi_channel + 1);
            break;
        case 3:
            c->wifi_max_conn = (uint8_t)(c->wifi_max_conn >= 10 ? 1 : c->wifi_max_conn + 1);
            break;
        case 4:
            (void)app_settings_save();
            draw_header("Rebooting Wi-Fi...");
            vTaskDelay(pdMS_TO_TICKS(400));
            esp_restart();
            break;
        default:
            s_screen = UI_ROOT;
            s_sel = 0;
            break;
        }
        paint();
        return;
    }
    if (s_screen == UI_DISP) {
        app_config_t *c = app_config_get();
        switch (s_sel) {
        case 0:
            c->disp_on = c->disp_on ? 0 : 1;
            break;
        case 1:
            c->disp_contrast = (uint8_t)((c->disp_contrast + 1) % 3);
            break;
        case 2:
            c->disp_invert = c->disp_invert ? 0 : 1;
            break;
        case 3:
            c->disp_rotate = c->disp_rotate ? 0 : 1;
            break;
        default:
            s_screen = UI_ROOT;
            s_sel = 1;
            paint();
            return;
        }
        save_live();
        display_ui_set_busy(true);
        paint();
        return;
    }
    if (s_screen == UI_LED) {
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
    if (s_screen == UI_ROOT) {
        leave_menu();
        return;
    }
    s_screen = UI_ROOT;
    s_sel = 0;
    paint();
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
    xTaskCreate(ui_task, "ui_menu", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Encoder A=%d B=%d BTN=%d (click for settings)", ENC_A, ENC_B, ENC_BTN);
}

#else /* !CONFIG_BOARD_T_EMBED_CC1101 */

void ui_menu_init(void)
{
}

#endif
