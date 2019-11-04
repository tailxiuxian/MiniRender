#pragma once

#include "mathlib.h"

class CCamera
{
public:
	CCamera() {};
	~CCamera() {};

	CCamera(point_t eye, vector_t at, vector_t up)
	{
		_eye = eye;
		_at = at;
		_up = up;
	};

	static void makeup_view_matrix(matrix_t *m, point_t eye, vector_t at, vector_t up)
	{
		vector_t xaxis, yaxis, zaxis;

		vector_sub(&zaxis, &at, &eye);
		vector_normalize(&zaxis);
		vector_crossproduct(&xaxis, &up, &zaxis);
		vector_normalize(&xaxis);
		vector_crossproduct(&yaxis, &zaxis, &xaxis);

		m->m[0][0] = xaxis.x;
		m->m[1][0] = xaxis.y;
		m->m[2][0] = xaxis.z;
		m->m[3][0] = -vector_dotproduct(&xaxis, &eye);

		m->m[0][1] = yaxis.x;
		m->m[1][1] = yaxis.y;
		m->m[2][1] = yaxis.z;
		m->m[3][1] = -vector_dotproduct(&yaxis, &eye);

		m->m[0][2] = zaxis.x;
		m->m[1][2] = zaxis.y;
		m->m[2][2] = zaxis.z;
		m->m[3][2] = -vector_dotproduct(&zaxis, &eye);

		m->m[0][3] = m->m[1][3] = m->m[2][3] = 0.0f;
		m->m[3][3] = 1.0f;
	}

	void makeup_view_matrix(matrix_t *m)
	{
		makeup_view_matrix(m, _eye, _at, _up);
	}

	void set_eye(const point_t eye) { _eye = eye; };

	vector_t get_eye() { return _eye; };

private:
	point_t _eye;
	vector_t _at;
	vector_t _up;
};