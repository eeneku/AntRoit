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

#include <random>

#include <Box2D\Box2D.h>

#pragma region GL debug

#define  LOG_TAG    "AntRoit"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define randomFloat std::uniform_real_distribution<float>

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

static const float Scale = 16.0f;

b2Vec2 worldToBox2D(float x, float y)
{
	return b2Vec2(x / Scale, y / Scale);
}

b2Vec2 worldToBox2D(glm::vec2 vec) 
{
	vec.x /= Scale;
	vec.y /= Scale;
	return b2Vec2(vec.x, vec.y);
}

float worldToBox2D(float f) 
{
	return f / Scale;
}

glm::vec2 box2DToWorld(float x, float y) 
{
	return glm::vec2(x * Scale, y * Scale);
}

glm::vec2 box2DToWorld(b2Vec2 vec) 
{
	vec.x *= Scale;
	vec.y *= Scale;
	return glm::vec2(vec.x, vec.y);
}

float box2DToWorld(float f) 
{
	return f * Scale;
}

GLuint program;
glm::mat4 projection;
b2World world(b2Vec2(0.0f, worldToBox2D(128.0f)));

#pragma endregion

#pragma region Shaders

static const char vertexShader[] =
"attribute vec2 position;\n"
"uniform mat4 MVP;\n"
"void main()\n"
"{\n"
"	gl_Position = MVP * vec4(position, 0.0, 1.0);\n"
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

	GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);

	if (!fragmentShader) return 0;

	GLuint program = glCreateProgram();

	if (program) 
	{
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);

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
	Shape(const glm::vec4& color, b2World& world) : 
		color(color), 
		model(1.0f), 
		world(world), 
		numVertices(0)
	{
		glGenBuffers(1, &VBO);
	}

	~Shape()
	{
		world.DestroyBody(body);
		body = nullptr;

		glDeleteBuffers(1, &VBO);
	}

	void draw()
	{
		glBindBuffer(GL_ARRAY_BUFFER, VBO);

		glEnableVertexAttribArray(0);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0);

		glm::mat4 model = glm::translate(glm::vec3(box2DToWorld(body->GetPosition().x), box2DToWorld(body->GetPosition().y), 0.0f));
		model *= glm::rotate(body->GetAngle(), glm::vec3(0.0f, 0.0f, 1.0f));

		glUniform4fv(glGetUniformLocation(program, "color"), 1, glm::value_ptr(color));
		glUniformMatrix4fv(glGetUniformLocation(program, "MVP"), 1, GL_FALSE, glm::value_ptr(projection * model));

		glDrawArrays(GL_TRIANGLES, 0, numVertices);

		glDisableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void setGravity(float gravity)
	{
		body->SetGravityScale(gravity);
	}

protected:

	glm::mat4 model;
	glm::vec4 color;
	b2World& world;
	b2Body* body;
	GLuint VBO;
	GLuint numVertices;
};

