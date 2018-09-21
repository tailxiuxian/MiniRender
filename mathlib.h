#pragma once

//=====================================================================
// ��ѧ�⣺�˲���Ӧ�ò�����⣬��Ϥ D3D ����任����
//=====================================================================
typedef struct { float m[4][4]; } matrix_t;
typedef struct { float x, y, z, w; } vector_t;
typedef vector_t point_t;

int CMID(int x, int min, int max);
float interp(float x1, float x2, float t);

// | v |
float vector_length(const vector_t *v);

// z = x + y
void vector_add(vector_t *z, const vector_t *x, const vector_t *y);

// z = x - y
void vector_sub(vector_t *z, const vector_t *x, const vector_t *y);

// ʸ�����
float vector_dotproduct(const vector_t *x, const vector_t *y);

// ʸ�����
void vector_crossproduct(vector_t *z, const vector_t *x, const vector_t *y);

// ʸ����ֵ��tȡֵ [0, 1]
void vector_interp(vector_t *z, const vector_t *x1, const vector_t *x2, float t);

// ʸ����һ��
void vector_normalize(vector_t *v);

// c = a + b
void matrix_add(matrix_t *c, const matrix_t *a, const matrix_t *b);

// c = a - b
void matrix_sub(matrix_t *c, const matrix_t *a, const matrix_t *b);

// c = a * b
void matrix_mul(matrix_t *c, const matrix_t *a, const matrix_t *b);

// c = a * f
void matrix_scale(matrix_t *c, const matrix_t *a, float f);

// y = x * m
void matrix_apply(vector_t *y, const vector_t *x, const matrix_t *m);

void matrix_set_identity(matrix_t *m);

void matrix_set_zero(matrix_t *m);

// ƽ�Ʊ任
void matrix_set_translate(matrix_t *m, float x, float y, float z);
// ���ű任
void matrix_set_scale(matrix_t *m, float x, float y, float z);

// ��ת����
void matrix_set_rotate(matrix_t *m, float x, float y, float z, float theta);

// ���������
void matrix_set_lookat(matrix_t *m, const vector_t *eye, const vector_t *at, const vector_t *up);

// D3DXMatrixPerspectiveFovLH
void matrix_set_perspective(matrix_t *m, float fovy, float aspect, float zn, float zf);

// ��������
void matrix_inverse(matrix_t *src, matrix_t *dst);

// ����ת��
void matrix_transpose(matrix_t *src, matrix_t *dst);