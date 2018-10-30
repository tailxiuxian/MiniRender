#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "GDIView.h"
#include "renderstate.h"

//=====================================================================
// Win32 窗口及图形绘制：为 device 提供一个 DibSection 的 FB
//=====================================================================
int screen_w, screen_h, screen_exit = 0;
int screen_mx = 0, screen_my = 0, screen_mb = 0;
static HWND screen_handle = NULL;		// 主窗口 HWND
static HDC screen_dc = NULL;			// 配套的 HDC
static HBITMAP screen_hb = NULL;		// DIB
static HBITMAP screen_ob = NULL;		// 老的 BITMAP
unsigned char *screen_fb = NULL;		// frame buffer
long screen_pitch = 0;

#ifdef _MSC_VER
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

extern void set_key_state(int keyCode, int value);
extern void set_key_quit();

// 初始化窗口并设置标题
int screen_init(int w, int h) {
	WNDCLASS wc = { CS_BYTEALIGNCLIENT, (WNDPROC)screen_events, 0, 0, 0,
		NULL, NULL, NULL, NULL, _T("SCREEN3.1415926") };
	BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB,
		w * h * 4, 0, 0, 0, 0 } };
	RECT rect = { 0, 0, w, h };
	int wx, wy, sx, sy;
	LPVOID ptr;
	HDC hDC;

	screen_close();

	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (!RegisterClass(&wc)) return -1;

	TCHAR *title = _T("Mini3d (software render tutorial) - ")
		_T("Left/Right: rotation, Up/Down: forward/backward, Space: switch state");

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
	device_t* device = get_device_inst();

	switch (wKey) {
	case VK_F1:
	{
		if (device->function_state & FUNC_STATE_CULL_BACK)
		{
			device->function_state &= ~(FUNC_STATE_CULL_BACK);
		}
		else
		{
			device->function_state |= FUNC_STATE_CULL_BACK;
		}

		break;
	}

	case VK_ESCAPE:
	{
		set_key_quit();
		break;
	}

	default:
		break;
	}
}

static void notify_key_state(int keyCode,int value)
{
	if (keyCode == VK_UP)
	{
		set_key_state(MOVE_NEAR, value);
	}
	else if (keyCode == VK_DOWN)
	{
		set_key_state(MOVE_FAR, value);
	}
	else if (keyCode == VK_LEFT)
	{
		set_key_state(ROTATE_LEFT, value);
	}
	else if (keyCode == VK_RIGHT)
	{
		set_key_state(ROTATE_RIGHT, value);
	}
	else if (keyCode == VK_SPACE)
	{
		set_key_state(CHANGE_MODE, value);
	}
}

LRESULT screen_events(HWND hWnd, UINT msg,
	WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_KEYDOWN:
		notify_key_state(wParam & 511, 1);
		break;
	case WM_KEYUP:
		notify_key_state(wParam & 511, 0);
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

unsigned char* get_screen_fb()
{
	return screen_fb;
}

int get_screen_exit()
{
	return screen_exit;
}

HWND get_screen_handle()
{
	return screen_handle;
}

void set_screen_title(TCHAR* title)
{
	SetWindowText(get_screen_handle(), title);
}

void draw_screen_title(device_t *device)
{
	TCHAR title[128];
	title[0] = _T('\0');
	lstrcat(title, _T("GDIView "));
	switch (device->render_state)
	{
	case RENDER_STATE_WIREFRAME:
	{
		lstrcat(title, _T("Line Mode"));
		break;
	}
	case RENDER_STATE_TEXTURE:
	{
		lstrcat(title, _T("Texture Mode"));
		break;
	}
	case RENDER_STATE_COLOR:
	{
		lstrcat(title, _T("Color Mode"));
		break;
	}
	case RENDER_STATE_LAMBERT_LIGHT_TEXTURE:
	{
		lstrcat(title, _T("Lambert Light Mode"));
		break;
	}
	case RENDER_STATE_PHONG_LIGHT_TEXTURE:
	{
		lstrcat(title, _T("Phong Light Mode"));
		break;
	}
	case RENDER_STATE_TEXTURE_ALPHA:
	{
		lstrcat(title, _T("Texture Alpha Mode"));
		break;
	}
	default:
		lstrcat(title, _T("No Find  Mode"));
		break;
	}

	switch (device->function_state)
	{
	case FUNC_STATE_CULL_BACK:
	{
		lstrcat(title, _T(" - culling back"));
		break;
	}
	default:
		break;
	}

	set_screen_title(title);
}