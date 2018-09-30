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

//=====================================================================
// 绘制区域
//=====================================================================

// 画点
void device_pixel(device_t *device, int x, int y, IUINT32 color) {
	if (((IUINT32)x) < (IUINT32)device->width && ((IUINT32)y) < (IUINT32)device->height) {
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
// 着色器
//=====================================================================

IUINT32 Get_R(IUINT32 color)
{
	return (color >> 16) & 0x000000FF;
}

IUINT32 Get_G(IUINT32 color)
{
	return (color >> 8) & 0x000000FF;
}

IUINT32 Get_B(IUINT32 color)
{
	return color & 0x000000FF;
}

typedef void(*func_vertex_shader)(device_t* device, vertex_t* vertex, point_t* output);
typedef void(*func_pixel_shader)(device_t* device, vertex_t* vertex, IUINT32* color);

typedef struct {
	IUINT32 RenderState;
	func_vertex_shader p_vertex_shader;
	func_pixel_shader p_pixel_shader;
} RenderComponent;

// 顶点着色器
void shader_vertex_normal_mvp(device_t* device, vertex_t* vertex, point_t* output)
{
	transform_apply(&device->transform, output, &(vertex->pos));
}

void shader_vertex_phong_mvp(device_t* device, vertex_t* vertex, point_t* output)
{
	transform_apply(&device->transform, output, &(vertex->pos));

	point_t posInWorld;
	matrix_apply(&posInWorld, &(vertex->pos), &(device->transform.world));

	point_t eye = device->uniform[2];
	vector_sub(&vertex->eye_view, &eye, &posInWorld);
}

unsigned char default_alpha = 255;

// 片元着色器
void shader_pixel_normal_color(device_t* device, vertex_t* vertex, IUINT32* color)
{
	float w = 1.0f / vertex->rhw;

	float r = vertex->color.r * w;
	float g = vertex->color.g * w;
	float b = vertex->color.b * w;
	int R = (int)(r * 255.0f);
	int G = (int)(g * 255.0f);
	int B = (int)(b * 255.0f);
	R = CMID(R, 0, 255);
	G = CMID(G, 0, 255);
	B = CMID(B, 0, 255);

#ifdef USE_GDI_VIEW
	*(color) = (R << 16) | (G << 8) | (B);
#else
	*(color) = (R << 24) | (G << 16) | (B << 8) | (default_alpha);
#endif
}

void shader_pixel_normal_texture(device_t* device, vertex_t* vertex, IUINT32* color)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

#ifdef USE_GDI_VIEW
	*(color) = device_texture_read(device, u, v);
#else
	*(color) = (device_texture_read(device, u, v) << 8) | (default_alpha);
#endif
}

void shader_pixel_texture_lambert_light(device_t* device, vertex_t* vertex, IUINT32* color)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	vector_t normal = vertex->normal;
	vector_t direction = device->uniform[1];
	vector_normalize(&direction);
	vector_normalize(&normal);

	matrix_t normal_world;
	matrix_transpose(&(device->transform.worldInv), &normal_world);
	vector_t cnormal;
	matrix_apply(&cnormal, &normal, &(normal_world));

	IUINT32 cc = device_texture_read(device, u, v);
	float diffuse = vector_dotproduct(&direction, &cnormal);
	if (diffuse >= 0.001)
	{
		IUINT32 texture_R = Get_R(cc);
		IUINT32 texture_G = Get_G(cc);
		IUINT32 texture_B = Get_B(cc);

		vector_t energy = device->uniform[0];
		IUINT32 diffuse_R = (IUINT32)(texture_R * diffuse * energy.x);
		IUINT32 diffuse_G = (IUINT32)(texture_G * diffuse * energy.y);
		IUINT32 diffuse_B = (IUINT32)(texture_B * diffuse * energy.z);

		diffuse_R = CMID(diffuse_R, 0, 255);
		diffuse_G = CMID(diffuse_G, 0, 255);
		diffuse_B = CMID(diffuse_B, 0, 255);

#ifdef USE_GDI_VIEW
		*(color) = (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
		*(color) = (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
	}
	else
	{
		*(color) = 0;
	}
}

void shader_pixel_texture_phong_light(device_t* device, vertex_t* vertex, IUINT32* color)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	vector_t normal = vertex->normal;
	vector_t direction = device->uniform[1];
	vector_normalize(&direction);
	vector_normalize(&normal);

	matrix_t normal_world;
	matrix_transpose(&(device->transform.worldInv), &normal_world);
	vector_t cnormal;
	matrix_apply(&cnormal, &normal, &(normal_world));

	IUINT32 cc = device_texture_read(device, u, v);
	float diffuse = vector_dotproduct(&direction, &cnormal);
	if (diffuse >= 0.001)
	{
		IUINT32 texture_R = Get_R(cc);
		IUINT32 texture_G = Get_G(cc);
		IUINT32 texture_B = Get_B(cc);

		vector_t energy = device->uniform[0];
		IUINT32 diffuse_R = (IUINT32)(texture_R * diffuse * energy.x);
		IUINT32 diffuse_G = (IUINT32)(texture_G * diffuse * energy.y);
		IUINT32 diffuse_B = (IUINT32)(texture_B * diffuse * energy.z);

		vector_t vec_spec = cnormal;
		vector_scale(&vec_spec, 2 * diffuse);
		vector_sub(&vec_spec, &vec_spec, &direction);
		vector_normalize(&vec_spec);

		vector_t eye_view = vertex->eye_view;
		vector_normalize(&eye_view);

		vector_t matrial = device->uniform[3];
		float kD = matrial.x;
		float kS = matrial.y;
		float kQ = matrial.z;

		float spec = vector_dotproduct(&eye_view, &vec_spec);
		if (spec >= 0)
		{
			spec = pow(spec, kQ);
			IUINT32 spec_R = (IUINT32)(texture_R * spec * energy.x);
			IUINT32 spec_G = (IUINT32)(texture_G * spec * energy.y);
			IUINT32 spec_B = (IUINT32)(texture_B * spec * energy.z);

			diffuse_R = CMID(diffuse_R * kD + spec_R * kS, 0, 255);
			diffuse_G = CMID(diffuse_G * kD + spec_G * kS, 0, 255);
			diffuse_B = CMID(diffuse_B * kD + spec_B * kS, 0, 255);

#ifdef USE_GDI_VIEW
			*(color) = (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
			*(color) = (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
		}
		else
		{
			diffuse_R = CMID(diffuse_R * kD, 0, 255);
			diffuse_G = CMID(diffuse_G * kD, 0, 255);
			diffuse_B = CMID(diffuse_B * kD, 0, 255);

#ifdef USE_GDI_VIEW
			*(color) = (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
			*(color) = (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
		}
	}
	else
	{
		*(color) = 0;
	}
}

#define MAX_RENDER_STATE 5

RenderComponent g_RenderComponent[MAX_RENDER_STATE] = {
	{ RENDER_STATE_WIREFRAME, shader_vertex_normal_mvp, NULL },
	{ RENDER_STATE_TEXTURE, shader_vertex_normal_mvp, shader_pixel_normal_texture },
	{ RENDER_STATE_COLOR, shader_vertex_normal_mvp, shader_pixel_normal_color },
	{ RENDER_STATE_LAMBERT_LIGHT_TEXTURE, shader_vertex_normal_mvp, shader_pixel_texture_lambert_light },
	{ RENDER_STATE_PHONG_LIGHT_TEXTURE, shader_vertex_phong_mvp, shader_pixel_texture_phong_light },
};

func_pixel_shader get_pixel_shader(device_t* device)
{
	int i;
	for (i = 0; i < MAX_RENDER_STATE; i++)
	{
		if (device->render_state == g_RenderComponent[i].RenderState)
		{
			return g_RenderComponent[i].p_pixel_shader;
		}
	}

	return NULL;
}

func_vertex_shader get_vertex_shader(device_t* device)
{
	int i;
	for (i = 0; i < MAX_RENDER_STATE; i++)
	{
		if (device->render_state == g_RenderComponent[i].RenderState)
		{
			return g_RenderComponent[i].p_vertex_shader;
		}
	}

	return NULL;
}

//=====================================================================
// 渲染实现
//=====================================================================

// 绘制扫描线
void device_draw_scanline(device_t *device, scanline_t *scanline) {
	IUINT32 *framebuffer = device->framebuffer[scanline->y];
	float *zbuffer = device->zbuffer[scanline->y];
	int x = scanline->x;
	int w = scanline->w;
	int width = device->width;
	int render_state = device->render_state;
	int function_state = device->function_state;
	for (; w > 0; x++, w--) {
		if (x >= 0 && x < width) {
			float rhw = scanline->v.rhw;
			if (rhw >= zbuffer[x]) {	
				float w = 1.0f / rhw;
				zbuffer[x] = rhw;
				func_pixel_shader p_shader = get_pixel_shader(device);
				if (p_shader)
				{
					p_shader(device, &(scanline->v), &(framebuffer[x]));
				}
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
		if (j >= 0 && j < device->height) {
			trapezoid_edge_interp(trap, (float)j + 0.5f);
			trapezoid_init_scan_line(trap, &scanline, j);
			device_draw_scanline(device, &scanline);
		}
		if (j >= device->height) break;
	}
}

// 根据 render_state 绘制原始三角形
void device_draw_primitive(device_t *device, vertex_t *v1, 
	vertex_t *v2, vertex_t *v3) {
	point_t p1, p2, p3, c1, c2, c3;
	int render_state = device->render_state;

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
	if (transform_check_cvv(&c1) != 0) return;
	if (transform_check_cvv(&c2) != 0) return;
	if (transform_check_cvv(&c3) != 0) return;

	// 归一化
	transform_homogenize(&device->transform, &p1, &c1);
	transform_homogenize(&device->transform, &p2, &c2);
	transform_homogenize(&device->transform, &p3, &c3);

	// 纹理或者色彩绘制
	if (render_state != 0) {
		vertex_t t1 = *v1, t2 = *v2, t3 = *v3;
		trapezoid_t traps[2];
		int n;

		t1.pos = p1; 
		t2.pos = p2;
		t3.pos = p3;
		t1.pos.w = c1.w;
		t2.pos.w = c2.w;
		t3.pos.w = c3.w;
		
		vertex_rhw_init(&t1);	// 初始化 w
		vertex_rhw_init(&t2);	// 初始化 w
		vertex_rhw_init(&t3);	// 初始化 w
		
		// 拆分三角形为0-2个梯形，并且返回可用梯形数量
		n = trapezoid_init_triangle(traps, &t1, &t2, &t3);

		if (n >= 1) device_render_trap(device, &traps[0]);
		if (n >= 2) device_render_trap(device, &traps[1]);
	}
	
	if (render_state == RENDER_STATE_WIREFRAME) {		// 线框绘制
		device_draw_line(device, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, device->foreground);
		device_draw_line(device, (int)p1.x, (int)p1.y, (int)p3.x, (int)p3.y, device->foreground);
		device_draw_line(device, (int)p3.x, (int)p3.y, (int)p2.x, (int)p2.y, device->foreground);
	}
}

//=====================================================================
// 主程序
//=====================================================================
vertex_t mesh[24] = {
	{ {  1, -1,  1, 1 }, { 0, 0 }, { 1.0f, 0.2f, 0.2f },{ 0, 0,  1, 0 }, 1 },
	{ { -1, -1,  1, 1 }, { 0, 1 }, { 0.2f, 1.0f, 0.2f },{ 0, 0,  1, 0 }, 1 },
	{ { -1,  1,  1, 1 }, { 1, 1 }, { 0.2f, 0.2f, 1.0f },{ 0, 0,  1, 0 }, 1 },
	{ {  1,  1,  1, 1 }, { 1, 0 }, { 1.0f, 0.2f, 1.0f },{ 0, 0,  1, 0 }, 1 },

	{ {  1, -1, -1, 1 }, { 0, 0 }, { 1.0f, 1.0f, 0.2f },{ 0, 0, -1, 0 }, 1 },
	{ { 1,  1, -1, 1 },{ 0, 1 },{ 0.2f, 1.0f, 0.3f },{ 0, 0, -1, 0 }, 1 },
	{ { -1,  1, -1, 1 },{ 1, 1 },{ 1.0f, 0.3f, 0.3f },{ 0, 0, -1, 0 }, 1 },
	{ { -1, -1, -1, 1 }, { 1, 0 }, { 0.2f, 1.0f, 1.0f },{ 0, 0, -1, 0 }, 1 },

	{ { -1, -1,  1, 1 },{ 0, 0 },{ 0.2f, 1.0f, 0.2f },{ -1, 0,  0, 0 }, 1 },
	{ { -1, -1, -1, 1 },{ 0, 1 },{ 0.2f, 1.0f, 1.0f },{ -1, 0,  0, 0 }, 1 },
	{ { -1,  1, -1, 1 },{ 1, 1 },{ 1.0f, 0.3f, 0.3f },{ -1, 0,  0, 0 }, 1 },
	{ { -1,  1,  1, 1 },{ 1, 0 },{ 0.2f, 0.2f, 1.0f },{ -1, 0,  0, 0 }, 1 },

	{ { 1,  1,  1, 1 },{ 0, 0 },{ 1.0f, 0.2f, 1.0f },{ 0, 1,  0, 0 }, 1 },
	{ { -1,  1,  1, 1 },{ 0, 1 },{ 0.2f, 0.2f, 1.0f },{ 0, 1,  0, 0 }, 1 },
	{ { -1,  1, -1, 1 },{ 1, 1 },{ 1.0f, 0.3f, 0.3f },{ 0, 1, 0, 0 }, 1 },
	{ { 1,  1, -1, 1 },{ 1, 0 },{ 0.2f, 1.0f, 0.3f },{ 0, 1, 0, 0 }, 1 },

	{ { 1, -1,  1, 1 },{ 0, 0 },{ 1.0f, 0.2f, 0.2f },{ 0, -1,  0, 0 }, 1 },
	{ { 1, -1, -1, 1 },{ 0, 1 },{ 1.0f, 1.0f, 0.2f },{ 0, -1, 0, 0 }, 1 },
	{ { -1, -1, -1, 1 },{ 1, 1 },{ 0.2f, 1.0f, 1.0f },{ 0, -1, 0, 0 }, 1 },
	{ { -1, -1,  1, 1 },{ 1, 0 },{ 0.2f, 1.0f, 0.2f },{ 0, -1,  0, 0 }, 1 },

	{ { 1,  1,  1, 1 },{ 0, 0 },{ 1.0f, 0.2f, 1.0f },{ 1, 0,  0, 0 }, 1 },
	{ { 1,  1, -1, 1 },{ 0, 1 },{ 0.2f, 1.0f, 0.3f },{ 1, 0, 0, 0 }, 1 },
	{ { 1, -1, -1, 1 },{ 1, 1 },{ 1.0f, 1.0f, 0.2f },{ 1, 0, 0, 0 }, 1 },
	{ { 1, -1,  1, 1 },{ 1, 0 },{ 1.0f, 0.2f, 0.2f },{ 1, 0,  0, 0 }, 1 },
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
void draw_box(device_t *device, float theta) {
	matrix_t m;
	matrix_set_rotate(&m, -1, -0.5, 1, theta);
	device->transform.world = m;
	matrix_inverse(&m, &(device->transform.worldInv));
	transform_update(&device->transform);

	int index[36] = { 0,1,2, 2,3,0, 4,5,6, 6,7,4, 8,9,10, 10,11,8, 12,13,14, 14,15,12, 16,17,18, 18,19,16, 20,21,22, 22,23,20 };
	device_set_vertex_attrib_pointer(device, mesh);
	draw_elements(device, TRIANGLES, 12, index);
}

void camera_at_zero(device_t *device, float x, float y, float z) {
	point_t eye = { x, y, z, 1 }, at = { 0, 0, 0, 1 }, up = { 0, 0, 1, 1 };
	
	// upload matrix to GPU
	matrix_set_lookat(&device->transform.view, &eye, &at, &up);
	transform_update(&device->transform);
	
	//upload uniform
	if (device->render_state == RENDER_STATE_LAMBERT_LIGHT_TEXTURE)
	{
		vector_t energy = { 1.0,1.0,1.0,0.0 };
		device_set_uniform_value(device, 0, &energy); // 入射光强

		vector_t direction = { 1.0,1.0,1.0,0.0 }; // 入射光方向
		device_set_uniform_value(device, 1, &direction);
	}
	else if (device->render_state == RENDER_STATE_PHONG_LIGHT_TEXTURE)
	{
		vector_t energy = { 1.0,1.0,1.0,0.0 };
		device_set_uniform_value(device, 0, &energy); // 入射光强

		vector_t direction = { 1.0,1.0,1.0,0.0 }; // 入射光方向
		device_set_uniform_value(device, 1, &direction);

		device_set_uniform_value(device, 2, &eye); // 视点位置

		vector_t matrial = { 1.0,2.0,1.0,0.0 }; // 材质参数 散射系数 反射系数 粗糙程度
		device_set_uniform_value(device, 3, &matrial);
	}
}


void init_texture(device_t *device) {
	static IUINT32 texture[256][256];
	int i, j;
	for (j = 0; j < 256; j++) {
		for (i = 0; i < 256; i++) {
			int x = i / 32, y = j / 32;
			texture[j][i] = ((x + y) & 1)? 0xffffff : 0x3fbcef;
		}
	}
	device_set_texture(device, texture, 256 * 4, 256, 256);
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
	int states[] = { RENDER_STATE_WIREFRAME, RENDER_STATE_TEXTURE, RENDER_STATE_COLOR, RENDER_STATE_LAMBERT_LIGHT_TEXTURE, RENDER_STATE_PHONG_LIGHT_TEXTURE };
	int indicator = 0;
	int kbhit = 0;
	float alpha = 0;
	float pos = 5;
	int window_w = WINDOW_SIZE;
	int window_h = WINDOW_SIZE;

	memset(keys_state, 0, sizeof(int) * 512);

#ifdef USE_GDI_VIEW
	if (screen_init(window_w, window_h))
		return -1;
#else
	initGLView(window_w, window_h);
#endif

	device_t* device = get_device_inst();
	device_init(device, window_w, window_h, get_screen_fb());
	camera_at_zero(device, 0, 0, 0);
	init_texture(device);

	while (get_key_quit() == 0) {

#ifdef USE_GDI_VIEW
		screen_dispatch();
#endif		
		if (get_key_state(MOVE_NEAR)) pos -= 0.01f;
		if (get_key_state(MOVE_FAR)) pos += 0.01f;
		if (get_key_state(ROTATE_LEFT)) alpha += 0.01f;
		if (get_key_state(ROTATE_RIGHT)) alpha -= 0.01f;

		if (get_key_state(CHANGE_MODE)) {
			if (kbhit == 0) {
				kbhit = 1;
				if (++indicator >= MAX_RENDER_STATE) indicator = 0;
				device->render_state = states[indicator];

			}
		}	else {
			kbhit = 0;
		}

		device_clear(device, 1);
		camera_at_zero(device, pos, pos, pos);

		draw_box(device, alpha);

#ifdef USE_GDI_VIEW
		draw_screen_title(device);
		screen_update();
#else		
		updateFrameBufferData(device->framebuffer);
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

