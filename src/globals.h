#pragma once

#include <stdbool.h>
#include <pthread.h>

// Перелічення доступних екранів
typedef enum {
    SCREEN_STATUS = 0,
    SCREEN_QR,
    SCREEN_DINO,
    SCREEN_UPLOAD,
    SCREEN_MATRIX
} ScreenMode;

// Спільна структура стану, до якої мають доступ різні потоки
typedef struct {
    bool is_uploading;           // Прапорець активного завантаження
    int free_space_percent;      // Відсоток заповненості пам'яті [0-100]
    ScreenMode current_screen;   // Поточний екран
    bool find_device_alarm;      // Прапорець тривоги (пошук пристрою)
    bool shutdown_requested;     // Запит на вимкнення системи
    int idle_timer_sec;          // Таймер простою для скрінсейвера
    bool dino_jump_triggered;    // Сигнал стрибка для динозавра
} AppStatus;

// Глобальні змінні для доступу з інших файлів
extern AppStatus app_state;
extern pthread_mutex_t state_mutex;