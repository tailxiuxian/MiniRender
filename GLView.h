#pragma once

#include "device.h"

void initGLView(int w, int h);
void destroyGLView();
void drawGLView();
void updateFrameBufferData(device_t* device);
void drawGLTitle(device_t *device);