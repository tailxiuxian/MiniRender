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
#include <assert.h>

#include <windows.h>
#include <tchar.h>

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
	vertex_t* vertex_array;

	// Uniform
	para_light_source_t para_light;
	point_t eye;
}	device_t;

static device_t* g_pRenderDevice;

#define RENDER_STATE_WIREFRAME      1		// 渲染线框
#define RENDER_STATE_TEXTURE        2		// 渲染纹理
#define RENDER_STATE_COLOR          4		// 渲染颜色
#define RENDER_STATE_LAMBERT_LIGHT_TEXTURE 8 // 兰伯特光照

#define FUNC_STATE_CULL_BACK		1		// 背部剔除
#define FUNC_STATE_PARA_LIGHT		2		// 平行光照


// 设备初始化，fb为外部帧缓存，非 NULL 将引用外部帧缓存（每行 4字节对齐）
void device_init(device_t *device, int width, int height, void *fb) {
	int need = sizeof(void*) * (height * 2 + 1024) + width * height * 8;
	char *ptr = (char*)malloc(need + 64);
	char *framebuf, *zbuf;
	int j;
	assert(ptr);
	device->framebuffer = (IUINT32**)ptr;
	device->zbuffer = (float**)(ptr + sizeof(void*) * height);
	ptr += sizeof(void*) * height * 2;
	device->texture = (IUINT32**)ptr;
	ptr += sizeof(void*) * 1024;
	framebuf = (char*)ptr;
	zbuf = (char*)ptr + width * height * 4;
	ptr += width * height * 8;
	if (fb != NULL) framebuf = (char*)fb;
	for (j = 0; j < height; j++) {
		device->framebuffer[j] = (IUINT32*)(framebuf + width * 4 * j);
		device->zbuffer[j] = (float*)(zbuf + width * 4 * j);
	}
	device->texture[0] = (IUINT32*)ptr;
	device->texture[1] = (IUINT32*)(ptr + 16);
	memset(device->texture[0], 0, 64);
	device->tex_width = 2;
	device->tex_height = 2;
	device->max_u = 1.0f;
	device->max_v = 1.0f;
	device->width = width;
	device->height = height;
	device->background = 0xc0c0c0;
	device->foreground = 0;
	transform_init(&device->transform, width, height);
	device->render_state = RENDER_STATE_WIREFRAME;
	device->function_state = 0;
}

// 删除设备
void device_destroy(device_t *device) {
	if (device->framebuffer) 
		free(device->framebuffer);
	device->framebuffer = NULL;
	device->zbuffer = NULL;
	device->texture = NULL;
}

// 设置当前纹理
void device_set_texture(device_t *device, void *bits, long pitch, int w, int h) {
	char *ptr = (char*)bits;
	int j;
	assert(w <= 1024 && h <= 1024);
	for (j = 0; j < h; ptr += pitch, j++) 	// 重新计算每行纹理的指针
		device->texture[j] = (IUINT32*)ptr;
	device->tex_width = w;
	device->tex_height = h;
	device->max_u = (float)(w - 1);
	device->max_v = (float)(h - 1);
}

// 清空 framebuffer 和 zbuffer
void device_clear(device_t *device, int mode) {
	int y, x, height = device->height;
	for (y = 0; y < device->height; y++) {
		IUINT32 *dst = device->framebuffer[y];
		IUINT32 cc = (height - 1 - y) * 230 / (height - 1);
		cc = (cc << 16) | (cc << 8) | cc;
		if (mode == 0) cc = device->background;
		for (x = device->width; x > 0; dst++, x--) dst[0] = cc;
	}
	for (y = 0; y < device->height; y++) {
		float *dst = device->zbuffer[y];
		for (x = device->width; x > 0; dst++, x--) dst[0] = 0.0f;
	}
}

//=====================================================================
// 功能区域
//=====================================================================
int function_cull_back(device_t* device, point_t* p1, point_t* p2, point_t* p3)
{
	if (device->function_state & FUNC_STATE_CULL_BACK)
	{
		vector_t dirPrimitive, vec1, vec2;

		vector_sub(&vec1, p2, p1);
		vector_sub(&vec2, p3, p2);
		vector_crossproduct(&dirPrimitive, &vec1, &vec2);

		static vector_t dirView = { 0,0,-1,1 };
		if (vector_dotproduct(&dirPrimitive, &dirView) > 0)
		{
			return 1;
		}
	}

	return 0;
}

void function_default_para_light(device_t* device)
{
	device->para_light.energy.r = 1.0;
	device->para_light.energy.g = 1.0;
	device->para_light.energy.b = 1.0;
	device->para_light.direction.x = 1.0;
	device->para_light.direction.y = 2.0;
	device->para_light.direction.z = 3.0;
	device->para_light.direction.w = 0.0;
}

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

// 根据坐标读取纹理
IUINT32 device_texture_read(const device_t *device, float u, float v) {
	int x, y;
	u = u * device->max_u;
	v = v * device->max_v;
	x = (int)(u + 0.5f);
	y = (int)(v + 0.5f);
	x = CMID(x, 0, device->tex_width - 1);
	y = CMID(y, 0, device->tex_height - 1);
	return device->texture[y][x];
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
	*(color) = (R << 16) | (G << 8) | (B);
}

