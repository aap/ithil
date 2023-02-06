#include "ithil.h"
#include "glad/glad.h"

#include <glm/gtx/intersect.hpp>

#include <stdio.h>
#include <float.h>

void
Box::Init(void)
{
	inf = vec3(FLT_MAX);
	sup = vec3(-FLT_MAX);
}

void
Box::ContainPoint(vec3 p)
{
	if(p.x < inf.x) inf.x = p.x;
	if(p.x > sup.x) sup.x = p.x;
	if(p.y < inf.y) inf.y = p.y;
	if(p.y > sup.y) sup.y = p.y;
	if(p.z < inf.z) inf.z = p.z;
	if(p.z > sup.z) sup.z = p.z;
}

void
Sphere::FromBox(const Box &box)
{
	center = (box.sup + box.inf)/2.0f;
	radius = length((box.sup - box.inf)/2.0f);
}

bool
IsPointInFrustum(vec3 point, const vec4 *planes)
{
	vec4 p(point, 1.0f);
	for(int i = 0; i < 6; i++)
		if(dot(planes[i], p) < 0.0f)
			return false;
	return true;
}



Mesh::~Mesh(void)
{
	delete[] indices;
	delete[] (Vertex*)vertices;
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &ibo);
	glDeleteVertexArrays(1, &vao);
}

void
Mesh::DrawShaded(void)
{
	u32 offset = 0;
	glBindVertexArray(vao);
	for(const auto &m : submeshes) {
		if(m.numIndices == 0)
			continue;
		if(m.matID < 0 || m.matID >= (int)materials.size())
			SetMaterial(defMat);	// TODO: maybe some error material
		else
			SetMaterial(materials[m.matID]);
		glDrawElements(primType, m.numIndices, GL_UNSIGNED_SHORT, (void*)(uintptr_t)offset);
		offset += m.numIndices*sizeof(u16);
	}
}
void
Mesh::DrawWire(bool active)
{
	ForceColor(active ? activeColor : hullColor);
	glBindVertexArray(vao);
	glDrawElements(primType, numIndices, GL_UNSIGNED_SHORT, 0);
}

void
Mesh::DrawRaw(void)
{
	glBindVertexArray(vao);
	glDrawElements(primType, numIndices, GL_UNSIGNED_SHORT, 0);
}

bool
Mesh::IntersectRay(const mat4 &matrix, vec3 orig, vec3 dir, float &dist)
{
	using namespace glm;

// TODO:
if(primType != GL_TRIANGLES) return false;
	mat4 inv = inverse(matrix);
	vec3 localOrig = vec3(inv * vec4(orig, 1.0f));
	vec3 localDir = mat3(inv) * dir;

	vec3 dummy;
	if(!intersectRaySphere(localOrig, normalize(localDir), boundSphere.center, boundSphere.radius, dummy, dummy))
		return false;

	float closestDist = 1.0f;
	bool wasHit = false;
	for(u32 i = 0; i < numIndices; i += 3) {
		vec3 v0 = GetVertex(indices[i+0]);
		vec3 v1 = GetVertex(indices[i+1]);
		vec3 v2 = GetVertex(indices[i+2]);
		vec2 bary;
		float d;

		if(intersectRayTriangle(localOrig, localDir, v0, v1, v2, bary, d)) {
			if(d < closestDist) {
				closestDist = d;
				wasHit = true;
			}
		}
	}

	if(wasHit) {
		vec3 hit = localOrig + closestDist*localDir;
		hit = vec3(matrix * vec4(hit, 1.0f));
		vec3 hitDir = hit - orig;
		dist = dot(dir, hitDir)/dot(dir, dir);
	}

	return wasHit;
}

int
ClipTriangle(vec3 *in, int nin, vec3 *out, vec4 plane)
{
	int nout = 0;
	for(int i0 = 0; i0 < nin; i0++) {
		int i1 = (i0+1)%nin;
		float d1 = dot(plane, vec4(in[i0], 1.0f));
		float d2 = dot(plane, vec4(in[i1], 1.0f));
		if(d1*d2 < 0.0f) {
			float t = d1/(d1 - d2);
			out[nout++] = (1.0f-t)*in[i0] + t*in[i1];
		}
		if(d2 >= 0.0f)
			out[nout++] = in[i1];
	}
	return nout;
}

