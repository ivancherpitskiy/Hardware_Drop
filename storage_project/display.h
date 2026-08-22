#pragma once
#include <stdbool.h>

bool display_init(void);
void display_clear(void);
void display_print(int page, int col, const char* str);

// Головний цикл відмальовки екрана
void *display_thread_loop(void *arg);