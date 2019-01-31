//=====================================================================
// 
// mini3d.c - Mini Software Render All-In-One
//
// build:
//   mingw: gcc -O3 mini3d.c -o mini3d.exe -lgdi32
//   msvc:  cl -O2 -nologo mini3d.c 
//
// history:
//   2007.7.01  skywind  create this file as a tutorial
//   2007.7.02  skywind  implementate texture and color render
//   2008.3.15  skywind  fixed a trapezoid issue
//   2015.8.09  skywind  rewrite with more comment
//   2015.8.12  skywind  adjust interfaces for clearity 
// 
//=====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <windows.h>
#include <tchar.h>

#include "basetype.h"
#include "mathlib.h"
#include "transform.h"
#include "geometry.h"
#include "device.h"
#include "renderstate.h"
#include "GLView.h"
#include "GDIView.h"
#include "shader.h"
#include "bmpReader.h"
#include "blend.h"
#include "camera.h"

static int default_texture_id = 0;
static int texture_bmp1 = 0;
static int texture_bmp2 = 0;
static int texture_shadow = 0;
static int framebuffer_shadow = 0;

//=====================================================================
// 绘制区域
//=====================================================================

// 画点
void device_pixel(device_t *device, int x, int y, IUINT32 color) {
	if (((IUINT32)x) < (IUINT32)device->framebuffer_width && ((IUINT32)y) < (IUINT32)device->framebuffer_height) {
		device->framebuffer[y][x] = color;
	}
}

// 绘制线段
void device_draw_line(device_t *device, int x1, int y1, int x2, int y2, IUINT32 c) {
	int x, y, rem = 0;
	if (x1 == x2 && y1 == y2) {
		device_pixel(device, x1, y1, c);
	}	else if (x1 == x2) {
		int inc = (y1 <= y2)? 1 : -1;
		for (y = y1; y != y2; y += inc) device_pixel(device, x1, y, c);
		device_pixel(device, x2, y2, c);
	}	else if (y1 == y2) {
		int inc = (x1 <= x2)? 1 : -1;
		for (x = x1; x != x2; x += inc) device_pixel(device, x, y1, c);
		device_pixel(device, x2, y2, c);
	}	else {
		int dx = (x1 < x2)? x2 - x1 : x1 - x2;
		int dy = (y1 < y2)? y2 - y1 : y1 - y2;
		if (dx >= dy) {
			if (x2 < x1) x = x1, y = y1, x1 = x2, y1 = y2, x2 = x, y2 = y;
			for (x = x1, y = y1; x <= x2; x++) {
				device_pixel(device, x, y, c);
				rem += dy;
				if (rem >= dx) {
					rem -= dx;
					y += (y2 >= y1)? 1 : -1;
					device_pixel(device, x, y, c);
				}
			}
			device_pixel(device, x2, y2, c);
		}	else {
			if (y2 < y1) x = x1, y = y1, x1 = x2, y1 = y2, x2 = x, y2 = y;
			for (x = x1, y = y1; y <= y2; y++) {
				device_pixel(device, x, y, c);
				rem += dx;
				if (rem >= dy) {
					rem -= dy;
					x += (x2 >= x1)? 1 : -1;
					device_pixel(device, x, y, c);
				}
			}
			device_pixel(device, x2, y2, c);
		}
	}
}

//=====================================================================
// 渲染实现
//=====================================================================

