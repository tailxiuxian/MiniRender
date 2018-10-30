#pragma once

#include "device.h"

typedef void(*func_vertex_shader)(device_t* device, vertex_t* vertex, point_t* output);
typedef IUINT32 (*func_pixel_shader)(device_t* device, vertex_t* vertex);

func_pixel_shader get_pixel_shader(device_t* device);
func_vertex_shader get_vertex_shader(device_t* device);