bool
Mesh::IntersectFrustum(const mat4 &matrix, const vec4 *planes)
{
	vec4 localPlanes[6];
	mat4 mt = glm::transpose(matrix);
	for(int i = 0; i < 6; i++)
		localPlanes[i] = mt * planes[i];

	// easy test - check if any vertex is inside the frustum
	for(u32 i = 0; i < numIndices; i++)
		if(IsPointInFrustum(GetVertex(indices[i]), localPlanes))
			return true;

// TODO:
if(primType != GL_TRIANGLES) return false;
	// harder test, clip triangles against frustum and see if anything stays inside
	vec3 buf[18];
	vec3 *in, *out;
	int nout;
	for(u32 i = 0; i < numIndices; i += 3) {
		vec3 v0 = GetVertex(indices[i+0]);
		vec3 v1 = GetVertex(indices[i+1]);
		vec3 v2 = GetVertex(indices[i+2]);
		in = &buf[0];
		out = &buf[9];
		in[0] = v0;
		in[1] = v1;
		in[2] = v2;
		nout = 0;

		if(nout = ClipTriangle(in,  3,    out, localPlanes[0]), nout == 0) continue;
		if(nout = ClipTriangle(out, nout, in,  localPlanes[1]), nout == 0) continue;
		if(nout = ClipTriangle(in,  nout, out, localPlanes[2]), nout == 0) continue;
		if(nout = ClipTriangle(out, nout, in,  localPlanes[3]), nout == 0) continue;
		if(nout = ClipTriangle(in,  nout, out, localPlanes[4]), nout == 0) continue;
		if(nout = ClipTriangle(out, nout, in,  localPlanes[5]), nout == 0) continue;

		return true;
	}
	return false;
}


Mesh*
CreateMesh(u32 primType, u32 numVertices, void *vertices, u32 numIndices, u16 *indices, u32 stride)
{
	Mesh *mesh = new Mesh;

	mesh->primType = primType;
	mesh->numVertices = numVertices;
	mesh->vertices = vertices;
	mesh->numIndices = numIndices;
	mesh->indices = indices;
	mesh->stride = stride;

	mesh->boundBox.Init();
	for(u32 i = 0; i < numVertices; i++)
		mesh->boundBox.ContainPoint(mesh->GetVertex(i));
	mesh->boundSphere.FromBox(mesh->boundBox);

	glCreateBuffers(1, &mesh->vbo);
	glNamedBufferStorage(mesh->vbo, numVertices*stride, vertices, GL_DYNAMIC_STORAGE_BIT);
	glCreateBuffers(1, &mesh->ibo);
	glNamedBufferStorage(mesh->ibo, numIndices*sizeof(u16), indices, GL_DYNAMIC_STORAGE_BIT);

	glCreateVertexArrays(1, &mesh->vao);
	glVertexArrayVertexBuffer(mesh->vao, 0, mesh->vbo, 0, stride);
	glVertexArrayElementBuffer(mesh->vao, mesh->ibo);

	glEnableVertexArrayAttrib(mesh->vao, 0);
	glEnableVertexArrayAttrib(mesh->vao, 1);
	glEnableVertexArrayAttrib(mesh->vao, 2);
	glVertexArrayAttribFormat(mesh->vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, pos));
	glVertexArrayAttribFormat(mesh->vao, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(Vertex, color));
	glVertexArrayAttribFormat(mesh->vao, 2, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
	glVertexArrayAttribBinding(mesh->vao, 0, 0);
	glVertexArrayAttribBinding(mesh->vao, 1, 0);
	glVertexArrayAttribBinding(mesh->vao, 2, 0);

	Mesh::Submesh sm;
	sm.numIndices = numIndices;
	sm.firstIndex = 0;
	sm.matID = MATID_DEFAULT;
	mesh->submeshes.push_back(sm);

	return mesh;
}

