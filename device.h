#pragma once

#include "basetype.h"
#include "mathlib.h"
#include "transform.h"
#include "geometry.h"

//=====================================================================
// 渲染设备
//=====================================================================

typedef struct {
	color_t energy;
	vector_t direction; //光向量
} para_light_source_t;

typedef struct {
	transform_t transform;      // 坐标变换器
	int width;                  // 窗口宽度
	int height;                 // 窗口高度
	IUINT32 **framebuffer;      // 像素缓存：framebuffer[y] 代表第 y行
	float **zbuffer;            // 深度缓存：zbuffer[y] 为第 y行指针
	IUINT32 **texture;          // 纹理：同样是每行索引
	int tex_width;              // 纹理宽度
	int tex_height;             // 纹理高度
	float max_u;                // 纹理最大宽度：tex_width - 1
	float max_v;                // 纹理最大高度：tex_height - 1
	int render_state;           // 渲染状态
	IUINT32 background;         // 背景颜色
	IUINT32 foreground;         // 线框颜色
	int function_state;			// 功能状态

	// Attribute
	vertex_t* vertex_array; // 应该申请一份显存 实现内存到显存的copy 用index管理顶点存储

	// Uniform
	para_light_source_t para_light;
	point_t eye;
}	device_t;

#define FUNC_STATE_CULL_BACK		1		// 背部剔除

void device_init(device_t *device, int width, int height, void *fb); //初始化渲染设备
void device_destroy(device_t *device); // 删除设备		   
void device_set_texture(device_t *device, void *bits, long pitch, int w, int h);// 设置当前纹理
void device_clear(device_t *device, int mode); // 清空 framebuffer 和 zbuffer						   
IUINT32 device_texture_read(const device_t *device, float u, float v); // 根据坐标读取纹理
void device_set_vertex_attrib_pointer(device_t* device, vertex_t* vertex_array); // 设置顶点数据

int function_cull_back(device_t* device, point_t* p1, point_t* p2, point_t* p3); // 背部剔除

device_t* get_device_inst();