// 绘制扫描线
void device_draw_scanline(device_t *device, scanline_t *scanline) {
	IUINT32 *framebuffer = device->framebuffer[scanline->y];
	float *zbuffer = device->zbuffer[scanline->y];
	if (device->bind_frame_buffer_idx >= 0 && device->bind_frame_buffer_idx < MAX_FRAME_BUFFER && device->framebuffer_array[device->bind_frame_buffer_idx].is_used)
	{
		framebuffer = device->framebuffer_array[device->bind_frame_buffer_idx].framebuffer[scanline->y];
		zbuffer = device->framebuffer_array[device->bind_frame_buffer_idx].zbuffer[scanline->y];
	}

	int x = scanline->x;
	int w = scanline->w;
	int width = device->framebuffer_width;
	int render_state = device->render_state;
	int function_state = device->function_state;
	for (; w > 0; x++, w--) {
		if (x >= 0 && x < width) {
			float rhw = scanline->v.rhw;
			if (rhw >= zbuffer[x]) {
#ifdef USE_GDI_VIEW
				func_pixel_shader p_shader = get_pixel_shader(device);
				if (p_shader)
				{
					framebuffer[x] = p_shader(device, &(scanline->v));
					zbuffer[x] = rhw;
				}
#else
				func_pixel_shader p_shader = get_pixel_shader(device);
				if (p_shader)
				{
					IUINT32 color = p_shader(device, &(scanline->v));
					if (is_opaque_pixel_color(color))
					{
						framebuffer[x] = color;
						zbuffer[x] = rhw;
					}
					else {
						framebuffer[x] = blend_frame_buffer_color(device, color, framebuffer[x]);
					}
				}
#endif
			}
		}
		vertex_add(&scanline->v, &scanline->step);
		if (x >= width) break;
	}
}

// 主渲染函数
void device_render_trap(device_t *device, trapezoid_t *trap) {
	scanline_t scanline;
	int j, top, bottom;
	top = (int)(trap->top + 0.5f);
	bottom = (int)(trap->bottom + 0.5f);
	for (j = top; j < bottom; j++) {
		if (j >= 0 && j < device->framebuffer_height) {
			trapezoid_edge_interp(trap, (float)j + 0.5f);
			trapezoid_init_scan_line(trap, &scanline, j);
			device_draw_scanline(device, &scanline);
		}
		if (j >= device->framebuffer_height) break;
	}
}

static void device_draw_triangles(device_t *device, 
	vector_t *k1, vector_t *k2, vector_t *k3,
	vertex_t *s1, vertex_t *s2, vertex_t *s3)
{
	point_t p1, p2, p3;
	// 归一化
	transform_homogenize(&device->transform, &p1, k1);
	transform_homogenize(&device->transform, &p2, k2);
	transform_homogenize(&device->transform, &p3, k3);

	// 纹理或者色彩绘制
	if (device->render_state != 0) {
		vertex_t t1 = *s1, t2 = *s2, t3 = *s3;
		trapezoid_t traps[2];
		int n;

		t1.pos = p1;
		t2.pos = p2;
		t3.pos = p3;
		t1.pos.w = k1->w;
		t2.pos.w = k2->w;
		t3.pos.w = k3->w;

		vertex_rhw_init(&t1);	// 初始化 w
		vertex_rhw_init(&t2);	// 初始化 w
		vertex_rhw_init(&t3);	// 初始化 w

								// 拆分三角形为0-2个梯形，并且返回可用梯形数量
		n = trapezoid_init_triangle(traps, &t1, &t2, &t3);

		if (n >= 1) device_render_trap(device, &traps[0]);
		if (n >= 2) device_render_trap(device, &traps[1]);
	}

	if (device->render_state == RENDER_STATE_WIREFRAME) {		// 线框绘制
		device_draw_line(device, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, device->foreground);
		device_draw_line(device, (int)p1.x, (int)p1.y, (int)p3.x, (int)p3.y, device->foreground);
		device_draw_line(device, (int)p3.x, (int)p3.y, (int)p2.x, (int)p2.y, device->foreground);
	}
}

