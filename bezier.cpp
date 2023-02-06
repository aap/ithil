#include "ithil.h"
#include "glad/glad.h"

#include <glm/gtx/intersect.hpp>

#include <stdio.h>
#include <float.h>


void
ControlVertex::SetSelected(bool sel)
{
	selected = sel;
	parent->dirty |= DIRTY_SEL;
}

void
ControlVertex::TransformDelta(const mat4 &mat)
{
	// TODO: this is stupid. don't want to recalculate this all the time
	mat4 delta = glm::inverse(parent->node->globalMatrix) * mat * parent->node->globalMatrix;
	pos = delta * vec4(vec3(pos), 1.0f);
	parent->dirty |= DIRTY_POS;
}


BezierSurface*
CreateBezierSurface(void)
{
	BezierSurface *surf = new BezierSurface;
	for(u32 i = 0; i < nelem(surf->CVs); i++)
		surf->CVs[i].parent = surf;
	surf->surfaceMesh = nil;
	surf->hullMesh = nil;
	surf->curveMesh = nil;
	surf->cvMesh = nil;
	surf->dirty = DIRTY_POS | DIRTY_SEL;
	surf->matID = MATID_DEFAULT;

	return surf;
}

BezierSurface::~BezierSurface(void)
{
	delete hullMesh;
	delete surfaceMesh;
	delete curveMesh;
	delete cvMesh;
}

void
BezierSurface::DrawShaded(void)
{
	Update();
	surfaceMesh->submeshes[0].matID = matID;
	surfaceMesh->DrawShaded();
}

void
BezierSurface::DrawWire(bool active)
{
	Update();
	ForceColor(active ? activeColor : wireColor);
	curveMesh->DrawRaw();
}

void
BezierSurface::DrawHull(bool active)
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
BezierSurface::Update(void)
{
	UpdateCVs();
	UpdateHull();
	UpdateSurf();
	UpdateCurve();
	dirty = 0;
}

