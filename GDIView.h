#pragma once

#include "device.h"

int screen_init(int w, int h);
int screen_close(void);	
void screen_dispatch(void);
void screen_update(void);
LRESULT screen_events(HWND, UINT, WPARAM, LPARAM);
unsigned char* get_screen_fb();
int get_screen_exit();
void set_screen_title(TCHAR* title);
void draw_screen_title(device_t *device);