// 根据 render_state 绘制原始三角形
void device_draw_primitive(device_t *device, vertex_t *v1, 
	vertex_t *v2, vertex_t *v3) {
	point_t c1, c2, c3;

	func_vertex_shader p_shader = get_vertex_shader(device);
	if (p_shader)
	{
		p_shader(device, v1, &c1);
		p_shader(device, v2, &c2);
		p_shader(device, v3, &c3);
	}

	// 背面剔除
	if (function_cull_back(device, &c1, &c2, &c3) != 0) return;

	// 裁剪，注意此处可以完善为具体判断几个点在 cvv内以及同cvv相交平面的坐标比例
	// 进行进一步精细裁剪，将一个分解为几个完全处在 cvv内的三角形
	int check1 = transform_check_cvv(&c1);
	int check2 = transform_check_cvv(&c2);
	int check3 = transform_check_cvv(&c3);

	int check_sum = 0;
	check_sum += (check1 & 1);
	check_sum += (check2 & 1);
	check_sum += (check3 & 1);

	if (check_sum == 3) // 全在外侧
	{
		return;
	}

	float ratio;
	vertex_t s1, s2, s3, s4;
	vector_t k1, k2, k3, k4;
	if (check_sum == 2)
	{
		if ((check1 & 1) == 0)
		{
			s1 = *v1;
			k1 = c1;

			calc_cvv_cut_vertex_ratio(&c1, &c2, &ratio);
			vertex_interp(&s2, v1, v2, ratio);
			vector_interp(&k2, &c1, &c2, ratio);

			calc_cvv_cut_vertex_ratio(&c1, &c3, &ratio);
			vertex_interp(&s3, v1, v3, ratio);
			vector_interp(&k3, &c1, &c3, ratio);
		}
		else if ((check2 & 1) == 0)
		{
			s2 = *v2;
			k2 = c2;

			calc_cvv_cut_vertex_ratio(&c2, &c3, &ratio);
			vertex_interp(&s3, v2, v3, ratio);
			vector_interp(&k3, &c2, &c3, ratio);

			calc_cvv_cut_vertex_ratio(&c2, &c1, &ratio);
			vertex_interp(&s1, v2, v1, ratio);
			vector_interp(&k1, &c2, &c1, ratio);
		}
		else if ((check3 & 1) == 0)
		{
			s3 = *v3;
			k3 = c3;

			calc_cvv_cut_vertex_ratio(&c3, &c2, &ratio);
			vertex_interp(&s2, v3, v2, ratio);
			vector_interp(&k2, &c3, &c2, ratio);

			calc_cvv_cut_vertex_ratio(&c3, &c1, &ratio);
			vertex_interp(&s1, v3, v1, ratio);
			vector_interp(&k1, &c3, &c1, ratio);
		}

		device_draw_triangles(device, &k1, &k2, &k3, &s1, &s2, &s3);
	}
	else if (check_sum == 1)
	{
		if ((check1 & 1) == 1)
		{
			calc_cvv_cut_vertex_ratio(&c1, &c2, &ratio);
			vertex_interp(&s1, v1, v2, ratio);
			vector_interp(&k1, &c1, &c2, ratio);

			s2 = *v2;
			k2 = c2;

			s3 = *v3;
			k3 = c3;

			calc_cvv_cut_vertex_ratio(&c1, &c3, &ratio);
			vertex_interp(&s4, v1, v3, ratio);
			vector_interp(&k4, &c1, &c3, ratio);

			device_draw_triangles(device, &k1, &k2, &k3, &s1, &s2, &s3);
			device_draw_triangles(device, &k1, &k3, &k4, &s1, &s3, &s4);
		}
		else if ((check2 & 1) == 1)
		{
			s1 = *v1;
			k1 = c1;

			calc_cvv_cut_vertex_ratio(&c2, &c1, &ratio);
			vertex_interp(&s2, v2, v1, ratio);
			vector_interp(&k2, &c2, &c1, ratio);

			calc_cvv_cut_vertex_ratio(&c2, &c3, &ratio);
			vertex_interp(&s3, v2, v3, ratio);
			vector_interp(&k3, &c2, &c3, ratio);

			s4 = *v3;
			k4 = c3;

			device_draw_triangles(device, &k1, &k2, &k3, &s1, &s2, &s3);
			device_draw_triangles(device, &k1, &k3, &k4, &s1, &s3, &s4);
		}
		else if ((check3 & 1) == 1)
		{
			s1 = *v1;
			k1 = c1;

			s2 = *v2;
			k2 = c2;

			calc_cvv_cut_vertex_ratio(&c3, &c2, &ratio);
			vertex_interp(&s3, v3, v2, ratio);
			vector_interp(&k3, &c3, &c2, ratio);

			calc_cvv_cut_vertex_ratio(&c3, &c1, &ratio);
			vertex_interp(&s4, v3, v1, ratio);
			vector_interp(&k4, &c3, &c1, ratio);

			device_draw_triangles(device, &k1, &k2, &k3, &s1, &s2, &s3);
			device_draw_triangles(device, &k1, &k3, &k4, &s1, &s3, &s4);
		}
	}
	else
	{
		s1 = *v1;
		k1 = c1;

		s2 = *v2;
		k2 = c2;

		s3 = *v3;
		k3 = c3;

		device_draw_triangles(device,&k1,&k2,&k3,&s1,&s2,&s3);
	}

}

