#pragma once
#include "GLEW/glew.h"
#ifdef _MSC_VER
    #include "FreeGLUT/freeglut.h"
    #include <direct.h>
#else
    #include <OpenGL/gl3.h>
    #include <GLUT/glut.h>
    #include <unistd.h>
#endif

#include "TinyOBJ/tiny_obj_loader.h"
#include "STB/stb_image.h"
#include "utils/glutils.hpp"
#include "utils/timer.hpp"
#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#ifdef _MSC_VER
    #pragma comment (lib, "glew32.lib")
	#pragma comment(lib, "freeglut.lib")
    #pragma comment(lib, "assimp.lib")
#endif

#define GLM_SWIZZLE
#include "GLM/glm/glm.hpp"
#include "GLM/glm/gtc/matrix_transform.hpp"
#include "GLM/glm/gtc/type_ptr.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>

#ifdef _MSC_VER
	#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
    #define __FILEPATH__(x) ((std::string(__FILE__).substr(0, std::string(__FILE__).rfind('\\'))+(x)).c_str())
#else
	#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define __FILEPATH__(x) ((std::string(__FILE__).substr(0, std::string(__FILE__).rfind('/'))+(x)).c_str())
#endif

#ifndef _DEG2RAD
#define _DEG2RAD
    #define deg2rad(x) ((x)*((M_PI)/(180.0f)))
#endif

#define deg2rad(x) ((x)*((3.1415926f)/(180.0f)))

// Print OpenGL context related information.
void dumpInfo(void)
{
    printf("Vendor: %s\n", glGetString (GL_VENDOR));
    printf("Renderer: %s\n", glGetString (GL_RENDERER));
    printf("Version: %s\n", glGetString (GL_VERSION));
    printf("GLSL: %s\n", glGetString (GL_SHADING_LANGUAGE_VERSION));
}

void shaderLog(GLuint shader)
{
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        
        // The maxLength includes the NULL character
        GLchar* errorLog = new GLchar[maxLength];
        glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);
        
        printf("%s\n", errorLog);
        delete[] errorLog;
    }
}

void printGLError()
{
    GLenum code = glGetError();
    switch(code)
    {
        case GL_NO_ERROR:
            std::cout << "GL_NO_ERROR" << std::endl;
            break;
        case GL_INVALID_ENUM:
            std::cout << "GL_INVALID_ENUM" << std::endl;
            break;
        case GL_INVALID_VALUE:
            std::cout << "GL_INVALID_VALUE" << std::endl;
            break;
        case GL_INVALID_OPERATION:
            std::cout << "GL_INVALID_OPERATION" << std::endl;
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            std::cout << "GL_INVALID_FRAMEBUFFER_OPERATION" << std::endl;
            break;
        case GL_OUT_OF_MEMORY:
            std::cout << "GL_OUT_OF_MEMORY" << std::endl;
            break;
        case GL_STACK_UNDERFLOW:
            std::cout << "GL_STACK_UNDERFLOW" << std::endl;
            break;
        case GL_STACK_OVERFLOW:
            std::cout << "GL_STACK_OVERFLOW" << std::endl;
            break;
        default:
            std::cout << "GL_ERROR" << std::endl;
    }
}

typedef struct _texture_data
{
    _texture_data() : width(0), height(0), data(0) {}
    int width;
    int height;
    unsigned char* data;
} texture_data;

texture_data load_png(const char* path)
{
    texture_data texture;
    int n;
    stbi_uc *data = stbi_load(path, &texture.width, &texture.height, &n, 4);
    if(data != NULL)
    {
        texture.data = new unsigned char[texture.width * texture.height * 4 * sizeof(unsigned char)];
        memcpy(texture.data, data, texture.width * texture.height * 4 * sizeof(unsigned char));
        // vertical-mirror image data
        for (size_t i = 0; i < texture.width; i++)
        {
            for (size_t j = 0; j < texture.height / 2; j++)
            {
                for(size_t k = 0; k < 4; k++) {
                    std::swap(texture.data[(j * texture.width + i) * 4 + k], texture.data[((texture.height - j - 1) * texture.width + i) * 4 + k]);
                }
            }
        }
        stbi_image_free(data);
    }
    return texture;
}

