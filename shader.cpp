#include <stdlib.h>
#include <math.h>

#include "shader.h"
#include "renderstate.h"

typedef struct {
	IUINT32 RenderState;
	func_vertex_shader p_vertex_shader;
	func_pixel_shader p_pixel_shader;
} RenderComponent;


IUINT32 Get_R(IUINT32 color)
{
	return (color >> 24) & 0x000000FF;
}

IUINT32 Get_G(IUINT32 color)
{
	return (color >> 16) & 0x000000FF;
}

IUINT32 Get_B(IUINT32 color)
{
	return (color >> 8) & 0x000000FF;
}

IUINT32 Get_A(IUINT32 color)
{
	return color & 0x000000FF;
}


// 顶点着色器
void shader_vertex_normal_mvp(device_t* device, vertex_t* vertex, point_t* output)
{
	transform_apply(&device->transform, output, &(vertex->pos));
}

void shader_vertex_phong_mvp(device_t* device, vertex_t* vertex, point_t* output)
{
	transform_apply(&device->transform, output, &(vertex->pos));

	point_t posInWorld;
	matrix_apply(&posInWorld, &(vertex->pos), &(device->transform.world));

	point_t eye = device->uniform[2];
	vector_sub(&vertex->eye_view, &eye, &posInWorld);
}

unsigned char default_alpha = 255;

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

#ifdef USE_GDI_VIEW
	*(color) = (R << 16) | (G << 8) | (B);
#else
	*(color) = (R << 24) | (G << 16) | (B << 8) | (default_alpha);
#endif
}

void shader_pixel_normal_texture(device_t* device, vertex_t* vertex, IUINT32* color)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	IUINT32 cc = device_texture_read(device, u, v, device->texture_id);
	IUINT32 texture_R = Get_R(cc);
	IUINT32 texture_G = Get_G(cc);
	IUINT32 texture_B = Get_B(cc);

#ifdef USE_GDI_VIEW
	*(color) = (texture_R << 16) | (texture_G << 8) | (texture_B);
#else
	*(color) = (texture_R << 24) | (texture_G << 16) | (texture_B << 8) | (default_alpha);
#endif
}

void shader_pixel_texture_lambert_light(device_t* device, vertex_t* vertex, IUINT32* color)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	vector_t normal = vertex->normal;
	vector_t direction = device->uniform[1];
	vector_normalize(&direction);
	vector_normalize(&normal);

	matrix_t normal_world;
	matrix_transpose(&(device->transform.worldInv), &normal_world);
	vector_t cnormal;
	matrix_apply(&cnormal, &normal, &(normal_world));

	IUINT32 cc = device_texture_read(device, u, v, device->texture_id);
	float diffuse = vector_dotproduct(&direction, &cnormal);
	if (diffuse >= 0.001)
	{
		IUINT32 texture_R = Get_R(cc);
		IUINT32 texture_G = Get_G(cc);
		IUINT32 texture_B = Get_B(cc);

		vector_t energy = device->uniform[0];
		IUINT32 diffuse_R = (IUINT32)(texture_R * diffuse * energy.x);
		IUINT32 diffuse_G = (IUINT32)(texture_G * diffuse * energy.y);
		IUINT32 diffuse_B = (IUINT32)(texture_B * diffuse * energy.z);

		diffuse_R = CMID(diffuse_R, 0, 255);
		diffuse_G = CMID(diffuse_G, 0, 255);
		diffuse_B = CMID(diffuse_B, 0, 255);

#ifdef USE_GDI_VIEW
		*(color) = (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
		*(color) = (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
	}
	else
	{
		*(color) = 0;
	}
}

void shader_pixel_texture_phong_light(device_t* device, vertex_t* vertex, IUINT32* color)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	vector_t normal = vertex->normal;
	vector_t direction = device->uniform[1];
	vector_normalize(&direction);
	vector_normalize(&normal);

	matrix_t normal_world;
	matrix_transpose(&(device->transform.worldInv), &normal_world);
	vector_t cnormal;
	matrix_apply(&cnormal, &normal, &(normal_world));

	IUINT32 cc = device_texture_read(device, u, v, device->texture_id);
	float diffuse = vector_dotproduct(&direction, &cnormal);
	if (diffuse >= 0.001)
	{
		IUINT32 texture_R = Get_R(cc);
		IUINT32 texture_G = Get_G(cc);
		IUINT32 texture_B = Get_B(cc);

		vector_t energy = device->uniform[0];
		IUINT32 diffuse_R = (IUINT32)(texture_R * diffuse * energy.x);
		IUINT32 diffuse_G = (IUINT32)(texture_G * diffuse * energy.y);
		IUINT32 diffuse_B = (IUINT32)(texture_B * diffuse * energy.z);

		vector_t vec_spec = cnormal;
		vector_scale(&vec_spec, 2 * diffuse);
		vector_sub(&vec_spec, &vec_spec, &direction);
		vector_normalize(&vec_spec);

		vector_t eye_view = vertex->eye_view;
		vector_normalize(&eye_view);

		vector_t matrial = device->uniform[3];
		float kD = matrial.x;
		float kS = matrial.y;
		float kQ = matrial.z;

		float spec = vector_dotproduct(&eye_view, &vec_spec);
		if (spec >= 0)
		{
			spec = pow(spec, kQ);
			IUINT32 spec_R = (IUINT32)(texture_R * spec * energy.x);
			IUINT32 spec_G = (IUINT32)(texture_G * spec * energy.y);
			IUINT32 spec_B = (IUINT32)(texture_B * spec * energy.z);

			diffuse_R = CMID(diffuse_R * kD + spec_R * kS, 0, 255);
			diffuse_G = CMID(diffuse_G * kD + spec_G * kS, 0, 255);
			diffuse_B = CMID(diffuse_B * kD + spec_B * kS, 0, 255);

#ifdef USE_GDI_VIEW
			*(color) = (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
			*(color) = (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
		}
		else
		{
			diffuse_R = CMID(diffuse_R * kD, 0, 255);
			diffuse_G = CMID(diffuse_G * kD, 0, 255);
			diffuse_B = CMID(diffuse_B * kD, 0, 255);

#ifdef USE_GDI_VIEW
			*(color) = (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
			*(color) = (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
		}
	}
	else
	{
		*(color) = 0;
	}
}

RenderComponent g_RenderComponent[MAX_RENDER_STATE] = {
	{ RENDER_STATE_WIREFRAME, shader_vertex_normal_mvp, NULL },
	{ RENDER_STATE_TEXTURE, shader_vertex_normal_mvp, shader_pixel_normal_texture },
	{ RENDER_STATE_COLOR, shader_vertex_normal_mvp, shader_pixel_normal_color },
	{ RENDER_STATE_LAMBERT_LIGHT_TEXTURE, shader_vertex_normal_mvp, shader_pixel_texture_lambert_light },
	{ RENDER_STATE_PHONG_LIGHT_TEXTURE, shader_vertex_phong_mvp, shader_pixel_texture_phong_light },
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