//=====================================================================
// 主程序
//=====================================================================
vertex_t mesh[24] = {
	{ {  1, -1,  1, 1 }, { 0, 0 }, { 1.0f, 0.2f, 0.2f, 1.0f },{ 0, 0,  1, 0 }, 1 },
	{ { -1, -1,  1, 1 }, { 0, 1 }, { 0.2f, 1.0f, 0.2f, 1.0f },{ 0, 0,  1, 0 }, 1 },
	{ { -1,  1,  1, 1 }, { 1, 1 }, { 0.2f, 0.2f, 1.0f, 1.0f },{ 0, 0,  1, 0 }, 1 },
	{ {  1,  1,  1, 1 }, { 1, 0 }, { 1.0f, 0.2f, 1.0f, 1.0f },{ 0, 0,  1, 0 }, 1 },

	{ {  1, -1, -1, 1 }, { 0, 0 }, { 1.0f, 1.0f, 0.2f, 1.0f },{ 0, 0, -1, 0 }, 1 },
	{ { 1,  1, -1, 1 },{ 0, 1 },{ 0.2f, 1.0f, 0.3f, 1.0f },{ 0, 0, -1, 0 }, 1 },
	{ { -1,  1, -1, 1 },{ 1, 1 },{ 1.0f, 0.3f, 0.3f, 1.0f },{ 0, 0, -1, 0 }, 1 },
	{ { -1, -1, -1, 1 }, { 1, 0 }, { 0.2f, 1.0f, 1.0f, 1.0f },{ 0, 0, -1, 0 }, 1 },

	{ { -1, -1,  1, 1 },{ 0, 0 },{ 0.2f, 1.0f, 0.2f, 1.0f },{ -1, 0,  0, 0 }, 1 },
	{ { -1, -1, -1, 1 },{ 0, 1 },{ 0.2f, 1.0f, 1.0f, 1.0f },{ -1, 0,  0, 0 }, 1 },
	{ { -1,  1, -1, 1 },{ 1, 1 },{ 1.0f, 0.3f, 0.3f, 1.0f },{ -1, 0,  0, 0 }, 1 },
	{ { -1,  1,  1, 1 },{ 1, 0 },{ 0.2f, 0.2f, 1.0f, 1.0f },{ -1, 0,  0, 0 }, 1 },

	{ { 1,  1,  1, 1 },{ 0, 0 },{ 1.0f, 0.2f, 1.0f, 1.0f },{ 0, 1,  0, 0 }, 1 },
	{ { -1,  1,  1, 1 },{ 0, 1 },{ 0.2f, 0.2f, 1.0f, 1.0f },{ 0, 1,  0, 0 }, 1 },
	{ { -1,  1, -1, 1 },{ 1, 1 },{ 1.0f, 0.3f, 0.3f, 1.0f },{ 0, 1, 0, 0 }, 1 },
	{ { 1,  1, -1, 1 },{ 1, 0 },{ 0.2f, 1.0f, 0.3f, 1.0f },{ 0, 1, 0, 0 }, 1 },

	{ { 1, -1,  1, 1 },{ 0, 0 },{ 1.0f, 0.2f, 0.2f, 1.0f },{ 0, -1,  0, 0 }, 1 },
	{ { 1, -1, -1, 1 },{ 0, 1 },{ 1.0f, 1.0f, 0.2f, 1.0f },{ 0, -1, 0, 0 }, 1 },
	{ { -1, -1, -1, 1 },{ 1, 1 },{ 0.2f, 1.0f, 1.0f, 1.0f },{ 0, -1, 0, 0 }, 1 },
	{ { -1, -1,  1, 1 },{ 1, 0 },{ 0.2f, 1.0f, 0.2f, 1.0f },{ 0, -1,  0, 0 }, 1 },

	{ { 1,  1,  1, 1 },{ 0, 0 },{ 1.0f, 0.2f, 1.0f, 1.0f },{ 1, 0,  0, 0 }, 1 },
	{ { 1,  1, -1, 1 },{ 0, 1 },{ 0.2f, 1.0f, 0.3f, 1.0f },{ 1, 0, 0, 0 }, 1 },
	{ { 1, -1, -1, 1 },{ 1, 1 },{ 1.0f, 1.0f, 0.2f, 1.0f },{ 1, 0, 0, 0 }, 1 },
	{ { 1, -1,  1, 1 },{ 1, 0 },{ 1.0f, 0.2f, 0.2f, 1.0f },{ 1, 0,  0, 0 }, 1 },
};

