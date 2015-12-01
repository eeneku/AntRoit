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
#include <glm\gtc\type_ptr.hpp>
#include <glm\gtx\transform.hpp>

#include <Box2D\Box2D.h>

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

#pragma region Globals


static const float Scale = 32.0f;

b2Vec2 worldToBox2D(float x, float y)
{
	return b2Vec2(x / Scale, y / Scale);
}

b2Vec2 worldToBox2D(glm::vec2 vec) {
	vec.x /= Scale;
	vec.y /= Scale;
	return b2Vec2(vec.x, vec.y);
}

float worldToBox2D(float f) {
	return f / Scale;
}

glm::vec2 box2DToWorld(float x, float y) {
	return glm::vec2(x * Scale, y * Scale);
}

glm::vec2 box2DToWorld(b2Vec2 vec) {
	vec.x *= Scale;
	vec.y *= Scale;
	return glm::vec2(vec.x, vec.y);
}

float box2DToWorld(float f) {
	return f * Scale;
}

GLuint program;
glm::mat4 projection;
b2World world(b2Vec2(0.0f, worldToBox2D(500.0f)));


#pragma endregion

#pragma region Shaders

static const char vertexShader[] =
"attribute vec4 position;\n"
"uniform mat4 MVP;\n"
"void main()\n"
"{\n"
"	gl_Position = MVP * position;\n"
"}\n";

static const char fragmentShader[] =
"precision mediump float;\n"
"uniform vec4 color;\n"
"void main()\n"
"{\n"
"	gl_FragColor = color;\n"
"}\n";

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

struct Shape
{
	Shape(float x, float y, const glm::vec4& color, b2World& world) : color(color), x(x), y(y), vertices(nullptr), numVertices(0), model(1.0f), world(world)
	{

	}

	~Shape()
	{
		delete[] vertices;
		vertices = nullptr;

		world.DestroyBody(body);
		body = nullptr;
	}


	void draw()
	{
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);

		glUniform4fv(glGetUniformLocation(program, "color"), 1, glm::value_ptr(color));
		model = glm::translate(glm::vec3(box2DToWorld(body->GetPosition().x), box2DToWorld(body->GetPosition().y), 0.0f)) * glm::rotate(body->GetAngle(), glm::vec3(0.0f, 0.0f, 1.0f));
		glUniformMatrix4fv(glGetUniformLocation(program, "MVP"), 1, GL_FALSE, glm::value_ptr(projection * model));
		checkGlError("glVertexAttribPointer");

		glDrawArrays(GL_TRIANGLES, 0, numVertices);
		checkGlError("glDrawArrays");
	}

protected:

	glm::mat4 model;
	glm::vec4 color;
	b2World& world;
	b2Body* body;
	float x;
	float y;
	float* vertices;
	int numVertices;
};

struct Triangle : public Shape
{
	Triangle(float x, float y, float width, float height, const glm::vec4& color, b2World &world, bool dynamic) : Shape(x, y, color, world)
	{
		vertices = new float[6] {
			-width / 2, height / 2,
			width / 2, height / 2,
			-width / 2, -height / 2 
		};

		numVertices = 6;

		b2BodyDef bodyDef;
		bodyDef.position = worldToBox2D(x, y);;
		
		bodyDef.type = dynamic ? b2_dynamicBody : b2_staticBody;
		body = world.CreateBody(&bodyDef);

		b2PolygonShape shape;
		b2Vec2 shapetices[3];

		shapetices[0].Set(-worldToBox2D(width / 2), worldToBox2D(height / 2));
		shapetices[1].Set(worldToBox2D(width / 2), worldToBox2D(height / 2));
		shapetices[2].Set(-worldToBox2D(width / 2), -worldToBox2D(height / 2));

		shape.Set(shapetices, 3);

		b2FixtureDef fixtureDef;
		fixtureDef.friction = 1.0f;
		fixtureDef.density = 1.0f;
		fixtureDef.shape = &shape;

		body->CreateFixture(&fixtureDef);
	}
};

struct Rectangle : public Shape
{
	Rectangle(float x, float y, float width, float height, const glm::vec4& color, b2World &world, bool dynamic) : Shape(x, y, color, world)
	{
		vertices = new float[12] {
			-width / 2,	height/2,
			width / 2,  height / 2,
			-width / 2, -height / 2,

			width / 2,  height / 2,
			width / 2,  -height / 2,
			-width / 2, -height / 2
		};

		numVertices = 12;

		b2BodyDef bodyDef;
		bodyDef.position = worldToBox2D(x, y);;
		bodyDef.type = dynamic ? b2_dynamicBody : b2_staticBody;
		body = world.CreateBody(&bodyDef);

		b2PolygonShape shape;
		shape.SetAsBox(worldToBox2D(width / 2), worldToBox2D(height / 2));

		b2FixtureDef fixtureDef;
		fixtureDef.friction = 1.0f;
		fixtureDef.density = 1.0f;
		fixtureDef.shape = &shape;

		body->CreateFixture(&fixtureDef);
	}
};

std::vector<Shape*> shapes;
std::vector<b2Body*> walls;

float createWall(float x, float y, float width, float height)
{
	b2Body* body;
	b2BodyDef bodyDef;
	bodyDef.position = worldToBox2D(x, y);;
	bodyDef.type = b2_staticBody;
	body = world.CreateBody(&bodyDef);

	b2PolygonShape shape;
	shape.SetAsBox(worldToBox2D(width), worldToBox2D(height));

	b2FixtureDef fixtureDef;
	fixtureDef.shape = &shape;

	body->CreateFixture(&fixtureDef);

	walls.push_back(body);
}

float createWalls(int width, int height)
{
	// Top
	createWall(0.0f, -5.0f, width, 10.0f);

	// Bottom
	createWall(0.0f, height + 5.0f, width, 10.0f);

	// Left
	createWall(-5.0f, 0.0f, 10.0f, height);

	// Right
	createWall(width + 5.0f, 0.0f, 10.0f, height);
}

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

	glViewport(0, 0, width, height);
	checkGlError("glViewport");

	glClearColor(0.7f, 0.3f, 0.1f, 1.0f);
	checkGlError("glClearColor");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	projection = glm::ortho(0.0f, float(width), float(height), 0.0f);

	createWalls(width, height);

	shapes.push_back(new Rectangle(150.0f, 50.0f, 100.0f, 100.0f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), world, true));
	shapes.push_back(new Triangle(350.0f, 250.0f, 500.0f, 500.0f, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), world, true));
	shapes.push_back(new Triangle(500.0f, 1000.0f, 100.0f, 100.0f, glm::vec4(0.0f, 0.0f, 1.0f, 0.5f), world, false));
	shapes.push_back(new Rectangle(600.0f, 1200.0f, 500.0f, 50.0f, glm::vec4(1.0f, 1.0f, 0.0f, 0.8f), world, false));
}

#pragma endregion

#pragma region Update

void update()
{
	world.Step(1.0f / 60.0f, 8, 3);
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
	JNIEXPORT void JNICALL Java_fi_enko_antroit_AntRoitLib_init(JNIEnv* env, jobject obj, jint width, jint height)
	{
		init(width, height);
	}

	JNIEXPORT void JNICALL Java_fi_enko_antroit_AntRoitLib_step(JNIEnv* env, jobject obj)
	{
		update();
		draw();
	}
}

#pragma endregion