void shader_pixel_normal_texture(device_t* device, vertex_t* vertex, IUINT32* color)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	*(color) = device_texture_read(device, u, v);
}

void shader_pixel_texture_lambert_light(device_t* device, vertex_t* vertex, IUINT32* color)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	vector_t normal = vertex->normal;
	para_light_source_t* light = &(device->para_light);
	vector_normalize(&(light->direction));
	vector_normalize(&normal);

	matrix_t normal_world;
	matrix_transpose(&(device->transform.worldInv), &normal_world);
	vector_t cnormal;
	matrix_apply(&cnormal, &normal, &(normal_world));

	IUINT32 cc = device_texture_read(device, u, v);
	float diffuse = vector_dotproduct(&(light->direction), &cnormal);
	if (diffuse >= 0)
	{
		int texture_R = Get_R(cc);
		int texture_G = Get_G(cc);
		int texture_B = Get_B(cc);

		int diffuse_R = texture_R * diffuse * light->energy.r;
		int diffuse_G = texture_G * diffuse * light->energy.g;
		int diffuse_B = texture_B * diffuse * light->energy.b;

		*(color) = (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
	}
	else
	{
		*(color) = 0;
	}
}

#define MAX_RENDER_STATE 4

RenderComponent g_RenderComponent[MAX_RENDER_STATE] = {
	{ RENDER_STATE_WIREFRAME, shader_vertex_normal_mvp, NULL },
	{ RENDER_STATE_TEXTURE, shader_vertex_normal_mvp, shader_pixel_normal_texture },
	{ RENDER_STATE_COLOR, shader_vertex_normal_mvp, shader_pixel_normal_color },
	{ RENDER_STATE_LAMBERT_LIGHT_TEXTURE, shader_vertex_normal_mvp, shader_pixel_texture_lambert_light },
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
void device_draw_primitive(device_t *device, const vertex_t *v1, 
	const vertex_t *v2, const vertex_t *v3) {
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
// Win32 窗口及图形绘制：为 device 提供一个 DibSection 的 FB
//=====================================================================
int screen_w, screen_h, screen_exit = 0;
int screen_mx = 0, screen_my = 0, screen_mb = 0;
int screen_keys[512];	// 当前键盘按下状态
static HWND screen_handle = NULL;		// 主窗口 HWND
static HDC screen_dc = NULL;			// 配套的 HDC
static HBITMAP screen_hb = NULL;		// DIB
static HBITMAP screen_ob = NULL;		// 老的 BITMAP
unsigned char *screen_fb = NULL;		// frame buffer
long screen_pitch = 0;

int screen_init(int w, int h, const TCHAR *title);	// 屏幕初始化
int screen_close(void);								// 关闭屏幕
void screen_dispatch(void);							// 处理消息
void screen_update(void);							// 显示 FrameBuffer

// win32 event handler
static LRESULT screen_events(HWND, UINT, WPARAM, LPARAM);	

#ifdef _MSC_VER
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

// 初始化窗口并设置标题
int screen_init(int w, int h, const TCHAR *title) {
	WNDCLASS wc = { CS_BYTEALIGNCLIENT, (WNDPROC)screen_events, 0, 0, 0, 
		NULL, NULL, NULL, NULL, _T("SCREEN3.1415926") };
	BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB, 
		w * h * 4, 0, 0, 0, 0 }  };
	RECT rect = { 0, 0, w, h };
	int wx, wy, sx, sy;
	LPVOID ptr;
	HDC hDC;

	screen_close();

	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (!RegisterClass(&wc)) return -1;

	screen_handle = CreateWindow(_T("SCREEN3.1415926"), title,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);
	if (screen_handle == NULL) return -2;

	screen_exit = 0;
	hDC = GetDC(screen_handle);
	screen_dc = CreateCompatibleDC(hDC);
	ReleaseDC(screen_handle, hDC);

	screen_hb = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS, &ptr, 0, 0);
	if (screen_hb == NULL) return -3;

	screen_ob = (HBITMAP)SelectObject(screen_dc, screen_hb);
	screen_fb = (unsigned char*)ptr;
	screen_w = w;
	screen_h = h;
	screen_pitch = w * 4;
	
	AdjustWindowRect(&rect, GetWindowLong(screen_handle, GWL_STYLE), 0);
	wx = rect.right - rect.left;
	wy = rect.bottom - rect.top;
	sx = (GetSystemMetrics(SM_CXSCREEN) - wx) / 2;
	sy = (GetSystemMetrics(SM_CYSCREEN) - wy) / 2;
	if (sy < 0) sy = 0;
	SetWindowPos(screen_handle, NULL, sx, sy, wx, wy, (SWP_NOCOPYBITS | SWP_NOZORDER | SWP_SHOWWINDOW));
	SetForegroundWindow(screen_handle);

	ShowWindow(screen_handle, SW_NORMAL);
	screen_dispatch();

	memset(screen_keys, 0, sizeof(int) * 512);
	memset(screen_fb, 0, w * h * 4);

	return 0;
}