void
Mesh::UpdateMesh(void)
{
	glNamedBufferSubData(vbo, 0, numVertices*stride, vertices);
}

void
Mesh::UpdateIndices(void)
{
	glNamedBufferSubData(ibo, 0, numIndices*sizeof(u16), indices);
}



VertexMesh*
CreateInstanceMesh(u32 primType, u32 numVertices, void *vertices, u32 numIndices, u16 *indices, u32 stride, InstData *instData, u32 nInst)
{
	VertexMesh *mesh = new VertexMesh;

	mesh->primType = primType;
	mesh->numVertices = numVertices;
	mesh->vertices = vertices;
	mesh->numIndices = numIndices;
	mesh->indices = indices;
	mesh->stride = stride;

	mesh->inst = instData;
	mesh->numInst = nInst;

	mesh->boundBox.Init();
	for(u32 i = 0; i < numVertices; i++)
		mesh->boundBox.ContainPoint(mesh->GetVertex(i));
	mesh->boundSphere.FromBox(mesh->boundBox);

	glCreateBuffers(1, &mesh->vbo);
	glNamedBufferStorage(mesh->vbo, numVertices*stride + nInst*sizeof(InstData), nil, GL_DYNAMIC_STORAGE_BIT);
	glNamedBufferSubData(mesh->vbo, 0, numVertices*stride, vertices);
	glNamedBufferSubData(mesh->vbo, numVertices*stride, nInst*sizeof(InstData), instData);
	glCreateBuffers(1, &mesh->ibo);
	glNamedBufferStorage(mesh->ibo, numIndices*sizeof(u16), indices, GL_DYNAMIC_STORAGE_BIT);

	glCreateVertexArrays(1, &mesh->vao);
	glVertexArrayVertexBuffer(mesh->vao, 0, mesh->vbo, 0, stride);
	glVertexArrayVertexBuffer(mesh->vao, 1, mesh->vbo, numVertices*stride, sizeof(InstData));
	glVertexArrayBindingDivisor(mesh->vao, 1, 1);
	glVertexArrayElementBuffer(mesh->vao, mesh->ibo);

	glEnableVertexArrayAttrib(mesh->vao, 0);
	glEnableVertexArrayAttrib(mesh->vao, 1);
	glEnableVertexArrayAttrib(mesh->vao, 2);
	glEnableVertexArrayAttrib(mesh->vao, 3);
	glEnableVertexArrayAttrib(mesh->vao, 4);
	glVertexArrayAttribFormat(mesh->vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, pos));
	glVertexArrayAttribFormat(mesh->vao, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(Vertex, color));
	glVertexArrayAttribFormat(mesh->vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, uv));
	glVertexArrayAttribFormat(mesh->vao, 3, 4, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribFormat(mesh->vao, 4, 2, GL_FLOAT, GL_FALSE, offsetof(InstData, uv));
	glVertexArrayAttribBinding(mesh->vao, 0, 0);
	glVertexArrayAttribBinding(mesh->vao, 1, 0);
	glVertexArrayAttribBinding(mesh->vao, 2, 0);
	glVertexArrayAttribBinding(mesh->vao, 3, 1);
	glVertexArrayAttribBinding(mesh->vao, 4, 1);

	return mesh;
}

void
VertexMesh::UpdateInstanceData(void)
{
	glNamedBufferSubData(vbo, numVertices*stride, numInst*sizeof(InstData), inst);
}

void
VertexMesh::DrawVertices(bool active)
{
	cvProg.Use();
	ForceColor(active ? activeCvColor : cvColor, activeCvColor);
	iconsTex->Bind(0);
	SetCamera();
	SetWorldMatrix(worldMat);
	glBindVertexArray(vao);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(0.0f, -200.0f);
	glDrawElementsInstanced(primType, numIndices, GL_UNSIGNED_SHORT, 0, numInst);
	glDisable(GL_POLYGON_OFFSET_FILL);
	defProg.Use();
}




