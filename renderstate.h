#pragma once

#define RENDER_STATE_WIREFRAME      1		// 渲染线框
#define RENDER_STATE_TEXTURE        2		// 渲染纹理
#define RENDER_STATE_COLOR          4		// 渲染颜色
#define RENDER_STATE_LAMBERT_LIGHT_TEXTURE 8 // 兰伯特光照
#define RENDER_STATE_PHONG_LIGHT_TEXTURE 16 // PHONG光照
#define RENDER_STATE_TEXTURE_ALPHA	32 //透明模式
#define RENDER_STATE_SHADOW_MAP	64 //阴影模式

#define SHADER_STATE_WIREFRAME      1		// 渲染线框
#define SHADER_STATE_TEXTURE        2		// 渲染纹理
#define SHADER_STATE_COLOR          4		// 渲染颜色
#define SHADER_STATE_LAMBERT_LIGHT_TEXTURE 8 // 兰伯特光照
#define SHADER_STATE_PHONG_LIGHT_TEXTURE 16 // PHONG光照
#define SHADER_STATE_TEXTURE_ALPHA	32 //透明模式
#define SHADER_STATE_SHADOW_MAP	64 //阴影模式
#define SHADER_STATE_LIGHT_SHADOW 128 //光照阴影

//#define USE_GDI_VIEW

#define WINDOW_SIZE 512
#define MAX_RENDER_STATE 7
#define MAX_SHADER_STATE 8

#define MOVE_NEAR 0
#define MOVE_FAR 1
#define ROTATE_LEFT 2
#define ROTATE_RIGHT 3
#define CHANGE_MODE	4