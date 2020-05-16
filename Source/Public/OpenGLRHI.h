#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define CHECK_GL_ERROR() \
{\
	int error = glGetError(); \
	if (error!=0) \
		std::cout << error << "in line " << __LINE__ << std::endl; \
}