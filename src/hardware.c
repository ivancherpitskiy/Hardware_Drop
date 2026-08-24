#include "hardware.h"
#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

// --- ПІНИ ---
#define BTN_1_PIN 17 // Вимкнення / Система
#define BTN_2_PIN 27 // Перемикання екранів
#define BTN_3_PIN 22 // Дія / Стрибок / Вимкнути сирену
#define BUZZER_PIN 18 // Зумер (PWM)

// --- РОБОТА З ПАМ'ЯТТЮ ---
static volatile uint32_t *gpio;

#define READ_PIN(g) (*(gpio + 13) & (1 << (g)))
#define SET_OUTPUT(g) { *(gpio + (g)/10) &= ~(7 << (((g)%10)*3)); *(gpio + (g)/10) |= (1 << (((g)%10)*3)); }
#define PIN_ON(g)  *(gpio + 7) = (1 << (g))
#define PIN_OFF(g) *(gpio + 10) = (1 << (g))

bool hardware_init(void) {
    // Вмикаємо підтяжку (Pull-up)
    system("pinctrl set 17 ip pu > /dev/null 2>&1");
    system("pinctrl set 27 ip pu > /dev/null 2>&1");
    system("pinctrl set 22 ip pu > /dev/null 2>&1");

    // Відкриваємо доступ до пам'яті
    int mem_fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    if (mem_fd < 0) return false;

    gpio = (volatile uint32_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
    close(mem_fd);

    if (gpio == MAP_FAILED) return false;

    // Налаштовуємо зумер
    SET_OUTPUT(BUZZER_PIN);
    PIN_OFF(BUZZER_PIN);

    return true;
}

void hardware_play_tone(int frequency, int duration_ms) {
    if (frequency == 0) {
        usleep(duration_ms * 1000);
        return;
    }
    long period_us = 1000000 / frequency;
    long half_period_us = period_us / 2;
    long cycles = (duration_ms * 1000L) / period_us;

    for (long i = 0; i < cycles; i++) {
        PIN_ON(BUZZER_PIN);
        usleep(half_period_us);
        PIN_OFF(BUZZER_PIN);
        usleep(half_period_us);
    }
}

// --- ЛОГІКА АПАРАТНОГО ПОТОКУ ---
void *hardware_thread_loop(void *arg) {
    (void)arg;
    printf("[HW] Thread started.\n");

    int state_b1 = 1; // last_b1 прибрали, він тут не потрібен
    int state_b2 = 1, last_b2 = 1;
    int state_b3 = 1, last_b3 = 1;

    int b1_press_duration = 0; 
    int tick_counter = 0;      

    while (1) {
        state_b1 = READ_PIN(BTN_1_PIN) ? 1 : 0;
        state_b2 = READ_PIN(BTN_2_PIN) ? 1 : 0;
        state_b3 = READ_PIN(BTN_3_PIN) ? 1 : 0;

        bool activity_detected = false;

        // Кнопка 1 (Довге натискання = Вимкнення)
        if (state_b1 == 0) {
            b1_press_duration++;
            if (b1_press_duration > 60) { // ~3 секунди
                pthread_mutex_lock(&state_mutex);
                app_state.shutdown_requested = true;
                pthread_mutex_unlock(&state_mutex);
                b1_press_duration = 0; 
            }
        } else {
            b1_press_duration = 0; 
        }

        // Кнопка 2 (Перемикання екранів)
        if (state_b2 == 0 && last_b2 == 1) {
            activity_detected = true;
            pthread_mutex_lock(&state_mutex);
            if (app_state.current_screen == SCREEN_STATUS) app_state.current_screen = SCREEN_QR;
            else if (app_state.current_screen == SCREEN_QR) app_state.current_screen = SCREEN_DINO;
            else app_state.current_screen = SCREEN_STATUS;
            pthread_mutex_unlock(&state_mutex);
        }

        // Кнопка 3 (Дія / Вимкнення сирени / Стрибок)
        if (state_b3 == 0 && last_b3 == 1) {
            activity_detected = true;
            pthread_mutex_lock(&state_mutex);
            
            if (app_state.find_device_alarm) {
                app_state.find_device_alarm = false; 
            } 
            else if (app_state.current_screen == SCREEN_DINO) {
                hardware_play_tone(1200, 20); // Звук стрибка
                app_state.dino_jump_triggered = true; // Підіймаємо прапорець стрибка
            }
            pthread_mutex_unlock(&state_mutex);
        }

        last_b2 = state_b2;
        last_b3 = state_b3;

        // Логіка таймера простою та сирени
        pthread_mutex_lock(&state_mutex);
        if (activity_detected) app_state.idle_timer_sec = 0;
        bool alarm_active = app_state.find_device_alarm;
        pthread_mutex_unlock(&state_mutex);

        if (alarm_active) {
            hardware_play_tone(2500, 200); // Сирена
            hardware_play_tone(0, 100);
            app_state.idle_timer_sec = 0; 
        }

        // Рахуємо секунди для скрінсейвера
        tick_counter++;
        if (tick_counter >= 20) {
            tick_counter = 0;
            pthread_mutex_lock(&state_mutex);
            app_state.idle_timer_sec++;
            pthread_mutex_unlock(&state_mutex);
        }

        usleep(50000); // Спимо 50мс (20 FPS для кнопок)
    }

    return NULL;
}