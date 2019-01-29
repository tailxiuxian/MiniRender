#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <tchar.h>

#include "device.h"
#include "renderstate.h"
#include "comm_func.h"

// 设备初始化，fb为外部帧缓存，非 NULL 将引用外部帧缓存（每行 4字节对齐）
void device_init(device_t *device, int width, int height, void *fb) {
	int need = sizeof(void*) * (MAX_FRAME_BUFFER_HEIGHT * 2 * (MAX_FRAME_BUFFER + 1) + 1024 * MAX_TEXTURE_NUM) + MAX_FRAME_BUFFER_WIDTH * MAX_FRAME_BUFFER_HEIGHT * 8 * (MAX_FRAME_BUFFER + 1);
	char *ptr = (char*)malloc(need + 64);
	char *framebuf, *zbuf;
	int j;
	assert(ptr);
	device->framebuffer = (IUINT32**)ptr;
	device->zbuffer = (float**)(ptr + sizeof(void*) * MAX_FRAME_BUFFER_HEIGHT);
	ptr += sizeof(void*) * MAX_FRAME_BUFFER_HEIGHT * 2;

	for (j = 0; j < MAX_FRAME_BUFFER; j++)
	{
		device->framebuffer_array[j].framebuffer = (IUINT32**)ptr;
		device->framebuffer_array[j].zbuffer = (float**)(ptr + sizeof(void*) * MAX_FRAME_BUFFER_HEIGHT);
		ptr += sizeof(void*) * MAX_FRAME_BUFFER_HEIGHT * 2;
	}

	for (j = 0; j < MAX_TEXTURE_NUM; j++)
	{
		device->texture_array[j].texture = (IUINT32**)ptr;
		ptr += sizeof(void*) * 1024;
	}
	
	framebuf = (char*)ptr;
	zbuf = (char*)ptr + MAX_FRAME_BUFFER_WIDTH * MAX_FRAME_BUFFER_HEIGHT * 4;
	ptr += MAX_FRAME_BUFFER_WIDTH * MAX_FRAME_BUFFER_HEIGHT * 8;
	if (fb != NULL)
	{
		framebuf = (char*)fb;
	}

	for (j = 0; j < MAX_FRAME_BUFFER_HEIGHT; j++) {
		device->framebuffer[j] = (IUINT32*)(framebuf + MAX_FRAME_BUFFER_WIDTH * 4 * j);
		device->zbuffer[j] = (float*)(zbuf + MAX_FRAME_BUFFER_WIDTH * 4 * j);
	}

	int i;
	for (i = 0; i < MAX_FRAME_BUFFER; i++)
	{
		framebuf = (char*)ptr;
		zbuf = (char*)ptr + MAX_FRAME_BUFFER_WIDTH * MAX_FRAME_BUFFER_HEIGHT * 4;
		ptr += MAX_FRAME_BUFFER_WIDTH * MAX_FRAME_BUFFER_HEIGHT * 8;
		for (j = 0; j < MAX_FRAME_BUFFER_HEIGHT; j++) {
			device->framebuffer_array[i].framebuffer[j] = (IUINT32*)(framebuf + MAX_FRAME_BUFFER_WIDTH * 4 * j);
			device->framebuffer_array[i].zbuffer[j] = (float*)(zbuf + MAX_FRAME_BUFFER_WIDTH * 4 * j);
			device->framebuffer_array[i].is_used = false;
		}
	}
	
	for (j = 0; j < MAX_TEXTURE_NUM; j++)
	{
		device->texture_array[j].texture[0] = (IUINT32*)ptr;
		device->texture_array[j].texture[1] = (IUINT32*)(ptr + 16);
		memset(device->texture_array[j].texture[0], 0, 64);
		device->texture_array[j].width = 2;
		device->texture_array[j].height = 2;
		device->texture_array[j].max_u = 1.0f;
		device->texture_array[j].max_v = 1.0f;
		device->texture_array[j].is_used = false;
	}
	
	device->screen_width = width;
	device->screen_height = height;
	device->framebuffer_width = width;
	device->framebuffer_height = height;
	device->background = 0xc0c0c0ff;
	device->foreground = 0;
	transform_init(&device->transform, device->framebuffer_width, device->framebuffer_height);
	device->render_state = RENDER_STATE_BLINN_LIGHT_TEXTURE;
	device->function_state = 0;
	device->bind_frame_buffer_idx = RENDER_NO_SET_FRAMEBUFFER_INDEX;
}

