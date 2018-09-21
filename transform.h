#pragma once

#include "mathlib.h"

//=====================================================================
// ����任
//=====================================================================
typedef struct {
	matrix_t world;         // ��������任
	matrix_t view;          // ��Ӱ������任
	matrix_t projection;    // ͶӰ�任
	matrix_t transform;     // transform = world * view * projection
	float w, h;             // ��Ļ��С
	matrix_t worldInv;		// ��������任�������
	matrix_t viewInv;		// ��Ӱ���任�������
}	transform_t;


// ������£����� transform = world * view * projection
void transform_update(transform_t *ts);

// ��ʼ����������Ļ����
void transform_init(transform_t *ts, int width, int height);

// ��ʸ�� x ���� project 
void transform_apply(const transform_t *ts, vector_t *y, const vector_t *x);

// ����������ͬ cvv �ı߽�������׶�ü�
int transform_check_cvv(const vector_t *v);

// ��һ�����õ���Ļ����
void transform_homogenize(const transform_t *ts, vector_t *y, const vector_t *x);