vertex_t mesh_panel[4] = {
	{ { 4,	-5, -4, 1 },{ 0, 0 },{ 1.0f, 0.2f, 0.2f, 1.0f },{ 0, 1,  0, 0 }, 1 },
	{ { 4,  -5, 4, 1 },{ 1, 0 },{ 1.0f, 0.2f, 1.0f, 1.0f },{ 0, 1,  0, 0 }, 1 },
	{ { -4, -5, 4, 1 },{ 1, 1 },{ 0.2f, 0.2f, 1.0f, 1.0f },{ 0, 1,  0, 0 }, 1 },
	{ { -4, -5, -4, 1 },{ 0, 1 },{ 0.2f, 1.0f, 0.2f, 1.0f },{ 0, 1,  0, 0 }, 1 },
};

#define TRIANGLES 1

// 简单期间 索引全部用int
void draw_elements(device_t* device,IUINT8 uElementType, IUINT32 uElementCount, int* index)
{	
	if (TRIANGLES == uElementType)
	{
		if (device->vertex_array == NULL)
		{
			return;
		}

		IUINT32 i;
		for (i = 0; i < uElementCount; i++)
		{
			vertex_t p1 = device->vertex_array[index[i * 3]];
			vertex_t p2 = device->vertex_array[index[i * 3 + 1]];
			vertex_t p3 = device->vertex_array[index[i * 3 + 2]];
			device_draw_primitive(device, &p1, &p2, &p3);
		}
	}
}

void draw_plane(device_t *device, int a, int b, int c, int d) {
	vertex_t p1 = mesh[a], p2 = mesh[b], p3 = mesh[c], p4 = mesh[d];
	device_draw_primitive(device, &p1, &p2, &p3);
	device_draw_primitive(device, &p3, &p4, &p1);
}

// application stage
void draw_backggroud(device_t *device)
{
	matrix_set_identity(&(device->transform.world));
	matrix_set_identity(&(device->transform.worldInv));
	transform_update(&device->transform);

	int index_panel[6] = { 0,1,2, 2,3,0 };
	device_set_vertex_attrib_pointer(device, mesh_panel);
	draw_elements(device, TRIANGLES, 2, index_panel);
}

void draw_box(device_t *device, float theta, float box_x, float box_y, float box_z) {
	matrix_t rotate;
	matrix_set_rotate(&rotate, -1, -0.5, 1, theta);

	matrix_t translate;
	matrix_set_translate(&translate, box_x, box_y, box_z);

	matrix_t scale;
	matrix_set_scale(&scale, 1.0f, 1.0f, 1.0f);

	matrix_t model;
	matrix_mul(&model, &scale, &rotate);
	matrix_mul(&(device->transform.world), &model, &translate);

	matrix_inverse(&(device->transform.world), &(device->transform.worldInv));
	transform_update(&device->transform);

	int index[36] = { 0,1,2, 2,3,0, 4,5,6, 6,7,4, 8,9,10, 10,11,8, 12,13,14, 14,15,12, 16,17,18, 18,19,16, 20,21,22, 22,23,20 };
	device_set_vertex_attrib_pointer(device, mesh);
	draw_elements(device, TRIANGLES, 12, index);
}

