#pragma once

#include "basetype.h"
#include "device.h"

bool is_opaque_pixel_color(IUINT32 color);
IUINT32 blend_frame_buffer_color(device_t *device, IUINT32 srcColor, IUINT32 dstColor);

#define BLEND_ZERO 1
#define BLEND_ONE 2
#define BLEND_SRC_ALPHA 3
#define BLEND_ONE_MINUS_SRC_ALPHA 4
