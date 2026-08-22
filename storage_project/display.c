#include "display.h"
#include "globals.h"
#include "hardware.h"
#include "qrcodegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define I2C_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

static int i2c_fd;
static uint8_t framebuffer[OLED_WIDTH * OLED_HEIGHT / 8];

// Шрифт 5x8
const unsigned char font5x8[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // пробіл
    {0x00, 0x00, 0x2f, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7f, 0x14, 0x7f, 0x14}, // #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1c, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1c, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x00, 0x50, 0x30, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}  // Z
};

static void oled_command(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    write(i2c_fd, buf, 2);
}

bool display_init(void) {
    i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0 || ioctl(i2c_fd, I2C_SLAVE, I2C_ADDR) < 0) return false;

    // Виправлена конфігурація для 128x64
    uint8_t init_cmds[] = {
        0xAE, 0x20, 0x00, 0x40, 0xA1, 0xC8, 0xA6, 0xA8, 0x3F, 
        0xD3, 0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12, 0xDB, 0x40, 
        0x8D, 0x14, 0xAF
    };
    for (int i = 0; i < (int)sizeof(init_cmds); i++) oled_command(init_cmds[i]);
    return true;
}

static void oled_set_cursor(int page, int col) {
    oled_command(0xB0 + page);
    oled_command(col & 0x0F);
    oled_command(0x10 | (col >> 4));
}

void display_clear(void) {
    for (int page = 0; page < 8; page++) {
        oled_set_cursor(page, 0);
        for (int col = 0; col < 128; col++) {
            uint8_t buf[2] = {0x40, 0x00};
            write(i2c_fd, buf, 2);
        }
    }
}

void display_print(int page, int col, const char* str) {
    oled_set_cursor(page, col);
    for (int i = 0; i < (int)strlen(str); i++) {
        char c = str[i];
        if (c >= 32 && c <= 90) { 
            int font_index = c - 32;
            for (int j = 0; j < 5; j++) {
                uint8_t buf[2] = {0x40, font5x8[font_index][j]};
                write(i2c_fd, buf, 2);
            }
            uint8_t space[2] = {0x40, 0x00};
            write(i2c_fd, space, 2);
        }
    }
}

static void draw_pixel(int x, int y, bool color) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    int index = x + (y / 8) * OLED_WIDTH;
    if (color) framebuffer[index] |= (1 << (y % 8));
    else framebuffer[index] &= ~(1 << (y % 8));
}

static void oled_update(void) {
    // Примусове скидання меж перед виводом буфера
    oled_command(0x21); 
    oled_command(0);    
    oled_command(127);  
    oled_command(0x22); 
    oled_command(0);    
    oled_command(7);    

    uint8_t data[1025];
    data[0] = 0x40;
    memcpy(&data[1], framebuffer, 1024);
    write(i2c_fd, data, 1025);
}

// --- ЕКРАНИ ---

static void draw_status(int mem) {
    display_clear();
    display_print(0, 0, "HARDWARE DROP AP");
    display_print(2, 0, "IP: 10.42.0.1");
    char buf[32];
    snprintf(buf, sizeof(buf), "MEM: %d%%", mem);
    display_print(4, 0, buf);
    display_print(6, 0, "STATUS: IDLE");
}

static void draw_qr(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
    const char *text = "WIFI:T:WPA;S:Hardware_Drop_V2;P:12345678;;";
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
    
    if (qrcodegen_encodeText(text, tempBuffer, qrcode, qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true)) {
        
        int size = qrcodegen_getSize(qrcode);
        int scale = 2; 
        int offset_x = (OLED_WIDTH - size * scale) / 2;
        int offset_y = (OLED_HEIGHT - size * scale) / 2;

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                if (qrcodegen_getModule(qrcode, x, y)) {
                    for (int dy = 0; dy < scale; dy++) {
                        for (int dx = 0; dx < scale; dx++) {
                            draw_pixel(offset_x + x * scale + dx, offset_y + y * scale + dy, true);
                        }
                    }
                }
            }
        }
    }
    oled_update();
}

