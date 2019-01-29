#include<iostream>

#define GLEW_STATIC

#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "renderstate.h"
#include "device.h"
#include <vector>

extern void set_key_state(int keyCode, int value);
extern void set_key_quit();

using namespace std;

const char* VSShader = "#version 330\n"\
"layout(location = 0) in vec4 position;\n"\
"layout(location = 1) in vec2 tex_coord;\n"\
"out vec2 vert_tex_coord;\n"\
"void main()"\
"{\n"\
"gl_Position = position;\n"\
"vert_tex_coord = tex_coord;\n"\
"}\n";

const GLchar *PSShader = "#version 330\n"\
"in vec2 vert_tex_coord;\n"\
"out vec4 out_color;\n"\
"uniform sampler2D CC_Texture0;\n"\
"void main()\n"\
"{\n"\
"out_color = texture2D(CC_Texture0, vert_tex_coord);\n"\
"}\n";

#define CHECK_GL_ERROR_DEBUG() \
    do { \
        GLenum __error = glGetError(); \
        if(__error) { \
            printf("OpenGL error 0x%04X in %s %s %d\n", __error, __FILE__, __FUNCTION__, __LINE__); \
        } \
    } while (false)

static const GLfloat quad_data[] = {
	-1.0f, -1.0f, 0.0f, 1.0f,
	1.0f, -1.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f, 0.0f,

	1.0f, 1.0f, 1.0f, 0.0f,
	-1.0f, 1.0f, 0.0f, 0.0f,
	-1.0f, -1.0f, 0.0f, 1.0f,
};

static GLFWwindow* gl_window;
static GLuint gl_programm;
static GLuint gl_texture;
static unsigned char* gl_texture_data = NULL;
static GLuint gl_vao;
static GLuint gl_vbo;
static int texture_size_w = 0;
static int texture_size_h = 0;

GLFWwindow* initGLBase(int w, int h)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	GLFWwindow* window = glfwCreateWindow(w, h, "LearnOpenGL", nullptr, nullptr);
	if (window == nullptr)
	{
		cout << "Failed to create GLFW window" << endl;
		glfwTerminate();
		return NULL;
	}

	glfwMakeContextCurrent(window);

	glewInit();
	if (GLEW_ARB_vertex_shader && GLEW_ARB_fragment_shader)
	{
		printf("Ready For OpenGL 2.0\n");
	}
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
	return window;
}

void init_texture(unsigned char* texture) {
	int i, j;
	for (j = 0; j < texture_size_h; j++) {
		for (i = 0; i < texture_size_w; i++) {
			int x = i / 32, y = j / 32;
			unsigned char* pData = &texture[j * texture_size_w * 4 + i * 4];
			if ((x + y) & 1)
			{
				*(pData) = 0xff;
				*(pData + 1) = 0x00;
				*(pData + 2) = 0x00;
				*(pData + 3) = 0xff;
			}
			else
			{
				*(pData) = 0x00;
				*(pData + 1) = 0xff;
				*(pData + 2) = 0x00;
				*(pData + 3) = 0xff;
			}
		}
	}
}

GLuint init_gl_texture() {
	if (gl_texture_data != NULL)
	{
		return 0;
	}

	int texture_mem_size = texture_size_w * texture_size_h * 4;
	gl_texture_data = new unsigned char[texture_mem_size];
	memset(gl_texture_data, 0, texture_mem_size);
	init_texture(gl_texture_data);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)texture_size_w, (GLsizei)texture_size_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, gl_texture_data);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

GLuint complileShader(GLenum shaderType, const char* shaderSource)
{
	GLuint shader;
	shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &shaderSource, NULL);
	glCompileShader(shader);
	GLint compileStatus = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus); // 检查编译状态
	if (compileStatus == GL_FALSE) // 获取错误报告
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
		std::vector<GLchar> errLog(maxLength);
		glGetShaderInfoLog(shader, maxLength, &maxLength, &errLog[0]);
		std::cout << "Error::Shader compiled failed," << &errLog[0] << std::endl;
	}
	return shader;
}

static void notify_key_state(int keyCode, int value)
{
	if (keyCode == GLFW_KEY_UP)
	{
		set_key_state(MOVE_NEAR, value);
	}
	else if (keyCode == GLFW_KEY_DOWN)
	{
		set_key_state(MOVE_FAR, value);
	}
	else if (keyCode == GLFW_KEY_LEFT)
	{
		set_key_state(ROTATE_LEFT, value);
	}
	else if (keyCode == GLFW_KEY_RIGHT)
	{
		set_key_state(ROTATE_RIGHT, value);
	}
	else if (keyCode == GLFW_KEY_SPACE)
	{
		set_key_state(CHANGE_MODE, value);
	}
}

