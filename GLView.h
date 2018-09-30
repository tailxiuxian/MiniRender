#pragma once

#include "device.h"

void initGLView(int w, int h);
void destroyGLView();
void drawGLView();
void updateFrameBufferData(unsigned int** frameBuffer);
void drawGLTitle(device_t *device);