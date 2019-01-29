#pragma once

#define RENDER_STATE_WIREFRAME      1		// ��Ⱦ�߿�
#define RENDER_STATE_TEXTURE        2		// ��Ⱦ����
#define RENDER_STATE_COLOR          4		// ��Ⱦ��ɫ
#define RENDER_STATE_LAMBERT_LIGHT_TEXTURE 8 // �����ع���
#define RENDER_STATE_PHONG_LIGHT_TEXTURE 16 // PHONG����
#define RENDER_STATE_TEXTURE_ALPHA	32 //͸��ģʽ
#define RENDER_STATE_SHADOW_MAP	64 //��Ӱģʽ
#define RENDER_STATE_BLINN_LIGHT_TEXTURE 128 //Blinn����

#define SHADER_STATE_WIREFRAME      1		// ��Ⱦ�߿�
#define SHADER_STATE_TEXTURE        2		// ��Ⱦ����
#define SHADER_STATE_COLOR          4		// ��Ⱦ��ɫ
#define SHADER_STATE_LAMBERT_LIGHT_TEXTURE 8 // �����ع���
#define SHADER_STATE_PHONG_LIGHT_TEXTURE 16 // PHONG����
#define SHADER_STATE_TEXTURE_ALPHA	32 //͸��ģʽ
#define SHADER_STATE_SHADOW_MAP	64 //��Ӱģʽ
#define SHADER_STATE_LIGHT_SHADOW 128 //������Ӱ
#define SHADER_STATE_BLINN_LIGHT_TEXTURE 256 //Blinn����

//#define USE_GDI_VIEW

#define WINDOW_SIZE 512
#define MAX_RENDER_STATE 8
#define MAX_SHADER_STATE 9

#define MOVE_NEAR 0
#define MOVE_FAR 1
#define ROTATE_LEFT 2
#define ROTATE_RIGHT 3
#define CHANGE_MODE	4