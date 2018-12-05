#pragma once

#include "basetype.h"
#include "mathlib.h"
#include "transform.h"
#include "geometry.h"

//=====================================================================
// ��Ⱦ�豸
//=====================================================================

typedef struct {
	IUINT32 **texture; // �������� ͬ����ÿ������
	int width;
	int height;
	float max_u;
	float max_v;
	bool is_used;
} texture_t;

typedef struct {
	IUINT32 **framebuffer;
	float **zbuffer;
	bool is_used;
} framebuffer_t;

#define MAX_UNIFORM_NUM 128
#define MAX_TEXTURE_NUM 16
#define MAX_VERTEX_NUM 4
#define MAX_FRAME_BUFFER 4
#define RENDER_NO_SET_FRAMEBUFFER_INDEX -1

typedef struct {
	int srcState;
	int dstState;
} blendstate_t;

typedef struct {
	transform_t transform;      // ����任��
	int width;                  // ���ڿ��
	int height;                 // ���ڸ߶�
	IUINT32 **framebuffer;      // ���ػ��棺framebuffer[y] ����� y��
	float **zbuffer;            // ��Ȼ��棺zbuffer[y] Ϊ�� y��ָ��
	int render_state;           // ��Ⱦ״̬
	int shader_state;			// shader�����״̬
	IUINT32 background;         // ������ɫ
	IUINT32 foreground;         // �߿���ɫ
	int function_state;			// ����״̬		
	texture_t texture_array[MAX_TEXTURE_NUM];// Texture
	framebuffer_t framebuffer_array[MAX_FRAME_BUFFER]; // framebuffer

	int bind_frame_buffer_idx;

	//----------------------- shader ��Ϣ

	// Attribute
	vertex_t* vertex_array; // Ӧ������һ���Դ� ʵ���ڴ浽�Դ��copy ��index������洢

	// Uniform
	vector_t uniform_vector[MAX_UNIFORM_NUM];

	matrix_t uniform_matrix[MAX_UNIFORM_NUM];

	// Texture ID
	int texture_id[MAX_TEXTURE_NUM];

	// Blend State
	blendstate_t blend_state;

}	device_t;

#define FUNC_STATE_CULL_BACK		1		// �����޳�

void device_init(device_t *device, int width, int height, void *fb); //��ʼ����Ⱦ�豸
void device_destroy(device_t *device); // ɾ���豸		   
void device_set_texture(device_t *device, void *bits, long pitch, int w, int h, int texture_id);// ���õ�ǰ����
void device_clear(device_t *device, int mode); // ��� framebuffer �� zbuffer						   
IUINT32 device_texture_read(const device_t *device, float u, float v, int texture_id); // ���������ȡ����
void device_set_vertex_attrib_pointer(device_t* device, vertex_t* vertex_array); // ���ö�������
float device_texture_read_float(const device_t *device, float u, float v, int texture_id);

void device_set_uniform_vector_value(device_t* device, int iUniformIndex, vector_t* pVec);
void device_set_uniform_matrix_value(device_t* device, int iUniformIndex, matrix_t* pMat);

int device_gen_texture(device_t* device);
void device_bind_texture(device_t* device, int iIndex, int texture_id);

void device_set_blend_state(device_t* device, blendstate_t blend_state);

int function_cull_back(device_t* device, point_t* p1, point_t* p2, point_t* p3); // �����޳�

device_t* get_device_inst();

int device_gen_frame_buffer(device_t* device);
void device_clear_framebuffer(device_t* device, int framebuffer_id, int mode);
void device_copy_framebuffer_z(device_t* device, int framebuffer_id, float** zbuffer);
bool device_bind_framebuffer(device_t* device, int framebuffer_id);
bool device_unbind_framebuffer(device_t* device, int framebuffer_id);

void device_copy_colorbuffer(device_t* device, IUINT32** buffer);

void device_set_shader_state(device_t* device, int shader_state);