struct Triangle : public Shape
{
	Triangle(GLfloat x, GLfloat y, GLfloat width, GLfloat height, GLfloat rotation, const glm::vec4& color, b2World &world, bool dynamic) : Shape(color, world)
	{
		b2BodyDef bodyDef;
		bodyDef.position = worldToBox2D(x, y);;

		bodyDef.type = dynamic ? b2_dynamicBody : b2_staticBody;
		body = world.CreateBody(&bodyDef);

		b2PolygonShape shape;
		b2Vec2 shapetices[3];

		shapetices[0].Set(-worldToBox2D(width / 2.0f), worldToBox2D(height / 2.0f));
		shapetices[1].Set(worldToBox2D(width / 2.0f), worldToBox2D(height / 2.0f));
		shapetices[2].Set(-worldToBox2D(width / 2.0f), -worldToBox2D(height / 2.0f));

		shape.Set(shapetices, 3);

		b2FixtureDef fixtureDef;
		fixtureDef.friction = 1.0f;
		fixtureDef.density = 1.0f;
		fixtureDef.shape = &shape;

		body->CreateFixture(&fixtureDef);

		body->SetTransform(body->GetPosition(), rotation);

		numVertices = 3;

		GLfloat vertices[] = {
			-width / 2.0f, height / 2.0f,
			width / 2.0f, height / 2.0f,
			-width / 2.0f, -height / 2.0f,
		};

		glBindBuffer(GL_ARRAY_BUFFER, VBO);

		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
};

struct Rectangle : public Shape
{
	Rectangle(GLfloat x, GLfloat y, GLfloat width, GLfloat height, GLfloat rotation, const glm::vec4& color, b2World &world, bool dynamic) : Shape(color, world)
	{
		b2BodyDef bodyDef;
		bodyDef.position = worldToBox2D(x, y);
		bodyDef.type = dynamic ? b2_dynamicBody : b2_staticBody;
		body = world.CreateBody(&bodyDef);

		b2PolygonShape shape;
		shape.SetAsBox(worldToBox2D(width / 2.0f), worldToBox2D(height / 2.0f));

		b2FixtureDef fixtureDef;
		fixtureDef.density = 1.0f;
		fixtureDef.shape = &shape;

		body->CreateFixture(&fixtureDef);

		body->SetTransform(body->GetPosition(), rotation);

		numVertices = 6;

		GLfloat vertices[] {
			-width / 2.0f, height / 2.0f,
			width / 2.0f, height / 2.0f,
			-width / 2.0f, -height / 2.0f,

			width / 2.0f, height / 2.0f,
			width / 2.0f, -height / 2.0f,
			-width / 2.0f, -height / 2.0f
		};

		glBindBuffer(GL_ARRAY_BUFFER, VBO);

		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
};

std::vector<Shape*> shapes;
int screenWidth = 0;
int screenHeight = 0;
int tWidth = 0;
int tHeight = 0;
std::random_device device;
std::default_random_engine generator(device());

void createShape()
{
	float w = randomFloat(tWidth / 5.0f, tWidth)(generator);
	float h = randomFloat(tHeight / 5.0f, tHeight)(generator);
	float x = randomFloat(tWidth / 8.0f + w, screenWidth - tWidth / 4.0f - w)(generator);
	float y = randomFloat(tHeight / 8.0f + h, screenHeight - tHeight / 4.0f - h)(generator);
	float r = randomFloat(0.0f, 1.0f)(generator);
	float g = randomFloat(0.0f, 1.0f)(generator);
	float b = randomFloat(0.0f, 1.0f)(generator);
	float a = randomFloat(0.0f, 1.0f)(generator);
	float angle = randomFloat(0.0f, 360.0f)(generator);

	if(randomFloat(0.0f, 1.0f)(generator) > 0.49f)
		shapes.push_back(new Triangle(x, y, w, h, glm::radians(angle), glm::vec4(r, g, b, a), world, true));
	else
		shapes.push_back(new Rectangle(x, y, w, h, glm::radians(angle), glm::vec4(r, g, b, a), world, true));
}

void clearShapes()
{
	for (auto shape : shapes)
	{
		delete shape;
	}

	shapes.clear();
}

float createShapes(int width, int height)
{
	tWidth = (width > height) ? height / 5.0f : width / 5.0f;
	tHeight = tWidth;

	// Borders
	shapes.push_back(new Rectangle(width / 2.0f, tHeight / 8.0f, width, tHeight / 4.0f, glm::radians(0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), world, false));
	shapes.push_back(new Rectangle(width / 2.0f, height - tHeight / 8.0f, width, tHeight / 4.0f, glm::radians(0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), world, false));
	shapes.push_back(new Rectangle(tWidth / 8.0f, height / 2.0f, tWidth / 4.0f, height, glm::radians(0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), world, false));
	shapes.push_back(new Rectangle(width - tWidth / 8.0f, height / 2.0f, tWidth / 4.0f, height, glm::radians(0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), world, false));

	//shapes.push_back(new Triangle(width / 1.5f, tHeight / 2.0f, tWidth / 1.5f, tHeight * 2.0f, glm::radians(44.5f), glm::vec4(0.0f, 0.6f, 0.1f, 1.0f), world, true));
	//shapes.push_back(new Rectangle(0, height / 2.0f, tWidth * 4.0f, tHeight / 5.0f, glm::radians(15.0f), glm::vec4(0.0f, 0.4f, 0.2f, 1.0f), world, false));

	//shapes.push_back(new Rectangle(width / 2.0f, tHeight, tWidth / 4.0f, tHeight, glm::radians(44.5f), glm::vec4(1.0f, 0.0f, 0.8f, 1.0f), world, true));
	//shapes.push_back(new Rectangle(width / 3.0f, height / 2.0f, tWidth / 3.0f, tHeight / 3.0f, glm::radians(44.5f), glm::vec4(0.7f, 0.8f, 0.8f, 1.0f), world, true));
}

#pragma endregion

#pragma region Init graphics

void initGraphics(int width, int height)
{
	clearShapes();

	screenWidth = width;
	screenHeight = height;

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
	glClearColor(0.1f, 0.1f, 0.8f, 1.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);

	createShapes(width, height);
}

#pragma endregion

#pragma region Update

float currentTime = 0.0f;
float accumulator = 0.0f;
float step = 1.0f / 60.0f;
float spawnTime = 0.0f;
float backgroundFun = 0.0f;
float clearR = 0.2f;
float clearG = 0.3f;
float clearB = 0.5f;

void update(int time)
{
	float newTime = time / 1000.0f;

	float deltaTime = std::min(newTime - currentTime, 0.25f);
	currentTime = newTime;

	accumulator += deltaTime;

	while (accumulator >= step)
	{
		clearR += randomFloat(-0.001f, 0.001f)(generator);
		clearG += randomFloat(-0.001f, 0.001f)(generator);
		clearB += randomFloat(-0.001f, 0.001f)(generator);

		if (clearR > 1.0f) clearR = 1.0f; else if (clearR < 0.0f) clearR = 0.0f;
		if (clearG > 1.0f) clearG = 1.0f; else if (clearG < 0.0f) clearG = 0.0f;
		if (clearB > 1.0f) clearB = 1.0f; else if (clearB < 0.0f) clearB = 0.0f;

		glClearColor(clearR, clearG, clearB, 1.0f);

		world.Step(step, 8, 3);

		accumulator -= step;

		if (newTime - spawnTime > 2.0f)
		{
			createShape();

			spawnTime = newTime;
		}
	}
}

#pragma endregion

#pragma region Touch

void touch(float x, float y)
{
	LOGI("PLS NO TOUCH :D %f, %f\n", x, y);

	//float w = randomFloat(tWidth / 5.0f, tWidth)(generator);
	//float h = randomFloat(tHeight / 5.0f, tHeight)(generator);

	//float r = randomFloat(0.0f, 1.0f)(generator);
	//float g = randomFloat(0.0f, 1.0f)(generator);
	//float b = randomFloat(0.0f, 1.0f)(generator);
	//float a = randomFloat(0.0f, 1.0f)(generator);
	//float angle = randomFloat(0.0f, 360.0f)(generator);

	//if (randomFloat(0.0f, 1.0f)(generator) > 0.49f)
	//	shapes.push_back(new Triangle(x, y, w, h, glm::radians(angle), glm::vec4(r, g, b, a), world, true));
	//else
	//	shapes.push_back(new Rectangle(x, y, w, h, glm::radians(angle), glm::vec4(r, g, b, a), world, true));
}

#pragma endregion

#pragma region Draw

void draw()
{
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	glUseProgram(program);

	for (auto shape : shapes)
	{
		shape->draw();
	}

	glUseProgram(0);
}

#pragma endregion

#pragma region Java

extern "C"
{
	JNIEXPORT void JNICALL Java_fi_enko_antroit_AntRoitLib_init(JNIEnv* env, jobject obj, jint width, jint height)
	{
		initGraphics(width, height);
	}

	JNIEXPORT void JNICALL Java_fi_enko_antroit_AntRoitLib_step(JNIEnv* env, jobject obj, jlong time)
	{
		update(time);
		draw();
	}

	JNIEXPORT void JNICALL Java_fi_enko_antroit_AntRoitLib_touch(JNIEnv* env, jobject obj, jfloat x, jfloat y)
	{
		touch(x, y);
	}
}

#pragma endregion