static void draw_upload(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
    for (int x = 14; x < 114; x++) {
        draw_pixel(x, 40, true);
        draw_pixel(x, 50, true);
    }
    for (int y = 40; y <= 50; y++) {
        draw_pixel(14, y, true);
        draw_pixel(114, y, true);
    }
    static int progress = 16;
    for (int x = 16; x < progress; x++) {
        for (int y = 42; y < 49; y++) draw_pixel(x, y, true);
    }
    progress += 4;
    if (progress > 112) progress = 16;

    oled_update();
    display_print(2, 20, "UPLOADING...");
}

static void draw_matrix(void) {
    for (int i = OLED_WIDTH * OLED_HEIGHT / 8 - 1; i >= OLED_WIDTH; i--) {
        framebuffer[i] = framebuffer[i - OLED_WIDTH];
    }
    for (int x = 0; x < OLED_WIDTH; x++) {
        framebuffer[x] = (rand() % 15 == 0) ? rand() % 256 : 0;
    }
    oled_update();
}

static int dino_y = 40;
static int dino_v = 0; 
static int cactus_x = 120;
static bool is_jumping = false;

static void draw_dino(bool do_jump) {
    memset(framebuffer, 0, sizeof(framebuffer));

    if (do_jump && !is_jumping) {
        dino_v = -6; 
        is_jumping = true;
    }

    dino_y += dino_v;
    dino_v += 1;

    if (dino_y >= 40) {
        dino_y = 40;
        is_jumping = false;
        dino_v = 0;
    }

    cactus_x -= 4; 
    if (cactus_x < 0) cactus_x = 120;

    if (cactus_x > 10 && cactus_x < 22 && dino_y > 32) {
        display_clear();
        display_print(3, 25, "GAME OVER!");
        hardware_play_tone(150, 400);
        usleep(1000000);
        cactus_x = 120;
        return;
    }

    for(int x = 0; x < OLED_WIDTH; x++) draw_pixel(x, 50, true);

    for(int dx = 0; dx < 10; dx++) {
        for(int dy = 0; dy < 10; dy++) draw_pixel(12 + dx, dino_y + dy, true);
    }

    for(int dx = 0; dx < 4; dx++) {
        for(int dy = 0; dy < 10; dy++) draw_pixel(cactus_x + dx, 40 + dy, true);
    }

    oled_update();
}

// --- ГОЛОВНИЙ ЦИКЛ ---
void *display_thread_loop(void *arg) {
    (void)arg;
    int last_screen = -1;
    bool was_uploading = false;

    while(1) {
        pthread_mutex_lock(&state_mutex);
        ScreenMode current = app_state.current_screen;
        bool uploading = app_state.is_uploading;
        int idle = app_state.idle_timer_sec;
        int mem = app_state.free_space_percent;
        
        bool jump = app_state.dino_jump_triggered;
        if (jump) app_state.dino_jump_triggered = false; 
        
        pthread_mutex_unlock(&state_mutex);

        if (uploading) {
            draw_upload();
            was_uploading = true;
        } 
        else if (idle > 60) {
            draw_matrix();
            last_screen = -1; 
        } 
        else {
            if (was_uploading) {
                was_uploading = false;
                last_screen = -1;
            }

            if (current == SCREEN_STATUS) {
                if (last_screen != SCREEN_STATUS) { draw_status(mem); last_screen = SCREEN_STATUS; }
            } else if (current == SCREEN_QR) {
                if (last_screen != SCREEN_QR) { draw_qr(); last_screen = SCREEN_QR; }
            } else if (current == SCREEN_DINO) {
                draw_dino(jump); 
                last_screen = SCREEN_DINO;
            }
        }

        usleep(33000); 
    }
    return NULL;
}