// 渲染相关组件
static CCamera* g_mainCamera = NULL;

// 光源
static vector_t normal_light_energy = { 1.0,1.0,1.0,0.0 };// 阴影入射光强
static vector_t normal_light_direction = { 1.0,1.0,1.0,0.0 }; // 阴影入射光方向

static vector_t shadow_light_energy = { 1.0,1.0,1.0,0.0 };// 阴影入射光强
static vector_t shadow_light_direction = { 0.0,5.0,0.0,0.0 }; // 阴影入射光方向
static matrix_t shadow_light_transform_box;
static matrix_t shadow_light_transform_panel;
static IUINT32 **shadow_texture = NULL;
static IUINT32 *shadow_texture_buffer = NULL;

void setup_shader(device_t *device)
{	
	if (device->render_state == RENDER_STATE_WIREFRAME)
	{
		device_set_shader_state(device, SHADER_STATE_WIREFRAME);
	}
	else if (device->render_state == RENDER_STATE_TEXTURE)
	{
		device_set_shader_state(device, SHADER_STATE_TEXTURE);
	}
	else if (device->render_state == RENDER_STATE_COLOR)
	{
		device_set_shader_state(device, SHADER_STATE_COLOR);
	}
	else if (device->render_state == RENDER_STATE_LAMBERT_LIGHT_TEXTURE)
	{
		device_set_shader_state(device,SHADER_STATE_LAMBERT_LIGHT_TEXTURE);
	}
	else if (device->render_state == RENDER_STATE_PHONG_LIGHT_TEXTURE)
	{
		device_set_shader_state(device, SHADER_STATE_PHONG_LIGHT_TEXTURE);
	}
	else if (device->render_state == RENDER_STATE_TEXTURE_ALPHA)
	{
		device_set_shader_state(device, SHADER_STATE_TEXTURE_ALPHA);
	}
	else if (device->render_state == RENDER_STATE_SHADOW_MAP)
	{
		device_set_shader_state(device, SHADER_STATE_SHADOW_MAP);
	}
	else if (device->render_state == RENDER_STATE_BLINN_LIGHT_TEXTURE)
	{
		device_set_shader_state(device, SHADER_STATE_BLINN_LIGHT_TEXTURE);
	}
}

void setup_shader_parma(device_t *device, float x, float y, float z)
{
	point_t eye = { x, y, z, 1 };
	
	if (device->shader_state == SHADER_STATE_WIREFRAME)
	{

	}
	else if (device->shader_state == SHADER_STATE_TEXTURE)
	{
		device_bind_texture(device, 0, default_texture_id);
	}
	else if (device->shader_state == SHADER_STATE_COLOR)
	{

	}
	else if (device->shader_state == SHADER_STATE_LAMBERT_LIGHT_TEXTURE)
	{
		device_set_uniform_vector_value(device, 0, &normal_light_energy);

		device_set_uniform_vector_value(device, 1, &normal_light_direction);

		device_bind_texture(device, 0, default_texture_id);
	}
	else if (device->shader_state == SHADER_STATE_PHONG_LIGHT_TEXTURE)
	{
		device_set_uniform_vector_value(device, 0, &normal_light_energy); // 入射光强

		device_set_uniform_vector_value(device, 1, &normal_light_direction);

		device_set_uniform_vector_value(device, 2, &eye); // 视点位置

		vector_t matrial = { 1.0,2.0,1.0,0.0 }; // 材质参数 散射系数 反射系数 粗糙程度
		device_set_uniform_vector_value(device, 3, &matrial);

		device_bind_texture(device, 0, default_texture_id);
	}
	else if (device->shader_state == SHADER_STATE_TEXTURE_ALPHA)
	{
		device_bind_texture(device, 0, default_texture_id);
		blendstate_t blendstate;
		blendstate.srcState = BLEND_SRC_ALPHA;
		blendstate.srcState = BLEND_ONE_MINUS_SRC_ALPHA;
		device_set_blend_state(device, blendstate);
	}
	else if (device->shader_state == SHADER_STATE_SHADOW_MAP)
	{

	}
	else if (device->shader_state == SHADER_STATE_LIGHT_SHADOW)
	{
		device_set_uniform_vector_value(device, 0, &shadow_light_energy);

		device_set_uniform_vector_value(device, 1, &shadow_light_direction);

		device_bind_texture(device, 0, default_texture_id);

		device_bind_texture(device, 1, texture_shadow);
	}
	else if (device->shader_state == SHADER_STATE_BLINN_LIGHT_TEXTURE)
	{
		device_set_uniform_vector_value(device, 0, &normal_light_energy); // 入射光强

		device_set_uniform_vector_value(device, 1, &normal_light_direction);

		device_set_uniform_vector_value(device, 2, &eye); // 视点位置

		vector_t matrial = { 1.0,2.0,1.0,0.0 }; // 材质参数 散射系数 反射系数 粗糙程度
		device_set_uniform_vector_value(device, 3, &matrial);

		device_bind_texture(device, 0, default_texture_id);
	}
}

