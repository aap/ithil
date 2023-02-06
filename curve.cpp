#include "ithil.h"
#include "glad/glad.h"

#include <glm/gtx/intersect.hpp>

#include <stdio.h>
#include <float.h>


Curve::Curve(void) : degree(0), curveMesh(nil), hullMesh(nil), cvMesh(nil) {}

Curve::~Curve(void)
{
	delete hullMesh;
	delete curveMesh;
	delete cvMesh;
}

void
Curve::DrawWire(bool active)
{
	Update();
	if(active) {
		ForceColor(activeColor);
		curveMesh->DrawRaw();
	} else
		curveMesh->DrawShaded();
}

void
Curve::DrawHull(bool active)
{
	Update();

	if(active) {
		ForceColor(activeHullColor);
		hullMesh->DrawRaw();
	} else
		hullMesh->DrawShaded();

	cvMesh->DrawVertices(active);
}

void
Curve::Update(void)
{
	UpdateCVs();
	UpdateHull();
	UpdateCurve();
	dirty = 0;
}

void
Curve::UpdateCVs(void)
{
	if(!dirty)
		return;
	assert(knots.size() == CVs.size() + degree + 1);

	InstData *instData;
	if(cvMesh)
		instData = cvMesh->inst;
	else
		instData = new InstData[CVs.size() + knots.size()];	// this will always suffice
	u32 i;
	// TODO: optimize this
	activeSpans.clear();
	activeSpans.resize(knots.size());
	for(i = 0; i < CVs.size(); i++) {
		instData[i].pos_sel = vec4(vec3(CVs[i].pos), CVs[i].selected);
		if(i == 0)
			instData[i].uv = vec2(0.5f, 0.0f);	// crosshair
		else if(i == 1)
			instData[i].uv = vec2(0.0f, 0.25f);	// U
		else
			instData[i].uv = vec2(0.25f, 0.0f);	// cross
		if(CVs[i].selected)
			for(int j = 0; j < degree+1; j++)
				activeSpans[i+j] = true;
	}
	float minU = knots[0];
	float maxU = knots[knots.size()-1];
	for(u32 j = 0; j < knots.size(); j++) {
		if(knots[j] == minU  || knots[j] == maxU)
			continue;
		instData[i].pos_sel = vec4(Eval(knots[j]), activeSpans[j] * 1.0f);
		instData[i].uv = vec2(0.0f, 0.0f);	// dot
		i++;
	}
	int numInst = i;
	if(cvMesh) {
		cvMesh->numInst = numInst;
		cvMesh->UpdateInstanceData();
		return;
	}

	// create CV mesh
	static Vertex vertices[] = {
		{ { -1.0f, -1.0f, 0.0f }, {   255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.25f } },
		{ { -1.0f,  1.0f, 0.0f }, {   255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
		{ {  1.0f, -1.0f, 0.0f }, {   255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.25f } },
		{ {  1.0f,  1.0f, 0.0f }, {   255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.0f } },
	};
	static u16 indices[] = {
		0, 1, 2,
		2, 1, 3,
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

	cvMesh = CreateInstanceMesh(GL_TRIANGLES, nelem(vertices), verts, nelem(indices), inds, sizeof(Vertex), instData, numInst);
}

void
Curve::UpdateHull(void)
{
	int N = CVs.size();
	int numIndices = 2*(N-1);
	Vertex *verts;
	u16 *indices;
	if(dirty & DIRTY_POS || hullMesh == nil) {
		if(hullMesh)
			verts = (Vertex*)hullMesh->vertices;
		else
			verts = new Vertex[N];

		for(int i = 0; i < N; i++) {
			verts[i].pos[0] = CVs[i].pos.x;
			verts[i].pos[1] = CVs[i].pos.y;
			verts[i].pos[2] = CVs[i].pos.z;
			verts[i].color[0] = 0;
			verts[i].color[1] = 0;
			verts[i].color[2] = 0;
			verts[i].color[3] = 255;
		}
		if(hullMesh)
			hullMesh->UpdateMesh();
	}

	if(dirty & DIRTY_SEL || hullMesh == nil) {
		if(hullMesh)
			indices = hullMesh->indices;
		else
			indices = new u16[numIndices];
		int idx = 0;
		int idxu = numIndices;
		for(int iu = 0; iu < N-1; iu++) {
			int i0 = iu;
			int i1 = iu+1;
			if(CVs[i0].selected || CVs[i1].selected) {
				indices[--idxu] = i0;
				indices[--idxu] = i1;
			} else {
				indices[idx++] = i0;
				indices[idx++] = i1;
			}
		}
		assert(idx == idxu);
		if(hullMesh) {
			hullMesh->submeshes[0].numIndices = idx;
			hullMesh->submeshes[1].numIndices = hullMesh->numIndices - idx;
			hullMesh->UpdateIndices();
		}
	}

	if(hullMesh == nil) {
		hullMesh = CreateMesh(GL_LINES, N, verts, numIndices, indices, sizeof(Vertex));
		hullMesh->submeshes[0].matID = MATID_HULL;
		Mesh::Submesh sm;
		sm.numIndices = 0;
		sm.matID = MATID_HULL_ACTIVE;
		hullMesh->submeshes.push_back(sm);
	}
}

void
Curve::UpdateCurve(void)
{
//	if(!(dirty & DIRTY_POS))
	if(!dirty)
		return;
	assert(knots.size() == CVs.size() + degree + 1);

	int N = 5 * (CVs.size() - degree) + 1;
	Vertex *verts;
	if(curveMesh)
		verts = (Vertex*)curveMesh->vertices;
	else
		verts = new Vertex[N];

	float minU = knots[0];
	float maxU = knots[knots.size()-1];
	float eps = 0.0001f;
	for(int iu = 0; iu < N; iu++) {
		float u = (float)iu/(N-1) * (minU+maxU) - minU;
		u = clamp(u, minU, maxU-eps);
		vec3 pos = Eval(u);
		Vertex *vx = &verts[iu];
		vx->pos[0] = pos.x;
		vx->pos[1] = pos.y;
		vx->pos[2] = pos.z;
		vx->color[0] = 0;
		vx->color[1] = 0;
		vx->color[2] = 0;
		vx->color[3] = 255;
	}

	u16 *indices;
	if(curveMesh)
		indices = curveMesh->indices;
	else
		indices = new u16[2*(N-1)];
	int idx = 0;
	int idxu = 2*(N-1);
	for(int iu = 0; iu < N-1; iu++) {
//		float u0 = (float)iu/(N-1) * (minU+maxU) - minU;
		float u1 = (float)(iu+1)/(N-1) * (minU+maxU) - minU;
//		int i0 = FindParam(u0);
		int i1 = FindParam(u1);
//		printf("%.3f %.3f | %d %d -> %d\n", u0, u1, i0, i1, activeSpans[i0] || activeSpans[i1]);

		// this seems to be working
		if(activeSpans[i1]) {
//		if(activeSpans[i0] || activeSpans[i1]) {
			indices[--idxu] = iu+1;
			indices[--idxu] = iu;
		} else {
			indices[idx++] = iu;
			indices[idx++] = iu+1;
		}
	}
//for(u32 i = 0; i < activeSpans.size(); i++) printf("%d ", activeSpans[i]);
//printf(" | %d %d %d\n", idx, 2*(N-1)-idxu, 2*(N-1));

	if(curveMesh) {
		curveMesh->submeshes[0].numIndices = idx;
		curveMesh->submeshes[1].numIndices = curveMesh->numIndices - idx;
		curveMesh->UpdateMesh();
		curveMesh->UpdateIndices();
		return;
	}

	curveMesh = CreateMesh(GL_LINES, N, verts, 2*(N-1), indices, sizeof(Vertex));
	curveMesh->submeshes[0].matID = MATID_WIRE;
	Mesh::Submesh sm;
	sm.numIndices = 0;
	sm.matID = MATID_WIRE_ACTIVE;
	curveMesh->submeshes.push_back(sm);
}

// a is always 0 in this case (or very close)
float div(float a, float b) { return b == 0.0f ? 0.0f : a/b; }

// naive but known to work
float
evalBasis(float t, int i, int d, float *knots)
{
	if(d == 0) {
		if(knots[i] <= t && t < knots[i+1])
			return 1.0f;
		return 0.0f;
	}
	float b0 = evalBasis(t, i, d-1, knots);
	float b1 = evalBasis(t, i+1, d-1, knots);
	// this way we're only dividing 0 by 0...should this ever become relevant
	return div(b0*(t-knots[i]),     knots[i+d]-knots[i])
	     + div(b1*(knots[i+d+1]-t), knots[i+d+1]-knots[i+1]);
//	return b0*div(t-knots[i], knots[i+d]-knots[i]) + b1*div(knots[i+d+1]-t, knots[i+d+1]-knots[i+1]);
}

vec3
Curve::Eval(float u)
{
	vec4 out(0.0f);
	for(int i = 0; i < (int)CVs.size(); i++) {
		float x = evalBasis(u, i, degree, &knots[0]);
		out += x * CVs[i].pos;
	}
	return vec3(out)/out.w;
}

int
Curve::FindParam(float u)
{
	for(u32 i = 0; i < knots.size()-1; i++)
		if(knots[i] <= u && u <= knots[i+1])
			return i;
	return -1;
}

void
Curve::FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs)
{
	vec4 localPlanes[6];
	mat4 mt = glm::transpose(matrix);
	for(int i = 0; i < 6; i++)
		localPlanes[i] = mt * planes[i];

	for(u32 i = 0; i < CVs.size(); i++)
		if(IsPointInFrustum(CVs[i].pos, localPlanes))
			cvs.push_back(&CVs[i]);

}

Node*
CreateTestCurve(void)
{
	Node *n = new Node("TestCurve");

	Curve *curve = new Curve;
	n->AttachMesh(curve);

	float s = 20.0f;
	ControlVertex cv;
	cv.parent = curve;

	curve->degree = 3;
#define V(x, y, z) \
	cv.pos = vec4(x, y, z, s)/s; \
	curve->CVs.push_back(cv);
	V(-30.63383, 22.65459, 0)
	V(13.50783, 33.01786, 15.06403)
	V(34.252, -10.36327, 15.06403)
	V(-7.959972, -1.205032, 0)
	V(6.995127, -41.32158, -18.19684)
	V(6.995127, -41.32158, 0.0)
#undef V
	curve->knots.push_back(0);
	curve->knots.push_back(0);
	curve->knots.push_back(0);
	curve->knots.push_back(0);
	curve->knots.push_back(1);
	curve->knots.push_back(2);
	curve->knots.push_back(3);
	curve->knots.push_back(3);
	curve->knots.push_back(3);
	curve->knots.push_back(3);

	return n;
}
