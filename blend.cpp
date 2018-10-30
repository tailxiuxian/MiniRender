#include "blend.h"
#include "comm_func.h"

bool is_opaque_pixel_color(IUINT32 color)
{
#ifdef USE_GDI_VIEW
	return true;
#else
	return Get_A(color) == 255;
#endif
}

IUINT32 blend_frame_buffer_color(device_t *device, IUINT32 srcColor, IUINT32 dstColor)
{
	float alphaSrc = (float)Get_A(srcColor) / 255.0f;
	float alphaDst = (float)Get_A(dstColor) / 255.0f;

	color_t srcFactor, dstFactor;

	if (device->blend_state.srcState == BLEND_ZERO)
	{
		srcFactor = { 0.0f,0.0f,0.0f,0.0f };
	}
	else if (device->blend_state.srcState == BLEND_ONE)
	{
		srcFactor = { 1.0f,1.0f,1.0f,1.0f };
	}
	else if (device->blend_state.srcState == BLEND_SRC_ALPHA)
	{
		srcFactor = { alphaSrc, alphaSrc, alphaSrc, alphaSrc };
	}
	else if (device->blend_state.srcState == BLEND_ONE_MINUS_SRC_ALPHA)
	{
		srcFactor = { 1.0f - alphaSrc, 1.0f - alphaSrc, 1.0f - alphaSrc, 1.0f - alphaSrc };
	}

	if (device->blend_state.srcState == BLEND_ZERO)
	{
		dstFactor = { 0.0f,0.0f,0.0f,0.0f };
	}
	else if (device->blend_state.srcState == BLEND_ONE)
	{
		dstFactor = { 1.0f,1.0f,1.0f,1.0f };
	}
	else if (device->blend_state.srcState == BLEND_SRC_ALPHA)
	{
		dstFactor = { alphaSrc, alphaSrc, alphaSrc, alphaSrc };
	}
	else if (device->blend_state.srcState == BLEND_ONE_MINUS_SRC_ALPHA)
	{
		dstFactor = { 1.0f - alphaSrc, 1.0f - alphaSrc, 1.0f - alphaSrc, 1.0f - alphaSrc };
	}

	IUINT32 Rsrc = Get_R(srcColor);
	IUINT32 Gsrc = Get_G(srcColor);
	IUINT32 Bsrc = Get_B(srcColor);
	IUINT32 Asrc = Get_A(srcColor);

	IUINT32 Rdst = Get_R(dstColor);
	IUINT32 Gdst = Get_G(dstColor);
	IUINT32 Bdst = Get_B(dstColor);
	IUINT32 Adst = Get_A(dstColor);

	IUINT32 Rframe = Rsrc * srcFactor.r + Rdst * dstFactor.r;
	IUINT32 Gframe = Gsrc * srcFactor.g + Gdst * dstFactor.g;
	IUINT32 Bframe = Bsrc * srcFactor.b + Bdst * dstFactor.b;
	IUINT32 Aframe = Asrc * srcFactor.a + Adst * dstFactor.a;

	return (Rframe << 24) | (Gframe << 16) | (Bframe << 8) | (Aframe);
}