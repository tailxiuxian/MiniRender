#pragma once

#include "basetype.h"
#include "mathlib.h"
#include "transform.h"
#include "geometry.h"

//=====================================================================
// ��Ⱦ�豸
//=====================================================================

typedef struct {
	color_t energy;
	vector_t direction; //������
} para_light_source_t;

typedef struct {
	transform_t transform;      // ����任��
	int width;                  // ���ڿ��
	int height;                 // ���ڸ߶�
	IUINT32 **framebuffer;      // ���ػ��棺framebuffer[y] ����� y��
	float **zbuffer;            // ��Ȼ��棺zbuffer[y] Ϊ�� y��ָ��
	IUINT32 **texture;          // ����ͬ����ÿ������
	int tex_width;              // ������
	int tex_height;             // ����߶�
	float max_u;                // ��������ȣ�tex_width - 1
	float max_v;                // �������߶ȣ�tex_height - 1
	int render_state;           // ��Ⱦ״̬
	IUINT32 background;         // ������ɫ
	IUINT32 foreground;         // �߿���ɫ
	int function_state;			// ����״̬

	// Attribute
	vertex_t* vertex_array; // Ӧ������һ���Դ� ʵ���ڴ浽�Դ��copy ��index������洢

	// Uniform
	para_light_source_t para_light;
	point_t eye;
}	device_t;

#define FUNC_STATE_CULL_BACK		1		// �����޳�

void device_init(device_t *device, int width, int height, void *fb); //��ʼ����Ⱦ�豸
void device_destroy(device_t *device); // ɾ���豸		   
void device_set_texture(device_t *device, void *bits, long pitch, int w, int h);// ���õ�ǰ����
void device_clear(device_t *device, int mode); // ��� framebuffer �� zbuffer						   
IUINT32 device_texture_read(const device_t *device, float u, float v); // ���������ȡ����
void device_set_vertex_attrib_pointer(device_t* device, vertex_t* vertex_array); // ���ö�������

int function_cull_back(device_t* device, point_t* p1, point_t* p2, point_t* p3); // �����޳�

device_t* get_device_inst();