void init_texture(device_t *device) {
	static IUINT32 texture[256][256];
	int i, j;
	for (j = 0; j < 256; j++) {
		for (i = 0; i < 256; i++) {
			int x = i / 32, y = j / 32;
			texture[j][i] = ((x + y) & 1) ? 0xffffffff : 0x3fbcefff;
		}
	}
	default_texture_id = device_gen_texture(device);
	device_set_texture(device, texture, 256 * 4, 256, 256, default_texture_id);

	long width, height;
	unsigned int* bmp_texture = read_bmp("../res/1.bmp", width, height);
	if (bmp_texture)
	{
		texture_bmp1 = device_gen_texture(device);
		device_set_texture(device, bmp_texture, width * 4, width, height, texture_bmp1);
	}

	bmp_texture = read_bmp("../res/2.bmp", width, height);
	if (bmp_texture)
	{
		texture_bmp2 = device_gen_texture(device);
		device_set_texture(device, bmp_texture, width * 4, width, height, texture_bmp2);
	}
}

static int keys_state[512];	// 当前键盘按下状态
static int key_quit = 0;

int get_key_state(int keyCode)
{
	if (keyCode >= 0 && keyCode < 512)
	{
		return keys_state[keyCode];
	}
	return 0;
}

void set_key_state(int keyCode, int value)
{
	if (keyCode >= 0 && keyCode < 512)
	{
		keys_state[keyCode] = value;
	}
}

int get_key_quit()
{
	return key_quit;
}

void set_key_quit()
{
	key_quit = 1;
}

