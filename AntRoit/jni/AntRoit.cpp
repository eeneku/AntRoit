/*
* Copyright (C) 2009 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

// OpenGL ES 2.0 code

#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <vector>
#include <glm\glm.hpp>

#pragma region GL debug

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static void printGLString(const char *name, GLenum s) 
{
	const char *v = (const char *)glGetString(s);
	LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) 
{
	for (GLint error = glGetError(); error; error
		= glGetError()) {
		LOGI("after %s() glError (0x%x)\n", op, error);
	}
}

#pragma endregion

#pragma region Shaders

static const char vertexShader[] =
"attribute vec4 position;\n"
"void main()\n"
"{\n"
"	gl_Position = position;\n"
"}\n";

static const char fragmentShader[] =
"precision mediump float;\n"
"void main()\n"
"{\n"
"	gl_FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
"}\n";

GLuint program;
GLuint positionHandle;

#pragma endregion

#pragma region Shader loading

GLuint loadShader(GLenum shaderType, const char* pSource) 
{
	GLuint shader = glCreateShader(shaderType);
	if (shader)
	{
		glShaderSource(shader, 1, &pSource, NULL);
		glCompileShader(shader);
		GLint compiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (!compiled) 
		{
			GLint infoLen = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
			if (infoLen)
			{
				char* buf = (char*)malloc(infoLen);
				if (buf) 
				{
					glGetShaderInfoLog(shader, infoLen, NULL, buf);
					LOGE("Could not compile shader %d:\n%s\n",
						shaderType, buf);
					free(buf);
				}
				glDeleteShader(shader);
				shader = 0;
			}
		}
	}
	return shader;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) 
{
	GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
	if (!vertexShader) return 0;

	GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);
	if (!pixelShader) return 0;

	GLuint program = glCreateProgram();
	if (program) 
	{
		glAttachShader(program, vertexShader);
		checkGlError("glAttachShader");
		glAttachShader(program, pixelShader);
		checkGlError("glAttachShader");
		glLinkProgram(program);
		GLint linkStatus = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (linkStatus != GL_TRUE)
		{
			GLint bufLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
			if (bufLength) 
			{
				char* buf = (char*)malloc(bufLength);
				if (buf) 
				{
					glGetProgramInfoLog(program, bufLength, NULL, buf);
					LOGE("Could not link program:\n%s\n", buf);
					free(buf);
				}
			}
			glDeleteProgram(program);
			program = 0;
		}
	}
	return program;
}

#pragma endregion

#pragma region Shapes

const float rectangle[] =
{
	0.0f, 1.0f,
	1.0f, 1.0f,
	0.0f, 0.0f,

	1.0f, 1.0f,
	1.0f, 0.0f,
	0.0f, 0.0f
};

const float triangle[] =
{
	0.0f, 1.0f,
	1.0f, 1.0f,
	0.0f, 0.0f
};

struct Shape
{
	virtual void draw() = 0;

protected:
	glm::vec4 color;
	float x;
	float y;
};

struct Triangle : public Shape
{
	void draw()
	{
		glVertexAttribPointer(positionHandle, 2, GL_FLOAT, GL_FALSE, 0, triangle);
		checkGlError("glVertexAttribPointer");

		glDrawArrays(GL_TRIANGLES, 0, 3);
		checkGlError("glDrawArrays");
	}
};

struct Rectangle : public Shape
{
	void draw()
	{
		glVertexAttribPointer(positionHandle, 2, GL_FLOAT, GL_FALSE, 0, rectangle);
		checkGlError("glVertexAttribPointer");

		glDrawArrays(GL_TRIANGLES, 0, 6);
		checkGlError("glDrawArrays");
	}
};

std::vector<Shape*> shapes;

#pragma endregion

#pragma region Init

void init(int width, int height)
{
	printGLString("Version", GL_VERSION);
	printGLString("Vendor", GL_VENDOR);
	printGLString("Renderer", GL_RENDERER);
	printGLString("Extensions", GL_EXTENSIONS);

	LOGI("setupGraphics(%d, %d)", width, height);

	program = createProgram(vertexShader, fragmentShader);
	if (!program)
	{
		LOGE("Could not create program.");
		return;
	}

	positionHandle = glGetAttribLocation(program, "position");
	checkGlError("glGetAttribLocation");
	LOGI("glGetAttribLocation(\"position\") = %d\n", positionHandle);

	glViewport(0, 0, width, height);
	checkGlError("glViewport");

	glClearColor(0.7f, 0.3f, 0.1f, 1.0f);
	checkGlError("glClearColor");
}

#pragma endregion

#pragma region Update

void update()
{
}

#pragma endregion

#pragma region Draw

void draw()
{
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	checkGlError("glClear");

	glUseProgram(program);
	checkGlError("glUseProgram");

	for (auto shape : shapes)
	{
		shape->draw();
	}
}

#pragma endregion

#pragma region Java

extern "C"
{
	JNIEXPORT void JNICALL Java_fi_enko_antroit_AntRoitLib_init(JNIEnv * env, jobject obj, jint width, jint height)
	{
		init(width, height);
	}

	JNIEXPORT void JNICALL Java_fi_enko_antroit_AntRoitLib_step(JNIEnv * env, jobject obj)
	{
		update();
		draw();
	}
}

#pragma endregion