void device_destroy(device_t *device) {
	if (device->framebuffer)
		free(device->framebuffer);
	
	device->framebuffer = NULL;
	device->zbuffer = NULL;

	for (int i = 0; i < MAX_TEXTURE_NUM; i++)
	{
		device->texture_array[i].texture = NULL;
	}
}

// 设置当前纹理
void device_set_texture(device_t *device, void *bits, long pitch, int w, int h, int texture_id) {
	char *ptr = (char*)bits;
	int j;
	assert(w <= 1024 && h <= 1024);
	for (j = 0; j < h; ptr += pitch, j++) 	// 重新计算每行纹理的指针
	{
		device->texture_array[texture_id].texture[j] = (IUINT32*)ptr;
	}
		
	device->texture_array[texture_id].width = w;
	device->texture_array[texture_id].height = h;
	device->texture_array[texture_id].max_u = (float)(w - 1);
	device->texture_array[texture_id].max_v = (float)(h - 1);
}

// 清空 framebuffer 和 zbuffer
void device_clear(device_t *device, int mode) {
	int y, x, height = device->framebuffer_height;
	for (y = 0; y < device->framebuffer_height; y++) {
		IUINT32 *dst = device->framebuffer[y];
		IUINT32 cc = (height - 1 - y) * 230 / (height - 1);
		
#ifdef USE_GDI_VIEW
		cc = (cc << 16) | (cc << 8) | cc;
#else
		cc = (cc << 24) | (cc << 16) | (cc << 8) | (255);
#endif

		if (mode == 0) cc = device->background;
		for (x = device->framebuffer_width; x > 0; dst++, x--) dst[0] = cc;
	}
	for (y = 0; y < device->framebuffer_height; y++) {
		float *dst = device->zbuffer[y];
		for (x = device->framebuffer_width; x > 0; dst++, x--) dst[0] = 0.0f;
	}
}

int device_gen_frame_buffer(device_t* device)
{
	for (int i = 0; i < MAX_FRAME_BUFFER; i++)
	{
		if (device->framebuffer_array[i].is_used == false)
		{
			device->framebuffer_array[i].is_used = true;
			return i;
		}
	}

	return -1;
}

bool device_bind_framebuffer(device_t* device, int framebuffer_id)
{
	if (framebuffer_id < 0 || framebuffer_id >= MAX_FRAME_BUFFER)
	{
		return false;
	}

	if (device->framebuffer_array[framebuffer_id].is_used == false)
	{
		return false;
	}

	device->bind_frame_buffer_idx = framebuffer_id;
	return true;
}

bool device_unbind_framebuffer(device_t* device, int framebuffer_id)
{
	if (framebuffer_id < 0 || framebuffer_id >= MAX_FRAME_BUFFER)
	{
		return false;
	}

	if (device->framebuffer_array[framebuffer_id].is_used == false)
	{
		return false;
	}

	device->bind_frame_buffer_idx = RENDER_NO_SET_FRAMEBUFFER_INDEX;
	return true;
}

void device_clear_framebuffer(device_t* device,  int framebuffer_id, int mode)
{
	if (framebuffer_id < 0 || framebuffer_id > MAX_FRAME_BUFFER)
	{
		return;
	}

	if (device->framebuffer_array[framebuffer_id].is_used == false)
	{
		return;
	}

	int y, x, height = device->framebuffer_height;
	for (y = 0; y < device->framebuffer_height; y++) {
		IUINT32 *dst = device->framebuffer_array[framebuffer_id].framebuffer[y];
		IUINT32 cc = (height - 1 - y) * 230 / (height - 1);

#ifdef USE_GDI_VIEW
		cc = (cc << 16) | (cc << 8) | cc;
#else
		cc = (cc << 24) | (cc << 16) | (cc << 8) | (255);
#endif

		if (mode == 0) cc = device->background;
		for (x = device->framebuffer_width; x > 0; dst++, x--) dst[0] = cc;
	}
	for (y = 0; y < device->framebuffer_height; y++) {
		float *dst = device->framebuffer_array[framebuffer_id].zbuffer[y];
		for (x = device->framebuffer_width; x > 0; dst++, x--) dst[0] = 0.0f;
	}
}