int main(void)
{
	int states[] = { RENDER_STATE_WIREFRAME, RENDER_STATE_TEXTURE, RENDER_STATE_COLOR, RENDER_STATE_LAMBERT_LIGHT_TEXTURE, RENDER_STATE_PHONG_LIGHT_TEXTURE, RENDER_STATE_TEXTURE_ALPHA, RENDER_STATE_SHADOW_MAP, RENDER_STATE_BLINN_LIGHT_TEXTURE };
	int indicator = 0;
	int kbhit = 0;
	float alpha = -2.40f;
	float pos = 5;
	int window_w = WINDOW_SIZE;
	int window_h = WINDOW_SIZE;

	float box_x = 0.0f;
	float box_y = 0.0f;
	float box_z = 0.0f;

	memset(keys_state, 0, sizeof(int) * 512);

#ifdef USE_GDI_VIEW
	if (screen_init(window_w, window_h))
		return -1;
#else
	initGLView(window_w, window_h);
#endif

	device_t* device = get_device_inst();
	device_init(device, window_w, window_h, get_screen_fb());
	
	if (g_mainCamera == NULL)
	{
		vector_t eye = { 0, 0, 0, 1 }, at = { 0, 0, 0, 1 }, up = { 0, 0, 1, 1 };
		g_mainCamera = new CCamera(eye, at, up);
		g_mainCamera->makeup_view_matrix(&device->transform.view);
		transform_update(&device->transform);
	}
	
	setup_shader(device);
	setup_shader_parma(device, 0, 0, 0);
	init_texture(device);

	while (get_key_quit() == 0) {

#ifdef USE_GDI_VIEW
		screen_dispatch();
#endif		
		if (get_key_state(MOVE_NEAR)) pos -= 0.01f;
		if (get_key_state(MOVE_FAR)) pos += 0.01f;
		if (get_key_state(ROTATE_LEFT)) alpha += 0.01f;
		if (get_key_state(ROTATE_RIGHT)) alpha -= 0.01f;

		vector_t eye = { pos, pos, pos, 1 };
		g_mainCamera->set_eye(eye);

		if (get_key_state(CHANGE_MODE)) {
			if (kbhit == 0) {
				kbhit = 1;
				if (++indicator >= MAX_RENDER_STATE) indicator = 0;
				device->render_state = states[indicator];

			}
		}	else {
			kbhit = 0;
		}

		// 绘制shadowmap
		if (device->render_state == RENDER_STATE_SHADOW_MAP)
		{
			if (framebuffer_shadow == 0)
			{
				framebuffer_shadow = device_gen_frame_buffer(device);
			}

			device_bind_framebuffer(device, framebuffer_shadow);

			device->background = 0;
			device_clear_framebuffer(device, framebuffer_shadow, 0);			
			
			vector_t eye = { shadow_light_direction.x, shadow_light_direction.y, shadow_light_direction.z, 1 }, at = { 0, 0, 0, 1 }, up = { 0, 0, 1, 1 };
			CCamera::makeup_view_matrix(&(device->transform.view), eye, at, up);

			setup_shader(device);
			setup_shader_parma(device, shadow_light_direction.x, shadow_light_direction.y, shadow_light_direction.z);
			draw_backggroud(device);
			shadow_light_transform_panel = device->transform.transform; // 获取平面的光源变换矩阵
			draw_box(device, alpha, box_x, box_y, box_z);
			shadow_light_transform_box = device->transform.transform; // 获取BOX的光源变换矩阵

			if (NULL == shadow_texture)
			{
				shadow_texture = new IUINT32*[MAX_FRAME_BUFFER_HEIGHT];
				shadow_texture_buffer = new IUINT32[MAX_FRAME_BUFFER_HEIGHT * MAX_FRAME_BUFFER_WIDTH];
				for (int i = 0; i < MAX_FRAME_BUFFER_HEIGHT; i++)
				{
					shadow_texture[i] = shadow_texture_buffer + MAX_FRAME_BUFFER_WIDTH * i;
				}
			}

			device_copy_framebuffer(device, framebuffer_shadow, shadow_texture); // 获取shadowmap
			device_unbind_framebuffer(device, framebuffer_shadow);

			if (texture_shadow == 0)
			{
				texture_shadow = device_gen_texture(device);
			}
			device_set_texture(device, shadow_texture_buffer, MAX_FRAME_BUFFER_WIDTH * 4, device->framebuffer_width, device->framebuffer_height, texture_shadow);
		}

		device_clear(device, 1);
		
		g_mainCamera->makeup_view_matrix(&device->transform.view);
		transform_update(&device->transform);
		
		setup_shader(device);
		if (device->render_state == RENDER_STATE_SHADOW_MAP)
		{
			// 设置新的shader
			device_set_shader_state(device, SHADER_STATE_LIGHT_SHADOW);
		}
		setup_shader_parma(device, pos, pos, pos);
		if (device->render_state == RENDER_STATE_SHADOW_MAP)
		{
			device_set_uniform_matrix_value(device, 0, &shadow_light_transform_panel);
			draw_backggroud(device);
			device_set_uniform_matrix_value(device, 0, &shadow_light_transform_box);
		}
		draw_box(device, alpha, box_x, box_y, box_z);

#ifdef USE_GDI_VIEW
		draw_screen_title(device);
		screen_update();
#else		
		updateFrameBufferData(device);
		drawGLTitle(device);
		drawGLView();
#endif

		Sleep(1);
	}

#ifdef USE_GDI_VIEW

#else
	destroyGLView();
#endif

	return 0;
}