Mesh*
CreateCube(void)
{
	static Vertex vertices[] = {
		{ { -1.0f, -1.0f, -1.0f }, {   0,   0,   0, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ { -1.0f, -1.0f,  1.0f }, {   0,   0, 255, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ { -1.0f,  1.0f, -1.0f }, {   0, 255,   0, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ { -1.0f,  1.0f,  1.0f }, {   0, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ {  1.0f, -1.0f, -1.0f }, { 255,   0,   0, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ {  1.0f, -1.0f,  1.0f }, { 255,   0, 255, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ {  1.0f,  1.0f, -1.0f }, { 255, 255,   0, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ {  1.0f,  1.0f,  1.0f }, { 255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f } },
	};
	static u16 indices[] = {
		0, 1, 2,
		2, 1, 3,

		4, 6, 5,
		5, 6, 7,
		
		5, 7, 1,
		1, 7, 3,

		0, 2, 4,
		4, 2, 6,

		7, 6, 3,
		3, 6, 2,

		1, 0, 5,
		5, 0, 4
	};
	for(u32 i = 0; i < nelem(vertices); i++) {
		float n = 1.0f/sqrt(sq(vertices[i].pos[0]) + sq(vertices[i].pos[1]) + sq(vertices[i].pos[2]));
		vertices[i].normal[0] = vertices[i].pos[0]*n;
		vertices[i].normal[1] = vertices[i].pos[1]*n;
		vertices[i].normal[2] = vertices[i].pos[2]*n;
	}

	Vertex *verts = new Vertex[nelem(vertices)];
	u16 *inds = new u16[nelem(indices)];
	memcpy(verts, vertices, sizeof(vertices));
	memcpy(inds, indices, sizeof(indices));

	return CreateMesh(GL_TRIANGLES, nelem(vertices), verts, nelem(indices), inds, sizeof(Vertex));
}

Mesh*
CreateTorus(float R1, float R2)
{
	int N1 = 100;
	int N2 = 20;

	u32 numVertices = N1*N2;
	u32 numIndices = 6*N1*N2;
	Vertex *vertices = new Vertex[numVertices];
	u16 *indices = new u16[numIndices];

	for(int i = 0; i < N1; i++) {
		float theta = i * TAU/N1;
		glm::quat rot = glm::angleAxis(theta, vec3(0.0f, 0.0f, 1.0f));
		for(int j = 0; j < N2; j++) {
			float phi = j * TAU/N2;

			vec3 pos(cosf(phi), 0.0f, sinf(phi));
			vec3 norm = pos = rot * pos;
			pos = R2*pos + vec3(R1*cosf(theta), R1*sinf(theta), 0.0f);

			Vertex *v = &vertices[i*N2 + j];
			v->pos[0] = pos.x;
			v->pos[1] = pos.y;
			v->pos[2] = pos.z;
			v->normal[0] = norm.x;
			v->normal[1] = norm.y;
			v->normal[2] = norm.z;
			v->color[0] = (norm.x+1.0f)*0.5f * 255;
			v->color[1] = (norm.y+1.0f)*0.5f * 255;
			v->color[2] = (norm.z+1.0f)*0.5f * 255;
			v->color[3] = 255;
		}
	}

	int n = 0;
	for(int i = 0; i < N1; i++)
		for(int j = 0; j < N2; j++) {
			int i0 = i*N2 + j;
			int i1 = (i+1)%N1*N2 + j;
			int i2 = i*N2 + (j+1)%N2;
			int i3 = (i+1)%N1*N2 + (j+1)%N2;
			indices[n++] = i0;
			indices[n++] = i1;
			indices[n++] = i2;
			indices[n++] = i1;
			indices[n++] = i2;
			indices[n++] = i3;
		}

	Mesh *mesh = CreateMesh(GL_TRIANGLES, numVertices, vertices, numIndices, indices, sizeof(Vertex));

//	delete[] indices;
//	delete[] vertices;

	return mesh;
}

Mesh*
CreateSphere(float R)
{
	int N = 30;

	u32 numVertices = N*N;
	u32 numIndices = 6*N*N;
	Vertex *vertices = new Vertex[numVertices];
	u16 *indices = new u16[numIndices];

	for(int i = 0; i < N; i++) {
		float theta = i * PI/(N-1);
		for(int j = 0; j < N; j++) {
			float phi = j * TAU/N;
		//	glm::vec3 pos(cosf(theta), sinf(theta)*cosf(phi), sinf(theta)*sinf(phi));
			glm::vec3 pos(sinf(theta)*cosf(phi), sinf(theta)*sinf(phi), cosf(theta));
			Vertex *v = &vertices[i*N + j];
			v->pos[0] = R*pos.x;
			v->pos[1] = R*pos.y;
			v->pos[2] = R*pos.z;
			v->normal[0] = R*pos.x;
			v->normal[1] = R*pos.y;
			v->normal[2] = R*pos.z;
			v->color[0] = (pos.x+1.0f)*0.5f * 255;
			v->color[1] = (pos.y+1.0f)*0.5f * 255;
			v->color[2] = (pos.z+1.0f)*0.5f * 255;
			v->color[3] = 255;
		}
	}

	int n = 0;
	for(int i = 0; i < N; i++)
		for(int j = 0; j < N; j++) {
			int i0 = i*N + j;
			int i1 = (i+1)%N*N + j;
			int i2 = i*N + (j+1)%N;
			int i3 = (i+1)%N*N + (j+1)%N;
			indices[n++] = i0;
			indices[n++] = i1;
			indices[n++] = i2;
			indices[n++] = i1;
			indices[n++] = i2;
			indices[n++] = i3;
		}

	Mesh *mesh = CreateMesh(GL_TRIANGLES, numVertices, vertices, numIndices, indices, sizeof(Vertex));

//	delete[] indices;
//	delete[] vertices;

	return mesh;
}

Mesh*
CreateGrid(void)
{
	int N = 11;
	float dx = 1.0f;

	u32 numVertices = 4*N;
	u32 numIndices = 4*N;
	Vertex *vertices = new Vertex[numVertices];
	u16 *indices = new u16[numIndices];

	float off = (N-1)*dx/2.0f;
	int n = 0;
	for(int i = 0; i < N; i++) {
		Vertex *v = &vertices[n++];
		v->pos[0] = i*dx - off;
		v->pos[1] = -off;
		v->pos[2] = 0.0f;
		v->color[0] = 141;
		v->color[1] = 141;
		v->color[2] = 141;
		if(i == N/2)
			v->color[0] = v->color[1] = v->color[2] = 0;
		v->color[3] = 255;
		v = &vertices[n++];
		v->pos[0] = i*dx - off;
		v->pos[1] = off;
		v->pos[2] = 0.0f;
		v->color[0] = 141;
		v->color[1] = 141;
		v->color[2] = 141;
		if(i == N/2)
			v->color[0] = v->color[1] = v->color[2] = 0;
		v->color[3] = 255;
	}
	for(int i = 0; i < N; i++) {
		Vertex *v = &vertices[n++];
		v->pos[0] = -off;
		v->pos[1] = i*dx - off;
		v->pos[2] = 0.0f;
		v->color[0] = 141;
		v->color[1] = 141;
		v->color[2] = 141;
		if(i == N/2)
			v->color[0] = v->color[1] = v->color[2] = 0;
		v->color[3] = 255;
		v = &vertices[n++];
		v->pos[0] = off;
		v->pos[1] = i*dx - off;
		v->pos[2] = 0.0f;
		v->color[0] = 141;
		v->color[1] = 141;
		v->color[2] = 141;
		if(i == N/2)
			v->color[0] = v->color[1] = v->color[2] = 0;
		v->color[3] = 255;
	}
	for(u32 i = 0; i < numIndices; i++)
		indices[i] = i;

	Mesh *mesh = CreateMesh(GL_LINES, numVertices, vertices, numIndices, indices, sizeof(Vertex));

//	delete[] indices;
//	delete[] vertices;

	return mesh;
}