int screen_close(void) {
	if (screen_dc) {
		if (screen_ob) { 
			SelectObject(screen_dc, screen_ob); 
			screen_ob = NULL; 
		}
		DeleteDC(screen_dc);
		screen_dc = NULL;
	}
	if (screen_hb) { 
		DeleteObject(screen_hb); 
		screen_hb = NULL; 
	}
	if (screen_handle) { 
		CloseWindow(screen_handle); 
		screen_handle = NULL; 
	}
	return 0;
}

static void dispatch_key_event(WPARAM wKey)
{
	switch (wKey) {
		case VK_F1:
		{
			if (g_pRenderDevice->function_state & FUNC_STATE_CULL_BACK)
			{
				g_pRenderDevice->function_state &= ~(FUNC_STATE_CULL_BACK);
			}
			else
			{
				g_pRenderDevice->function_state |= FUNC_STATE_CULL_BACK;
			}

			break;
		}

		default:
			break;
	}
}

static LRESULT screen_events(HWND hWnd, UINT msg, 
	WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CLOSE: 
		screen_exit = 1; 
		break;
	case WM_KEYDOWN: 
		screen_keys[wParam & 511] = 1; 
		break;
	case WM_KEYUP: 
		screen_keys[wParam & 511] = 0; 
		dispatch_key_event(wParam);
		break;
	default: 
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

void screen_dispatch(void) {
	MSG msg;
	while (1) {
		if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) break;
		if (!GetMessage(&msg, NULL, 0, 0)) break;
		DispatchMessage(&msg);
	}
}

void screen_update(void) {
	HDC hDC = GetDC(screen_handle);
	BitBlt(hDC, 0, 0, screen_w, screen_h, screen_dc, 0, 0, SRCCOPY);
	ReleaseDC(screen_handle, hDC);
	screen_dispatch();
}


//{ {  1, -1, 1, 1 }, { 0, 0 }, { 1.0f, 0.2f, 0.2f }, { 0, 0,  1, 0 }, 1 },
//{ { -1, -1,  1, 1 },{ 0, 1 },{ 0.2f, 1.0f, 0.2f },{ 0, 0,  1, 0 }, 1 },
//{ { -1,  1,  1, 1 },{ 1, 1 },{ 0.2f, 0.2f, 1.0f },{ 0, 0,  1, 0 }, 1 },
//{ { 1,  1,  1, 1 },{ 1, 0 },{ 1.0f, 0.2f, 1.0f },{ 0, 0,  1, 0 }, 1 },

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

void set_vertex_attrib_pointer(device_t* device,vertex_t* vertex_array)
{
	device->vertex_array = vertex_array;
}

// 简单期间 索引全部用int
void draw_elements(device_t* device,IUINT8 uElementType, IUINT32 uElementCount, int* index)
{	
	if (TRIANGLES == uElementType)
	{
		if (device->vertex_array == NULL)
		{
			return;
		}

		int i;
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
	draw_elements(device, TRIANGLES, 12, index);
}

void camera_at_zero(device_t *device, float x, float y, float z) {
	point_t eye = { x, y, z, 1 }, at = { 0, 0, 0, 1 }, up = { 0, 0, 1, 1 };
	matrix_set_lookat(&device->transform.view, &eye, &at, &up);
	transform_update(&device->transform);
	device->eye = eye;
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

int main(void)
{
	device_t device;
	g_pRenderDevice = &device;

	int states[] = { RENDER_STATE_WIREFRAME, RENDER_STATE_TEXTURE, RENDER_STATE_COLOR, RENDER_STATE_LAMBERT_LIGHT_TEXTURE };
	int indicator = 0;
	int kbhit = 0;
	float alpha = 0;
	float pos = 5;

	TCHAR *title = _T("Mini3d (software render tutorial) - ")
		_T("Left/Right: rotation, Up/Down: forward/backward, Space: switch state");

	if (screen_init(800, 600, title)) 
		return -1;

	device_init(&device, 800, 600, screen_fb);
	camera_at_zero(&device, 0, 0, 0);

	init_texture(&device);

	function_default_para_light(g_pRenderDevice);

	while (screen_exit == 0 && screen_keys[VK_ESCAPE] == 0) {
		screen_dispatch();
		device_clear(&device, 1);
		camera_at_zero(&device, pos, pos, pos);
		
		if (screen_keys[VK_UP]) pos -= 0.01f;
		if (screen_keys[VK_DOWN]) pos += 0.01f;
		if (screen_keys[VK_LEFT]) alpha += 0.01f;
		if (screen_keys[VK_RIGHT]) alpha -= 0.01f;

		if (screen_keys[VK_SPACE]) {
			if (kbhit == 0) {
				kbhit = 1;
				if (++indicator >= 4) indicator = 0;
				device.render_state = states[indicator];
			}
		}	else {
			kbhit = 0;
		}

		draw_box(&device, alpha);
		screen_update();
		Sleep(1);
	}
	return 0;
}

