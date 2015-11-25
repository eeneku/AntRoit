#pragma once

#include <GLES2\gl2.h>

class GameObject
{
public:
	GameObject(float x = 0.0f , float y = 0.0f, float width = 0.0f, float height = 0.0f, float r = 1.0f, float g = 0.0f, float b = 0.0f, GLuint program = 0.0f);
	~GameObject();

	void draw();
private:
	float x;
	float y;
	float width;
	float height;
	float r;
	float g;
	float b;

	GLuint program;
};