void device_copy_framebuffer(device_t* device, int framebuffer_id, IUINT32** buffer)
{
	if (framebuffer_id < 0 || framebuffer_id > MAX_FRAME_BUFFER)
	{
		return;
	}

	if (device->framebuffer_array[framebuffer_id].is_used == false)
	{
		return;
	}
	
	int y, x, height = device->framebuffer_height;
	for (y = 0; y < device->framebuffer_height; y++) {
		;
		for (x = 0; x < device->framebuffer_width; x++)
		{
			buffer[y][x] = device->framebuffer_array[framebuffer_id].framebuffer[y][x];
		}
	}
}

void device_copy_framebuffer_z(device_t* device, int framebuffer_id, float** zbuffer)
{
	if (framebuffer_id < 0 || framebuffer_id > MAX_FRAME_BUFFER)
	{
		return;
	}

	if (device->framebuffer_array[framebuffer_id].is_used == false)
	{
		return;
	}

	int y, x, height = device->framebuffer_height;
	for (y = 0; y < device->framebuffer_height; y++) {
		;
		for (x = 0; x < device->framebuffer_width; x++)
		{
			zbuffer[y][x] = device->framebuffer_array[framebuffer_id].zbuffer[y][x];
		}
	}
}

void device_copy_colorbuffer(device_t* device, IUINT32** buffer)
{
	int y, x, height = device->framebuffer_height;
	for (y = 0; y < device->framebuffer_height; y++) {;
		for (x = 0; x < device->framebuffer_width; x++)
		{
			buffer[y][x] = device->framebuffer[y][x];
		}
	}
}

void device_set_shader_state(device_t* device, int shader_state)
{
	device->shader_state = shader_state;
}

// 根据坐标读取纹理
IUINT32 device_texture_read(const device_t *device, float u, float v, int texture_id) {
	int x, y;
	u = u * device->texture_array[texture_id].max_u;
	v = v * device->texture_array[texture_id].max_v;
	x = (int)(u + 0.5f);
	y = (int)(v + 0.5f);
	x = CMID(x, 0, device->texture_array[texture_id].width - 1);
	y = CMID(y, 0, device->texture_array[texture_id].height - 1);
	return device->texture_array[texture_id].texture[y][x];
}

float device_texture_read_float(const device_t *device, float u, float v, int texture_id)
{
	int x, y;
	u = u * device->texture_array[texture_id].max_u;
	v = v * device->texture_array[texture_id].max_v;
	x = (int)(u + 0.5f);
	y = (int)(v + 0.5f);
	x = CMID(x, 0, device->texture_array[texture_id].width - 1);
	y = CMID(y, 0, device->texture_array[texture_id].height - 1);
	float* ptr =  (float*)(&(device->texture_array[texture_id].texture[y][x]));
	return *ptr;
}

void device_set_vertex_attrib_pointer(device_t* device, vertex_t* vertex_array)
{
	device->vertex_array = vertex_array;
}

void device_set_uniform_vector_value(device_t* device, int iUniformIndex, vector_t* pVec)
{
	if (iUniformIndex >= 0 && iUniformIndex < MAX_UNIFORM_NUM)
	{
		device->uniform_vector[iUniformIndex] = *(pVec);
	}
}

void device_set_uniform_matrix_value(device_t* device, int iUniformIndex, matrix_t* pMat)
{
	if (iUniformIndex >= 0 && iUniformIndex < MAX_UNIFORM_NUM)
	{
		device->uniform_matrix[iUniformIndex] = *(pMat);
	}
}

