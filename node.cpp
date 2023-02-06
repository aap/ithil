#include "ithil.h"

#include <stdio.h>

Node::Node(const char *name) : root(this), parent(nil), child(nil), next(nil), localMatrix(1.0f), mesh(nil), visible(true), isTreeOpen(true)
{
	this->name = (char*)strdup(name);
}

Node::~Node(void)
{
	free(name);
}

void
Node::TransformDelta(const mat4 &mat)
{
	globalMatrix = mat * globalMatrix;
}

vec3
Node::GetPosition(void)
{
	return globalMatrix[3];
}

void
Node::AddChild(Node *c)
{
	if(c->parent)
		c->RemoveChild();
	c->next = child;
	child = c;

	c->parent = this;
	c->root = root;
	for(Node *c = child; c; c = c->next)
		c->UpdateRoot();
}

void
Node::RemoveChild(void)
{
	if(parent->child == this)
		parent->child = next;
	else {
		Node *c;
		for(c = parent->child; c->next != this; c = c->next);
		c->next = next;
	}
	parent = nil;
	next = nil;
	root = this;
	for(Node *c = child; c; c = c->next)
		c->UpdateRoot();
}

bool
Node::IsHigherThan(Node *node)
{
	for(; node->parent != this; node = node->parent)
		if(node->parent == nil)
			return false;
	return true;
}

void
Node::UpdateMatrices(void)
{
	if(parent == nil)
		globalMatrix = localMatrix;
	else
		globalMatrix = parent->globalMatrix * localMatrix;
	for(Node *c = child; c; c = c->next)
		c->UpdateMatrices();
}

void
Node::RecalculateLocal(void)
{
	if(parent == nil)
		localMatrix = globalMatrix;
	else
		localMatrix = glm::inverse(parent->globalMatrix) * globalMatrix;
}

void
Node::AttachMesh(Drawable *m)
{
	assert(mesh == nil);
	mesh = m;
	m->node = this;
}


void
Node::UpdateRoot(void)
{
	root = parent->root;
	for(Node *c = child; c; c = c->next)
		c->UpdateRoot();
}

