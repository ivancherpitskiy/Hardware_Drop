#pragma once

#include <stdbool.h>

// Ініціалізує пам'ять для GPIO та налаштовує піни кнопок і зумера
bool hardware_init(void);

// Генерує ШІМ-сигнал на піні зумера (frequency - частота в Гц, duration_ms - тривалість)
void hardware_play_tone(int frequency, int duration_ms);

// Головний цикл апаратного потоку (читає кнопки, керує таймером простою)
void *hardware_thread_loop(void *arg);