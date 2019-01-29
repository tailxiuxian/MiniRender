#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "shader.h"
#include "renderstate.h"
#include "comm_func.h"

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

void shader_vertex_phong_mvp(device_t* device, vertex_t* vertex, point_t* output)
{
	transform_apply(&device->transform, output, &(vertex->pos));

	point_t posInWorld;
	matrix_apply(&posInWorld, &(vertex->pos), &(device->transform.world));

	point_t eye = device->uniform_vector[2];
	vector_sub(&vertex->vs_result[0], &eye, &posInWorld);
}

void shader_vertex_shadow_map_mvp(device_t* device, vertex_t* vertex, point_t* output)
{
	transform_apply(&device->transform, output, &(vertex->pos));
	
	vector_t pos_in_light_space;
	matrix_apply(&(pos_in_light_space), &(vertex->pos), &(device->uniform_matrix[0]));
	transform_homogenize(&device->transform, &(vertex->vs_result[0]), &pos_in_light_space);
}

void shader_vertex_blinn_mvp(device_t* device, vertex_t* vertex, point_t* output)
{
	transform_apply(&device->transform, output, &(vertex->pos));

	point_t posInWorld;
	matrix_apply(&posInWorld, &(vertex->pos), &(device->transform.world));

	point_t eye = device->uniform_vector[2];
	vector_sub(&vertex->vs_result[0], &eye, &posInWorld);
}

unsigned char default_alpha = 255;

// 片元着色器
IUINT32 shader_pixel_normal_color(device_t* device, vertex_t* vertex)
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
	return (R << 16) | (G << 8) | (B);
#else
	return (R << 24) | (G << 16) | (B << 8) | (default_alpha);
#endif
}

IUINT32 shader_pixel_normal_texture(device_t* device, vertex_t* vertex)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	IUINT32 cc = device_texture_read(device, u, v, device->texture_id[0]);
	IUINT32 texture_R = Get_R(cc);
	IUINT32 texture_G = Get_G(cc);
	IUINT32 texture_B = Get_B(cc);

#ifdef USE_GDI_VIEW
	return (texture_R << 16) | (texture_G << 8) | (texture_B);
#else
	return (texture_R << 24) | (texture_G << 16) | (texture_B << 8) | (default_alpha);
#endif
}