void
BezierSurface::UpdateCVs(void)
{
	if(!dirty)
		return;

	InstData *instData;
	if(cvMesh)
		instData = cvMesh->inst;
	else
		instData = new InstData[4*4];
	for(u32 i = 0; i < nelem(CVs); i++) {
		instData[i].pos_sel = vec4(vec3(CVs[i].pos), CVs[i].selected);
		if(i == 0)
			instData[i].uv = vec2(0.5f, 0.0f);	// crosshair
		else if(i == 1)
			instData[i].uv = vec2(0.0f, 0.25f);	// U
		else if(i == 4)
			instData[i].uv = vec2(0.25f, 0.25f);	// V
		else
			instData[i].uv = vec2(0.25f, 0.0f);	// cross
	}
	if(cvMesh) {
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

	cvMesh = CreateInstanceMesh(GL_TRIANGLES, nelem(vertices), verts, nelem(indices), inds, sizeof(Vertex), instData, 4*4);
}

void
BezierSurface::UpdateHull(void)
{
	int N = 4;
//	int numIndices = 2*N*(N + N-2);
	int numIndices = 2*2*N*(N-1);
	Vertex *verts;
	u16 *indices;
	if(dirty & DIRTY_POS || hullMesh == nil) {
		if(hullMesh)
			verts = (Vertex*)hullMesh->vertices;
		else
			verts = new Vertex[N*N];

		for(int i = 0; i < N*N; i++) {
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
		for(int iv = 0; iv < N; iv++)
			for(int iu = 0; iu < N-1; iu++) {
				int i0 = iu + iv*N;
				int i1 = iu+1 + iv*N;
				if(CVs[i0].selected || CVs[i1].selected) {
					indices[--idxu] = i0;
					indices[--idxu] = i1;
				} else {
					indices[idx++] = i0;
					indices[idx++] = i1;
				}
			}
		for(int iu = 0; iu < N; iu++)
			for(int iv = 0; iv < N-1; iv++) {
				int i0 = iu + iv*N;
				int i1 = iu + (iv+1)*N;
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
		hullMesh = CreateMesh(GL_LINES, N*N, verts, numIndices, indices, sizeof(Vertex));
		hullMesh->submeshes[0].matID = MATID_HULL;
		Mesh::Submesh sm;
		sm.numIndices = 0;
		sm.matID = MATID_HULL_ACTIVE;
		hullMesh->submeshes.push_back(sm);
	}
}

void
BezierSurface::UpdateCurve(void)
{
	if(!(dirty & DIRTY_POS))
		return;
	assert(surfaceMesh);
	int N = 10;
	Vertex *verts;
	if(curveMesh)
		verts = (Vertex*)curveMesh->vertices;
	else
		verts = new Vertex[N*N];
	memcpy(verts, surfaceMesh->vertices, surfaceMesh->numVertices*surfaceMesh->stride);

	for(u32 i = 0; i < surfaceMesh->numVertices; i++) {
		verts[i].color[0] = 0;
		verts[i].color[1] = 0;
		verts[i].color[2] = 0;
	}
	if(curveMesh) {
		curveMesh->UpdateMesh();
		return;
	}

	u16 *indices = new u16[2*N*(N + N-2)];
	int idx = 0;
	for(int iv = 0; iv < N; iv++) {
		for(int iu = 0; iu < N; iu++) {
			indices[idx++] = iu + iv*N;
			if(iu != 0 && iu != N-1)
				indices[idx++] = iu + iv*N;
		}
	}
	for(int iu = 0; iu < N; iu++) {
		for(int iv = 0; iv < N; iv++) {
			indices[idx++] = iu + iv*N;
			if(iv != 0 && iv != N-1)
				indices[idx++] = iu + iv*N;
		}
	}

	curveMesh = CreateMesh(GL_LINES, surfaceMesh->numVertices, verts, 2*N*(N + N-2), indices, sizeof(Vertex));
}

void
BezierSurface::UpdateSurf(void)
{
	if(!(dirty & DIRTY_POS))
		return;
	int N = 10;
	Vertex *verts;
	if(surfaceMesh)
		verts = (Vertex*)surfaceMesh->vertices;
	else
		verts = new Vertex[N*N];

	float eps = 0.001f;
	for(int iv = 0; iv < N; iv++) {
		float v = (float)iv/(N-1);
		for(int iu = 0; iu < N; iu++) {
			float u = (float)iu/(N-1);
			vec3 pos = Eval(u, v);
			Vertex *vx = &verts[iv*N + iu];
			vx->pos[0] = pos.x;
			vx->pos[1] = pos.y;
			vx->pos[2] = pos.z;

			vec3 v1, v2;
			if(iu == N-1)
				v1 = pos - Eval(u-eps, v);
			else
				v1 = Eval(u+eps, v) - pos;
			if(iv == N-1)
				v2 = pos - Eval(u, v-eps);
			else
				v2 = Eval(u, v+eps) - pos;
			vec3 n = normalize(cross(v1, v2));
			if(length(n) == 0.0f)
				n = vec3(0.0f, 0.0f, 1.0f);

			vx->normal[0] = n.x;
			vx->normal[1] = n.y;
			vx->normal[2] = n.z;
			vx->color[0] = (n.x+1.0f)*0.5f * 255;
			vx->color[1] = (n.y+1.0f)*0.5f * 255;
			vx->color[2] = (n.z+1.0f)*0.5f * 255;
			vx->color[3] = 255;
		}
	}
	if(surfaceMesh) {
		surfaceMesh->UpdateMesh();
		return;
	}

	u16 *indices = new u16[3*2*(N-1)*(N-1)];
	int idx = 0;
	for(int iv = 0; iv < N-1; iv++) {
		for(int iu = 0; iu < N-1; iu++) {
			indices[idx++] = iv*N + iu;
			indices[idx++] = (iv+1)*N + iu;
			indices[idx++] = iv*N + iu+1;

			indices[idx++] = iv*N + iu+1;
			indices[idx++] = (iv+1)*N + iu;
			indices[idx++] = (iv+1)*N + iu+1;
		}
	}

	surfaceMesh = CreateMesh(GL_TRIANGLES, N*N, verts, 3*2*(N-1)*(N-1), indices, sizeof(Vertex));
}

vec3
BezierSurface::Eval(float u, float v)
{
	vec4 out(0.0f);
	float us[4], vs[4];
	float iu = 1.0f-u;
	float iv = 1.0f-v;
	us[0] = iu*iu*iu;
	us[1] = 3.0f*u*iu*iu;
	us[2] = 3.0f*u*u*iu;
	us[3] = u*u*u;
	vs[0] = iv*iv*iv;
	vs[1] = 3.0f*v*iv*iv;
	vs[2] = 3.0f*v*v*iv;
	vs[3] = v*v*v;
	for(int i = 0; i < 4; i++)
		for(int j = 0; j < 4; j++)
			out += CVs[j+i*4].pos*us[j]*vs[i];
	return vec3(out);
}

void
BezierSurface::FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs)
{
	vec4 localPlanes[6];
	mat4 mt = glm::transpose(matrix);
	for(int i = 0; i < 6; i++)
		localPlanes[i] = mt * planes[i];

	for(u32 i = 0; i < nelem(CVs); i++)
		if(IsPointInFrustum(CVs[i].pos, localPlanes))
			cvs.push_back(&CVs[i]);

}

vec3 teapotVerts[] = {
	{  0.2000,  0.0000, 2.70000 }, {  0.2000, -0.1120, 2.70000 },
	{  0.1120, -0.2000, 2.70000 }, {  0.0000, -0.2000, 2.70000 },
	{  1.3375,  0.0000, 2.53125 }, {  1.3375, -0.7490, 2.53125 },
	{  0.7490, -1.3375, 2.53125 }, {  0.0000, -1.3375, 2.53125 },
	{  1.4375,  0.0000, 2.53125 }, {  1.4375, -0.8050, 2.53125 },
	{  0.8050, -1.4375, 2.53125 }, {  0.0000, -1.4375, 2.53125 },
	{  1.5000,  0.0000, 2.40000 }, {  1.5000, -0.8400, 2.40000 },
	{  0.8400, -1.5000, 2.40000 }, {  0.0000, -1.5000, 2.40000 },
	{  1.7500,  0.0000, 1.87500 }, {  1.7500, -0.9800, 1.87500 },
	{  0.9800, -1.7500, 1.87500 }, {  0.0000, -1.7500, 1.87500 },
	{  2.0000,  0.0000, 1.35000 }, {  2.0000, -1.1200, 1.35000 },
	{  1.1200, -2.0000, 1.35000 }, {  0.0000, -2.0000, 1.35000 },
	{  2.0000,  0.0000, 0.90000 }, {  2.0000, -1.1200, 0.90000 },
	{  1.1200, -2.0000, 0.90000 }, {  0.0000, -2.0000, 0.90000 },
	{ -2.0000,  0.0000, 0.90000 }, {  2.0000,  0.0000, 0.45000 },
	{  2.0000, -1.1200, 0.45000 }, {  1.1200, -2.0000, 0.45000 },
	{  0.0000, -2.0000, 0.45000 }, {  1.5000,  0.0000, 0.22500 },
	{  1.5000, -0.8400, 0.22500 }, {  0.8400, -1.5000, 0.22500 },
	{  0.0000, -1.5000, 0.22500 }, {  1.5000,  0.0000, 0.15000 },
	{  1.5000, -0.8400, 0.15000 }, {  0.8400, -1.5000, 0.15000 },
	{  0.0000, -1.5000, 0.15000 }, { -1.6000,  0.0000, 2.02500 },
	{ -1.6000, -0.3000, 2.02500 }, { -1.5000, -0.3000, 2.25000 },
	{ -1.5000,  0.0000, 2.25000 }, { -2.3000,  0.0000, 2.02500 },
	{ -2.3000, -0.3000, 2.02500 }, { -2.5000, -0.3000, 2.25000 },
	{ -2.5000,  0.0000, 2.25000 }, { -2.7000,  0.0000, 2.02500 },
	{ -2.7000, -0.3000, 2.02500 }, { -3.0000, -0.3000, 2.25000 },
	{ -3.0000,  0.0000, 2.25000 }, { -2.7000,  0.0000, 1.80000 },
	{ -2.7000, -0.3000, 1.80000 }, { -3.0000, -0.3000, 1.80000 },
	{ -3.0000,  0.0000, 1.80000 }, { -2.7000,  0.0000, 1.57500 },
	{ -2.7000, -0.3000, 1.57500 }, { -3.0000, -0.3000, 1.35000 },
	{ -3.0000,  0.0000, 1.35000 }, { -2.5000,  0.0000, 1.12500 },
	{ -2.5000, -0.3000, 1.12500 }, { -2.6500, -0.3000, 0.93750 },
	{ -2.6500,  0.0000, 0.93750 }, { -2.0000, -0.3000, 0.90000 },
	{ -1.9000, -0.3000, 0.60000 }, { -1.9000,  0.0000, 0.60000 },
	{  1.7000,  0.0000, 1.42500 }, {  1.7000, -0.6600, 1.42500 },
	{  1.7000, -0.6600, 0.60000 }, {  1.7000,  0.0000, 0.60000 },
	{  2.6000,  0.0000, 1.42500 }, {  2.6000, -0.6600, 1.42500 },
	{  3.1000, -0.6600, 0.82500 }, {  3.1000,  0.0000, 0.82500 },
	{  2.3000,  0.0000, 2.10000 }, {  2.3000, -0.2500, 2.10000 },
	{  2.4000, -0.2500, 2.02500 }, {  2.4000,  0.0000, 2.02500 },
	{  2.7000,  0.0000, 2.40000 }, {  2.7000, -0.2500, 2.40000 },
	{  3.3000, -0.2500, 2.40000 }, {  3.3000,  0.0000, 2.40000 },
	{  2.8000,  0.0000, 2.47500 }, {  2.8000, -0.2500, 2.47500 },
	{  3.5250, -0.2500, 2.49375 }, {  3.5250,  0.0000, 2.49375 },
	{  2.9000,  0.0000, 2.47500 }, {  2.9000, -0.1500, 2.47500 },
	{  3.4500, -0.1500, 2.51250 }, {  3.4500,  0.0000, 2.51250 },
	{  2.8000,  0.0000, 2.40000 }, {  2.8000, -0.1500, 2.40000 },
	{  3.2000, -0.1500, 2.40000 }, {  3.2000,  0.0000, 2.40000 },
	{  0.0000,  0.0000, 3.15000 }, {  0.8000,  0.0000, 3.15000 },
	{  0.8000, -0.4500, 3.15000 }, {  0.4500, -0.8000, 3.15000 },
	{  0.0000, -0.8000, 3.15000 }, {  0.0000,  0.0000, 2.85000 },
	{  1.4000,  0.0000, 2.40000 }, {  1.4000, -0.7840, 2.40000 },
	{  0.7840, -1.4000, 2.40000 }, {  0.0000, -1.4000, 2.40000 },
	{  0.4000,  0.0000, 2.55000 }, {  0.4000, -0.2240, 2.55000 },
	{  0.2240, -0.4000, 2.55000 }, {  0.0000, -0.4000, 2.55000 },
	{  1.3000,  0.0000, 2.55000 }, {  1.3000, -0.7280, 2.55000 },
	{  0.7280, -1.3000, 2.55000 }, {  0.0000, -1.3000, 2.55000 },
	{  1.3000,  0.0000, 2.40000 }, {  1.3000, -0.7280, 2.40000 },
	{  0.7280, -1.3000, 2.40000 }, {  0.0000, -1.3000, 2.40000 }
};
int teapotPatches[10][16] = {
        { 118, 118, 118, 118, 124, 122, 119, 121,
          123, 126, 125, 120,  40,  39,  38,  37 },
        { 102, 103, 104, 105,   4,   5,   6,   7,
            8,   9,  10,  11,  12,  13,  14,  15 },
        {  12,  13,  14,  15,  16,  17,  18,  19,
           20,  21,  22,  23,  24,  25,  26,  27 },
        {  24,  25,  26,  27,  29,  30,  31,  32,
           33,  34,  35,  36,  37,  38,  39,  40 },
        {  96,  96,  96,  96,  97,  98,  99, 100,
          101, 101, 101, 101,   0,   1,   2,   3 },
        {   0,   1,   2,   3, 106, 107, 108, 109,
          110, 111, 112, 113, 114, 115, 116, 117 },
        {  41,  42,  43,  44,  45,  46,  47,  48,
           49,  50,  51,  52,  53,  54,  55,  56 },
        {  53,  54,  55,  56,  57,  58,  59,  60,
           61,  62,  63,  64,  28,  65,  66,  67 },
        {  68,  69,  70,  71,  72,  73,  74,  75,
           76,  77,  78,  79,  80,  81,  82,  83 },
        {  80,  81,  82,  83,  84,  85,  86,  87,
           88,  89,  90,  91,  92,  93,  94,  95 },
};

void
FlipU(ControlVertex *CVs)
{
	for(int iv = 0; iv < 4; iv++) {
		vec4 t = CVs[iv*4 + 0].pos;
		CVs[iv*4 + 0].pos = CVs[iv*4 + 3].pos;
		CVs[iv*4 + 3].pos = t;
		t = CVs[iv*4 + 1].pos;
		CVs[iv*4 + 1].pos = CVs[iv*4 + 2].pos;
		CVs[iv*4 + 2].pos = t;
	}
}
void
MirrorX(ControlVertex *CVs)
{
	for(int i = 0; i < 4*4; i++)
		CVs[i].pos.x = -CVs[i].pos.x;
	FlipU(CVs);
}
void
MirrorY(ControlVertex *CVs)
{
	for(int i = 0; i < 4*4; i++)
		CVs[i].pos.y = -CVs[i].pos.y;
	FlipU(CVs);
}

Node*
CreateTeapot(void)
{
	Node *teapot = new Node("Teapot");

	BezierSurface *surfs[32];
	for(int i = 0; i < 32; i++) {
		char name[256];
		surfs[i] = CreateBezierSurface();
		surfs[i]->matID = 5;
		sprintf(name, "Teapot_%d", i);
		Node *node = new Node(name);
		node->AttachMesh(surfs[i]);
		teapot->AddChild(node);
	}

	for(int i = 0; i < 10; i++) {
		for(int j = 0; j < 4*4; j++) {
			surfs[i]->CVs[j].pos = vec4(teapotVerts[teapotPatches[i][j]], 1.0f);
		//	surfs[i]->CVs[j].pos.z *= 1.2f;
		}
	}
	for(int i = 0; i < 10; i++) {
		for(int j = 0; j < 4*4; j++)
			surfs[i+10]->CVs[j].pos = surfs[i]->CVs[j].pos;
		MirrorY(surfs[i+10]->CVs);
	}
	for(int i = 0; i < 6; i++) {
		for(int j = 0; j < 4*4; j++)
			surfs[i+20]->CVs[j].pos = surfs[i]->CVs[j].pos;
		MirrorX(surfs[i+20]->CVs);
		for(int j = 0; j < 4*4; j++)
			surfs[i+20+6]->CVs[j].pos = surfs[i+10]->CVs[j].pos;
		MirrorX(surfs[i+20+6]->CVs);
	}

	return teapot;
}
