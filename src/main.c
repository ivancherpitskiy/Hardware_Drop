// Точка входу програми
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "globals.h"
#include "hardware.h"
#include "display.h"
#include "server.h"

// Глобальний стан
AppStatus app_state = {
    .is_uploading = false,
    .free_space_percent = 0,
    .current_screen = SCREEN_STATUS,
    .find_device_alarm = false,
    .shutdown_requested = false,
    .idle_timer_sec = 0
};

// М'ютекс для захисту змінних
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(void) {
    printf("[SYSTEM] Starting Hardware Drop...\n");

    // Ініціалізація заліза та екрана
    if (!hardware_init()) {
        fprintf(stderr, "[ERROR] Failed to init GPIO/mmap.\n");
        return EXIT_FAILURE;
    }
    
    if (!display_init()) {
        fprintf(stderr, "[ERROR] Failed to init OLED.\n");
        return EXIT_FAILURE;
    }

    printf("[SYSTEM] Hardware ready.\n");
    
    // Звук старту системи
    hardware_play_tone(2000, 100); 
    usleep(100000);
    hardware_play_tone(2000, 100);

    // Запуск потоків
    pthread_t hw_thread, net_thread, disp_thread;

    if (pthread_create(&hw_thread, NULL, hardware_thread_loop, NULL) != 0) {
        perror("[ERROR] HW thread failed");
        return EXIT_FAILURE;
    }

    if (pthread_create(&net_thread, NULL, server_thread_loop, NULL) != 0) {
        perror("[ERROR] NET thread failed");
        return EXIT_FAILURE;
    }

    if (pthread_create(&disp_thread, NULL, display_thread_loop, NULL) != 0) {
        perror("[ERROR] DISP thread failed");
        return EXIT_FAILURE;
    }

    // Головний цикл моніторингу (чекає на сигнал вимкнення)
    while (1) {
        pthread_mutex_lock(&state_mutex);
        bool needs_shutdown = app_state.shutdown_requested;
        pthread_mutex_unlock(&state_mutex);

        if (needs_shutdown) {
            printf("[SYSTEM] Shutdown requested via button. Halting...\n");
            
            // Виводимо повідомлення і гудимо
            display_clear();
            display_print(2, 20, "SHUTTING DOWN...");
            hardware_play_tone(1000, 500); 
            
            // Безпечне вимкнення ОС
            system("sudo poweroff");
            break;
        }

        sleep(1); // Перевіряємо раз на секунду
    }

    // Чекаємо завершення потоків (сюди дійде тільки при вимкненні)
    pthread_join(hw_thread, NULL);
    pthread_join(net_thread, NULL);
    pthread_join(disp_thread, NULL);

    return EXIT_SUCCESS;
}