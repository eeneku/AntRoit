#include "gameobject.h"

GameObject::GameObject(float x, float y, float width, float height, float r, float g, float b, GLuint program) :
	x(x), 
	y(y), 
	width(width), 
	height(height), 
	r(r), 
	g(g), 
	b(b), 
	program(program)
{

}

GameObject::~GameObject()
{

}

void GameObject::draw()
{

}