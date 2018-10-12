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
	//long width, height;
	//unsigned int* bmp_texture = read_bmp("../res/2.bmp", width, height);
	//if (bmp_texture)
	//{
	//	printf("init_texture succ\n");
	//}
	//device_set_texture(device, bmp_texture, width * 4, width, height);

	static IUINT32 texture[256][256];
	int i, j;
	for (j = 0; j < 256; j++) {
		for (i = 0; i < 256; i++) {
			int x = i / 32, y = j / 32;
			texture[j][i] = ((x + y) & 1)? 0xffffffff : 0x3fbcefff;
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