static void dispatch_key_event(int keyCode)
{
	device_t* device = get_device_inst();

	switch (keyCode) {
	case GLFW_KEY_F1:
	{
		if (device->function_state & FUNC_STATE_CULL_BACK)
		{
			device_disable_render_func_state(device, FUNC_STATE_CULL_BACK);
		}
		else
		{
			device_enable_render_func_state(device, FUNC_STATE_CULL_BACK);
		}

		break;
	}
	case GLFW_KEY_F2:
		if (device->function_state & FUNC_STATE_ANTI_ALIAS_FSAA)
		{
			device_disable_render_func_state(device, FUNC_STATE_ANTI_ALIAS_FSAA);
		}
		else
		{
			device_enable_render_func_state(device, FUNC_STATE_ANTI_ALIAS_FSAA);
		}
		break;
	case GLFW_KEY_ESCAPE:
	{
		set_key_quit();
		break;
	}

	default:
		break;
	}
}

static void onGLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		notify_key_state(key, 1);
	}
	else if (action == GLFW_RELEASE)
	{
		notify_key_state(key, 0);
		dispatch_key_event(key);
	}
}

void initGLView(int w, int h)
{
	texture_size_w = w;
	texture_size_h = h;

	gl_window = initGLBase(w,h);
	if (gl_window == NULL)
	{
		return;
	}

	gl_texture = init_gl_texture();

	GLuint gl_fragshader = complileShader(GL_FRAGMENT_SHADER, PSShader);
	GLuint gl_vertshader = complileShader(GL_VERTEX_SHADER, VSShader);
	gl_programm = glCreateProgram();
	glAttachShader(gl_programm, gl_vertshader);
	glAttachShader(gl_programm, gl_fragshader);
	glLinkProgram(gl_programm);

	
	glGenVertexArrays(1, &gl_vao);
	glBindVertexArray(gl_vao);

	glGenBuffers(1, &gl_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_data), quad_data, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (GLvoid*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glfwSetKeyCallback(gl_window, onGLFWKeyCallback);
}

void destroyGLView()
{
	glDeleteVertexArrays(1, &gl_vao);
	glDeleteBuffers(1, &gl_vbo);
	glfwTerminate();
}

void drawGLView()
{
	glfwPollEvents();

	glDisable(GL_DEPTH_TEST);
	glDepthMask(false);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindVertexArray(gl_vao);
	glUseProgram(gl_programm);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl_texture);
	glUniform1i(glGetUniformLocation(gl_programm, "CC_Texture0"), 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindVertexArray(0);
	glUseProgram(0);

	glfwSwapBuffers(gl_window);
}

void updateFrameBufferData(device_t* device)
{
	for (int height = 0; height < texture_size_h; height++)
	{
		for (int width = 0; width < texture_size_w; width++)
		{
			unsigned int data = device_get_framebuffer_data(device, height, width);
			unsigned char* pPixel = &gl_texture_data[height * texture_size_w * 4 + width * 4];
			*(pPixel) = (data >> 24) & 0xFF;
			*(pPixel + 1) = (data >> 16) & 0xFF;
			*(pPixel + 2) = (data >> 8) & 0xFF;
			*(pPixel + 3) = data & 0xFF;
		}
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture_size_w, texture_size_h, GL_RGBA, GL_UNSIGNED_BYTE, gl_texture_data);
}

void drawGLTitle(device_t *device)
{
	string GLTitle;
	GLTitle += "GLView ";
	switch (device->render_state)
	{
	case RENDER_STATE_WIREFRAME:
	{
		GLTitle += "Line Mode ";
		break;
	}
	case RENDER_STATE_TEXTURE:
	{
		GLTitle += "Texture Mode ";
		break;
	}
	case RENDER_STATE_COLOR:
	{
		GLTitle += "Color Mode ";
		break;
	}
	case RENDER_STATE_LAMBERT_LIGHT_TEXTURE:
	{
		GLTitle += "Lambert Light Mode ";
		break;
	}
	case RENDER_STATE_PHONG_LIGHT_TEXTURE:
	{
		GLTitle += "Phong Light Mode ";
		break;
	}
	case RENDER_STATE_TEXTURE_ALPHA:
	{
		GLTitle += "Texture Alpha Mode ";
		break;
	}
	case RENDER_STATE_SHADOW_MAP:
	{
		GLTitle += "Shadow Map Mode ";
		break;
	}
	case RENDER_STATE_BLINN_LIGHT_TEXTURE:
	{
		GLTitle += "Blinn Light Mode ";
		break;
	}
	default:
		GLTitle += "No Find  Mode ";
		break;
	}

	if (device->function_state & FUNC_STATE_CULL_BACK)
	{
		GLTitle += " - culling back";
	}

	glfwSetWindowTitle(gl_window, GLTitle.c_str());
}