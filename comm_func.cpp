#include "comm_func.h"

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