int device_gen_texture(device_t* device)
{
	for (int i = 0; i < MAX_TEXTURE_NUM; i++)
	{
		if (device->texture_array[i].is_used == false)
		{
			device->texture_array[i].is_used = true;
			return i;
		}
	}

	return -1;
}

void device_bind_texture(device_t* device, int iIndex, int texture_id)
{
	if (device->texture_array[texture_id].is_used == true)
	{
		if (iIndex < 0 || iIndex >= MAX_TEXTURE_NUM)
		{
			return;
		}

		device->texture_id[iIndex] = texture_id;
	}
}

unsigned int device_get_framebuffer_data(device_t* device, int h, int w)
{
	if (device->function_state & FUNC_STATE_ANTI_ALIAS_FSAA)
	{
		unsigned int R = 0;
		unsigned int G = 0;
		unsigned int B = 0;
		unsigned int A = 0;

		static unsigned int pixel_array[4];
		pixel_array[0] = device->framebuffer[2 * h][2 * w];
		pixel_array[1] = device->framebuffer[2 * h][2 * w + 1];
		pixel_array[2] = device->framebuffer[2 * h + 1][2 * w];
		pixel_array[3] = device->framebuffer[2 * h + 1][2 * w + 1];

		for (int i = 0; i < 4; i++)
		{
			R += Get_R(pixel_array[i]) / 4;
			G += Get_G(pixel_array[i]) / 4;
			B += Get_B(pixel_array[i]) / 4;
			A += Get_A(pixel_array[i]) / 4;
		}

		R = CMID(R, 0, 255);
		G = CMID(G, 0, 255);
		B = CMID(B, 0, 255);
		A = CMID(A, 0, 255);

		return (R << 24) | (G << 16) | (B << 8) | (A);
	}
	else
	{
		return device->framebuffer[h][w];
	}
}

unsigned int device_enable_render_func_state(device_t* device, int iState)
{
	if (iState == FUNC_STATE_ANTI_ALIAS_FSAA)
	{
		if (device->function_state & FUNC_STATE_ANTI_ALIAS_FSAA)
		{
			return 1;
		}

		if (device->screen_height * 2 > MAX_FRAME_BUFFER_HEIGHT || device->screen_width * 2 > MAX_FRAME_BUFFER_WIDTH)
		{
			return 2;
		}

		device->framebuffer_width = device->screen_width * 2;
		device->framebuffer_height = device->screen_height * 2;
		transform_init(&device->transform, device->framebuffer_width, device->framebuffer_height);
		device->function_state |= FUNC_STATE_ANTI_ALIAS_FSAA;
		return 0;
	}
	else if (iState == FUNC_STATE_CULL_BACK)
	{
		if (device->function_state & FUNC_STATE_CULL_BACK)
		{
			return 1;
		}

		device->function_state |= FUNC_STATE_CULL_BACK;
		return 0;
	}

	return 3;
}

unsigned int device_disable_render_func_state(device_t* device, int iState)
{
	if (iState == FUNC_STATE_ANTI_ALIAS_FSAA)
	{
		if (device->function_state & FUNC_STATE_ANTI_ALIAS_FSAA)
		{
			device->framebuffer_width = device->screen_width;
			device->framebuffer_height = device->screen_height;
			transform_init(&device->transform, device->framebuffer_width, device->framebuffer_height);
			device->function_state &= ~(FUNC_STATE_ANTI_ALIAS_FSAA);
			return 0;
		}
	}
	else if (iState == FUNC_STATE_CULL_BACK)
	{
		if (device->function_state & FUNC_STATE_CULL_BACK)
		{
			device->function_state &= ~(FUNC_STATE_CULL_BACK);
			return 0;
		}
	}

	return 3;
}

void device_set_blend_state(device_t* device, blendstate_t blend_state)
{
	device->blend_state = blend_state;
}

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

device_t* get_device_inst()
{
	static device_t* g_pRenderDevice = NULL;
	if (g_pRenderDevice == NULL)
	{
		g_pRenderDevice = new device_t;
	}
	return g_pRenderDevice;
}