#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <tchar.h>

#include "device.h"
#include "renderstate.h"

// 设备初始化，fb为外部帧缓存，非 NULL 将引用外部帧缓存（每行 4字节对齐）
void device_init(device_t *device, int width, int height, void *fb) {
	int need = sizeof(void*) * (height * 2 * (MAX_FRAME_BUFFER + 1) + 1024 * MAX_TEXTURE_NUM) + width * height * 8 * (MAX_FRAME_BUFFER + 1);
	char *ptr = (char*)malloc(need + 64);
	char *framebuf, *zbuf;
	int j;
	assert(ptr);
	device->framebuffer = (IUINT32**)ptr;
	device->zbuffer = (float**)(ptr + sizeof(void*) * height);
	ptr += sizeof(void*) * height * 2;

	for (j = 0; j < MAX_FRAME_BUFFER; j++)
	{
		device->framebuffer_array[j].framebuffer = (IUINT32**)ptr;
		device->framebuffer_array[j].zbuffer = (float**)(ptr + sizeof(void*) * height);
		ptr += sizeof(void*) * height * 2;
	}

	for (j = 0; j < MAX_TEXTURE_NUM; j++)
	{
		device->texture_array[j].texture = (IUINT32**)ptr;
		ptr += sizeof(void*) * 1024;
	}
	
	framebuf = (char*)ptr;
	zbuf = (char*)ptr + width * height * 4;
	ptr += width * height * 8;
	if (fb != NULL)
	{
		framebuf = (char*)fb;
	}

	for (j = 0; j < height; j++) {
		device->framebuffer[j] = (IUINT32*)(framebuf + width * 4 * j);
		device->zbuffer[j] = (float*)(zbuf + width * 4 * j);
	}

	int i;
	for (i = 0; i < MAX_FRAME_BUFFER; i++)
	{
		framebuf = (char*)ptr;
		zbuf = (char*)ptr + width * height * 4;
		ptr += width * height * 8;
		for (j = 0; j < height; j++) {
			device->framebuffer_array[i].framebuffer[j] = (IUINT32*)(framebuf + width * 4 * j);
			device->framebuffer_array[i].zbuffer[j] = (float*)(zbuf + width * 4 * j);
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
	
	device->width = width;
	device->height = height;
	device->background = 0xc0c0c0ff;
	device->foreground = 0;
	transform_init(&device->transform, width, height);
	device->render_state = RENDER_STATE_SHADOW_MAP;
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
	int y, x, height = device->height;
	for (y = 0; y < device->height; y++) {
		IUINT32 *dst = device->framebuffer[y];
		IUINT32 cc = (height - 1 - y) * 230 / (height - 1);
		
#ifdef USE_GDI_VIEW
		cc = (cc << 16) | (cc << 8) | cc;
#else
		cc = (cc << 24) | (cc << 16) | (cc << 8) | (255);
#endif

		if (mode == 0) cc = device->background;
		for (x = device->width; x > 0; dst++, x--) dst[0] = cc;
	}
	for (y = 0; y < device->height; y++) {
		float *dst = device->zbuffer[y];
		for (x = device->width; x > 0; dst++, x--) dst[0] = 0.0f;
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
	int y, x, height = device->height;
	for (y = 0; y < device->height; y++) {
		IUINT32 *dst = device->framebuffer_array[framebuffer_id].framebuffer[y];
		IUINT32 cc = (height - 1 - y) * 230 / (height - 1);

#ifdef USE_GDI_VIEW
		cc = (cc << 16) | (cc << 8) | cc;
#else
		cc = (cc << 24) | (cc << 16) | (cc << 8) | (255);
#endif

		if (mode == 0) cc = device->background;
		for (x = device->width; x > 0; dst++, x--) dst[0] = cc;
	}
	for (y = 0; y < device->height; y++) {
		float *dst = device->framebuffer_array[framebuffer_id].zbuffer[y];
		for (x = device->width; x > 0; dst++, x--) dst[0] = 0.0f;
	}
}

void device_copy_framebuffer_z(device_t* device, int framebuffer_id, float** zbuffer)
{
	int y, x, height = device->height;
	for (y = 0; y < device->height; y++) {
		;
		for (x = 0; x < device->width; x++)
		{
			zbuffer[y][x] = device->framebuffer_array[framebuffer_id].zbuffer[y][x];
		}
	}
}

void device_copy_colorbuffer(device_t* device, IUINT32** buffer)
{
	int y, x, height = device->height;
	for (y = 0; y < device->height; y++) {;
		for (x = 0; x < device->width; x++)
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