IUINT32 shader_pixel_texture_lambert_light(device_t* device, vertex_t* vertex)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	vector_t normal = vertex->normal;
	vector_t direction = device->uniform_vector[1];
	vector_normalize(&direction);
	vector_normalize(&normal);

	matrix_t normal_world;
	matrix_transpose(&(device->transform.worldInv), &normal_world);
	vector_t cnormal;
	matrix_apply(&cnormal, &normal, &(normal_world));

	IUINT32 cc = device_texture_read(device, u, v, device->texture_id[0]);
	float diffuse = vector_dotproduct(&direction, &cnormal);
	if (diffuse >= 0.001)
	{
		IUINT32 texture_R = Get_R(cc);
		IUINT32 texture_G = Get_G(cc);
		IUINT32 texture_B = Get_B(cc);

		vector_t energy = device->uniform_vector[0];
		IUINT32 diffuse_R = (IUINT32)(texture_R * diffuse * energy.x);
		IUINT32 diffuse_G = (IUINT32)(texture_G * diffuse * energy.y);
		IUINT32 diffuse_B = (IUINT32)(texture_B * diffuse * energy.z);

		diffuse_R = CMID(diffuse_R, 0, 255);
		diffuse_G = CMID(diffuse_G, 0, 255);
		diffuse_B = CMID(diffuse_B, 0, 255);

#ifdef USE_GDI_VIEW
		return (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
		return (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
	}
	else
	{
#ifdef USE_GDI_VIEW
		return 0;
#else
		return 255;
#endif
	}
}

IUINT32 shader_pixel_texture_phong_light(device_t* device, vertex_t* vertex)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	vector_t normal = vertex->normal;
	vector_t direction = device->uniform_vector[1];
	vector_normalize(&direction);
	vector_normalize(&normal);

	matrix_t normal_world;
	matrix_transpose(&(device->transform.worldInv), &normal_world);
	vector_t cnormal;
	matrix_apply(&cnormal, &normal, &(normal_world));

	IUINT32 cc = device_texture_read(device, u, v, device->texture_id[0]);
	float diffuse = vector_dotproduct(&direction, &cnormal);
	if (diffuse >= 0.001)
	{
		IUINT32 texture_R = Get_R(cc);
		IUINT32 texture_G = Get_G(cc);
		IUINT32 texture_B = Get_B(cc);

		vector_t energy = device->uniform_vector[0];
		IUINT32 diffuse_R = (IUINT32)(texture_R * diffuse * energy.x);
		IUINT32 diffuse_G = (IUINT32)(texture_G * diffuse * energy.y);
		IUINT32 diffuse_B = (IUINT32)(texture_B * diffuse * energy.z);

		vector_t vec_spec = cnormal;
		vector_scale(&vec_spec, 2 * diffuse);
		vector_sub(&vec_spec, &vec_spec, &direction);
		vector_normalize(&vec_spec);

		vector_t eye_view = vertex->vs_result[0];
		vector_normalize(&eye_view);

		vector_t matrial = device->uniform_vector[3];
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
			return (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
			return (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
		}
		else
		{
			diffuse_R = CMID(diffuse_R * kD, 0, 255);
			diffuse_G = CMID(diffuse_G * kD, 0, 255);
			diffuse_B = CMID(diffuse_B * kD, 0, 255);

#ifdef USE_GDI_VIEW
			return (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
			return (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
		}
	}
	else
	{
#ifdef USE_GDI_VIEW
		return 0;
#else
		return 255;
#endif
	}
}

IUINT32 shader_pixel_normal_texture_alpha(device_t* device, vertex_t* vertex)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	IUINT32 cc = device_texture_read(device, u, v, device->texture_id[0]);
	IUINT32 texture_R = Get_R(cc);
	IUINT32 texture_G = Get_G(cc);
	IUINT32 texture_B = Get_B(cc);
	IUINT32 texture_A = 127;

#ifdef USE_GDI_VIEW
	return (texture_R << 16) | (texture_G << 8) | (texture_B);
#else
	return (texture_R << 24) | (texture_G << 16) | (texture_B << 8) | (texture_A);
#endif
}

IUINT32 shader_pixel_shadow_map(device_t* device, vertex_t* vertex)
{
	IUINT32 texture_R = 0;
	IUINT32 texture_G = 0;
	IUINT32 texture_B = IUINT32(255.0f * vertex->pos.z);

#ifdef USE_GDI_VIEW
	return (texture_R << 16) | (texture_G << 8) | (texture_B);
#else
	return (texture_R << 24) | (texture_G << 16) | (texture_B << 8) | (default_alpha);
#endif
}

IUINT32 shader_pixel_texture_lambert_light_shadow(device_t* device, vertex_t* vertex)
{

//	IUINT32 texture_R = 0;
//	IUINT32 texture_G = 0;
//	IUINT32 texture_B = IUINT32(255.0f * vertex->vs_result[0].z);
//
//#ifdef USE_GDI_VIEW
//	return (texture_R << 16) | (texture_G << 8) | (texture_B);
//#else
//	return (texture_R << 24) | (texture_G << 16) | (texture_B << 8) | (default_alpha);
//#endif


	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	IUINT32 depth = device_texture_read(device,vertex->vs_result[0].x / device->framebuffer_width,vertex->vs_result[0].y / device->framebuffer_height,device->texture_id[1]);
	float fDepth = (depth >> 8) / 255.0f;
	float fShadow = 1.0f;
	if (fDepth > 0.0f)
	{
		if (vertex->vs_result[0].z >= (fDepth + 0.01f))
		{
			fShadow = 0.6f;
		}
	}

	vector_t normal = vertex->normal;
	vector_t direction = device->uniform_vector[1];
	vector_normalize(&direction);
	vector_normalize(&normal);

	matrix_t normal_world;
	matrix_transpose(&(device->transform.worldInv), &normal_world);
	vector_t cnormal;
	matrix_apply(&cnormal, &normal, &(normal_world));

	IUINT32 cc = device_texture_read(device, u, v, device->texture_id[0]);
	float diffuse = vector_dotproduct(&direction, &cnormal);
	if (diffuse >= 0.001)
	{
		IUINT32 texture_R = Get_R(cc);
		IUINT32 texture_G = Get_G(cc);
		IUINT32 texture_B = Get_B(cc);

		vector_t energy = device->uniform_vector[0];
		IUINT32 diffuse_R = (IUINT32)(texture_R * diffuse * energy.x * fShadow);
		IUINT32 diffuse_G = (IUINT32)(texture_G * diffuse * energy.y * fShadow);
		IUINT32 diffuse_B = (IUINT32)(texture_B * diffuse * energy.z * fShadow);

		diffuse_R = CMID(diffuse_R, 0, 255);
		diffuse_G = CMID(diffuse_G, 0, 255);
		diffuse_B = CMID(diffuse_B, 0, 255);

#ifdef USE_GDI_VIEW
		return (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
		return (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
	}
	else
	{
#ifdef USE_GDI_VIEW
		return 0;
#else
		return default_alpha;
#endif
	}
}

IUINT32 shader_pixel_texture_blinn_light(device_t* device, vertex_t* vertex)
{
	float w = 1.0f / vertex->rhw;

	float u = vertex->tc.u * w;
	float v = vertex->tc.v * w;

	vector_t normal = vertex->normal;
	vector_t direction = device->uniform_vector[1];
	vector_normalize(&direction);
	vector_normalize(&normal);

	matrix_t normal_world;
	matrix_transpose(&(device->transform.worldInv), &normal_world);
	vector_t cnormal;
	matrix_apply(&cnormal, &normal, &(normal_world));
	vector_normalize(&cnormal);

	IUINT32 cc = device_texture_read(device, u, v, device->texture_id[0]);
	float diffuse = vector_dotproduct(&direction, &cnormal);
	if (diffuse >= 0.001)
	{
		IUINT32 texture_R = Get_R(cc);
		IUINT32 texture_G = Get_G(cc);
		IUINT32 texture_B = Get_B(cc);

		vector_t energy = device->uniform_vector[0];
		IUINT32 diffuse_R = (IUINT32)(texture_R * diffuse * energy.x);
		IUINT32 diffuse_G = (IUINT32)(texture_G * diffuse * energy.y);
		IUINT32 diffuse_B = (IUINT32)(texture_B * diffuse * energy.z);

		vector_t eye_view = vertex->vs_result[0];
		vector_normalize(&eye_view);

		vector_t halfvecDir;
		vector_add(&halfvecDir, &eye_view, &direction);
		vector_normalize(&halfvecDir);

		vector_t matrial = device->uniform_vector[3];
		float kD = matrial.x;
		float kS = matrial.y;
		float kQ = matrial.z;

		float spec = vector_dotproduct(&halfvecDir, &cnormal);
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
			return (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
			return (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
		}
		else
		{
			diffuse_R = CMID(diffuse_R * kD, 0, 255);
			diffuse_G = CMID(diffuse_G * kD, 0, 255);
			diffuse_B = CMID(diffuse_B * kD, 0, 255);

#ifdef USE_GDI_VIEW
			return (diffuse_R << 16) | (diffuse_G << 8) | (diffuse_B);
#else
			return (diffuse_R << 24) | (diffuse_G << 16) | (diffuse_B << 8) | (default_alpha);
#endif
		}
	}
	else
	{
#ifdef USE_GDI_VIEW
		return 0;
#else
		return 255;
#endif
	}
}

RenderComponent g_ShaderComponent[MAX_SHADER_STATE] = {
	{ SHADER_STATE_WIREFRAME, shader_vertex_normal_mvp, NULL },
	{ SHADER_STATE_TEXTURE, shader_vertex_normal_mvp, shader_pixel_normal_texture },
	{ SHADER_STATE_COLOR, shader_vertex_normal_mvp, shader_pixel_normal_color },
	{ SHADER_STATE_LAMBERT_LIGHT_TEXTURE, shader_vertex_normal_mvp, shader_pixel_texture_lambert_light },
	{ SHADER_STATE_PHONG_LIGHT_TEXTURE, shader_vertex_phong_mvp, shader_pixel_texture_phong_light },
	{ SHADER_STATE_TEXTURE_ALPHA , shader_vertex_normal_mvp, shader_pixel_normal_texture_alpha },
	{ SHADER_STATE_SHADOW_MAP, shader_vertex_normal_mvp, shader_pixel_shadow_map },
	{ SHADER_STATE_LIGHT_SHADOW, shader_vertex_shadow_map_mvp, shader_pixel_texture_lambert_light_shadow },
	{ SHADER_STATE_BLINN_LIGHT_TEXTURE , shader_vertex_blinn_mvp, shader_pixel_texture_phong_light},
};

func_pixel_shader get_pixel_shader(device_t* device)
{
	int i;
	for (i = 0; i < MAX_SHADER_STATE; i++)
	{
		if (device->shader_state == g_ShaderComponent[i].RenderState)
		{
			return g_ShaderComponent[i].p_pixel_shader;
		}
	}

	return NULL;
}

func_vertex_shader get_vertex_shader(device_t* device)
{
	int i;
	for (i = 0; i < MAX_SHADER_STATE; i++)
	{
		if (device->shader_state == g_ShaderComponent[i].RenderState)
		{
			return g_ShaderComponent[i].p_vertex_shader;
		}
	}

	return NULL;
}