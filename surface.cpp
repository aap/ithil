#include "ithil.h"
#include "glad/glad.h"

#include <glm/gtx/intersect.hpp>

#include <stdio.h>
#include <float.h>


float evalBasis(float t, int i, int d, float *knots);

Surface::Surface(void) : degreeU(0), degreeV(0), numU(0), numV(0), surfaceMesh(nil), curveMesh(nil), hullMesh(nil), cvMesh(nil), matID(MATID_DEFAULT) {}

Surface::~Surface(void)
{
	delete hullMesh;
	delete surfaceMesh;
	delete cvMesh;
}


void
Surface::DrawWire(bool active)
{
	Update();

	if(active) {
		ForceColor(activeColor);
		curveMesh->DrawRaw();
	} else
		curveMesh->DrawShaded();
}

void
Surface::DrawHull(bool active)
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
Surface::DrawShaded(void)
{
	Update();
	surfaceMesh->submeshes[0].matID = matID;
	surfaceMesh->DrawShaded();
}

void
Surface::Update(void)
{
	UpdateCVs();
	UpdateHull();
	UpdateSurface();
	UpdateCurve();
	dirty = 0;
}

void
Surface::UpdateCVs(void)
{
	if(!dirty)
		return;
//	assert(knots.size() == CVs.size() + degree + 1);

	InstData *instData;
	if(cvMesh)
		instData = cvMesh->inst;
	else
		instData = new InstData[CVs.size()];
	u32 i;
	// TODO: optimize this
//	activeSpans.clear();
//	activeSpans.resize(knots.size());
	for(i = 0; i < CVs.size(); i++) {
		instData[i].pos_sel = vec4(vec3(CVs[i].pos), CVs[i].selected);
		if(i == 0)
			instData[i].uv = vec2(0.5f, 0.0f);	// crosshair
		else if(i == 1)
			instData[i].uv = vec2(0.0f, 0.25f);	// U
		else if(i == numU)
			instData[i].uv = vec2(0.25f, 0.25f);	// V
		else
			instData[i].uv = vec2(0.25f, 0.0f);	// cross
/*
		if(CVs[i].selected)
			for(int j = 0; j < degree+1; j++)
				activeSpans[i+j] = true;
*/
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

	cvMesh = CreateInstanceMesh(GL_TRIANGLES, nelem(vertices), verts, nelem(indices), inds, sizeof(Vertex), instData, CVs.size());
}

void
Surface::UpdateHull(void)
{
	int N = CVs.size();
	int numIndices = 2*(numU-1)*numV + 2*(numV-1)*numU;
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
		for(int iv = 0; iv < numV; iv++)
			for(int iu = 0; iu < numU-1; iu++) {
				int i0 = iv*numU + iu;
				int i1 = iv*numU + iu+1;
				if(CVs[i0].selected || CVs[i1].selected) {
					indices[--idxu] = i0;
					indices[--idxu] = i1;
				} else {
					indices[idx++] = i0;
					indices[idx++] = i1;
				}
			}
		for(int iu = 0; iu < numU; iu++)
			for(int iv = 0; iv < numV-1; iv++) {
				int i0 = iv*numU + iu;
				int i1 = (iv+1)*numU + iu;
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
Surface::UpdateSurface(void)
{
	if(!(dirty & DIRTY_POS))
		return;

	int Nu = 5 * (numU - degreeU) + 1;
	int Nv = 5 * (numV - degreeV) + 1;
	Vertex *verts;
	if(surfaceMesh)
		verts = (Vertex*)surfaceMesh->vertices;
	else
		verts = new Vertex[Nu*Nv];

	float minU = knotsU[0];
	float maxU = knotsU[knotsU.size()-1];
	float minV = knotsV[0];
	float maxV = knotsV[knotsV.size()-1];
	float eps = 0.0001f;
	for(int iv = 0; iv < Nv; iv++) {
		float v = (float)iv/(Nv-1) * (minV+maxV) - minV;
		v = clamp(v, minV, maxV-eps);
		for(int iu = 0; iu < Nu; iu++) {
			float u = (float)iu/(Nu-1) * (minU+maxU) - minU;
			u = clamp(u, minU, maxU-eps);
			vec3 pos = Eval(u, v);
			Vertex *vx = &verts[iv*Nu + iu];
			vx->pos[0] = pos.x;
			vx->pos[1] = pos.y;
			vx->pos[2] = pos.z;

			vec3 v1, v2;
			if(iu == Nu-1)
				v1 = pos - Eval(u-eps, v);
			else
				v1 = Eval(u+eps, v) - pos;
			if(iv == Nv-1)
				v2 = pos - Eval(u, v-eps);
			else
				v2 = Eval(u, v+eps) - pos;
			vec3 n = normalize(cross(v1, v2));
			if(length(n) == 0.0f)
				n = vec3(0.0f, 0.0f, 1.0f);

			vx->normal[0] = n.x;
			vx->normal[1] = n.y;
			vx->normal[2] = n.z;
			vx->color[0] = 0;
			vx->color[1] = 0;
			vx->color[2] = 0;
			vx->color[3] = 255;
		}
	}
	if(surfaceMesh) {
		surfaceMesh->UpdateMesh();
		return;
	}

	int numIndices = 3*2*(Nu-1)*(Nv-1);
	u16 *indices = new u16[numIndices];
	int idx = 0;
	for(int iv = 0; iv < Nv-1; iv++) {
		for(int iu = 0; iu < Nu-1; iu++) {
			indices[idx++] = iv*Nu + iu;
			indices[idx++] = (iv+1)*Nu + iu;
			indices[idx++] = iv*Nu + iu+1;

			indices[idx++] = iv*Nu + iu+1;
			indices[idx++] = (iv+1)*Nu + iu;
			indices[idx++] = (iv+1)*Nu + iu+1;
		}
	}
	assert(numIndices == idx);

	surfaceMesh = CreateMesh(GL_TRIANGLES, Nu*Nv, verts, numIndices, indices, sizeof(Vertex));
}

void
Surface::UpdateCurve(void)
{
	if(!dirty)
		return;

	float minU = knotsU[0];
	float maxU = knotsU[knotsU.size()-1];
	float minV = knotsV[0];
	float maxV = knotsV[knotsV.size()-1];
	float eps = 0.0001f;
	std::vector<float> isoU;
	std::vector<float> isoV;
	for(u32 i = 0; i < knotsU.size(); i++)
		if(i == 0 || knotsU[i] != knotsU[i-1])
			isoU.push_back(knotsU[i]);
	for(u32 i = 0; i < knotsV.size(); i++)
		if(i == 0 || knotsV[i] != knotsV[i-1])
			isoV.push_back(knotsV[i]);

	int Iu = isoU.size();
	int Iv = isoV.size();
	int Nu = 5 * (numU - degreeU) + 1;
	int Nv = 5 * (numV - degreeV) + 1;

	int N = Iv*Nu + Iu*Nv;
	int numIndices = 2*Iv*(Nu-1) + 2*Iu*(Nv-1);
	Vertex *verts, *verts2;
	u16 *indices;

	if(dirty & DIRTY_POS || curveMesh == nil) {
		if(curveMesh)
			verts = (Vertex*)curveMesh->vertices;
		else
			verts = new Vertex[N];
		verts2 = &verts[Iv*Nu];

		for(int iv = 0; iv < Iv; iv++) {
			float v = clamp(isoV[iv], minV, maxV-eps);
			for(int iu = 0; iu < Nu; iu++) {
				float u = (float)iu/(Nu-1) * (minU+maxU) - minU;
				u = clamp(u, minU, maxU-eps);
				vec3 pos = Eval(u, v);
				Vertex *vx = &verts[iv*Nu + iu];
				vx->pos[0] = pos.x;
				vx->pos[1] = pos.y;
				vx->pos[2] = pos.z;
				vx->color[0] = 0;
				vx->color[1] = 0;
				vx->color[2] = 0;
				vx->color[3] = 255;
			}
		}
		for(int iu = 0; iu < Iu; iu++) {
			float u = clamp(isoU[iu], minU, maxU-eps);
			for(int iv = 0; iv < Nv; iv++) {
				float v = (float)iv/(Nv-1) * (minV+maxV) - minV;
				v = clamp(v, minV, maxV-eps);
				vec3 pos = Eval(u, v);
				Vertex *vx = &verts2[iu*Nv + iv];
				vx->pos[0] = pos.x;
				vx->pos[1] = pos.y;
				vx->pos[2] = pos.z;
				vx->color[0] = 0;
				vx->color[1] = 0;
				vx->color[2] = 0;
				vx->color[3] = 255;
			}
		}
		if(curveMesh)
			curveMesh->UpdateMesh();
	}

	if(dirty & DIRTY_SEL || curveMesh == nil) {
		std::vector<u8> activeSpans(knotsU.size()*knotsV.size());
		for(int iv = 0; iv < numV; iv++)
			for(int iu = 0; iu < numU; iu++)
				if(CVs[iv*numU + iu].selected)
					for(int i = 0; i < degreeV+1; i++)
						for(int j = 0; j < degreeU+1; j++)
							activeSpans[(iv+i)*knotsU.size() + iu+j] = true;

		if(curveMesh)
			indices = curveMesh->indices;
		else
			indices = new u16[numIndices];
		int idx = 0;
		int idxu = numIndices;
// TODO: the highlight logic is not quite right
		for(int iv = 0; iv < Iv; iv++)
			for(int iu = 0; iu < Nu-1; iu++) {
				float u1 = (float)(iu+1)/(Nu-1) * (minU+maxU) - minU;
				float v1 = isoV[iv];
				int i1 = FindParamV(v1);
				int j1 = FindParamU(u1);

				if(activeSpans[i1*knotsU.size() + j1]) {
					indices[--idxu] = iv*Nu + iu+1;
					indices[--idxu] = iv*Nu + iu;
				} else {
					indices[idx++] = iv*Nu + iu;
					indices[idx++] = iv*Nu + iu+1;
				}
			}
		for(int iu = 0; iu < Iu; iu++)
			for(int iv = 0; iv < Nv-1; iv++) {
				float u1 = isoU[iu];
				float v1 = (float)(iv+1)/(Nv-1) * (minV+maxV) - minV;
				int i1 = FindParamV(v1);
				int j1 = FindParamU(u1);

				if(activeSpans[i1*knotsU.size() + j1]) {
					indices[--idxu] = Iv*Nu + iu*Nv + iv+1;
					indices[--idxu] = Iv*Nu + iu*Nv + iv;
				} else {
					indices[idx++] = Iv*Nu + iu*Nv + iv;
					indices[idx++] = Iv*Nu + iu*Nv + iv+1;
				}
			}

		if(curveMesh) {
			curveMesh->submeshes[0].numIndices = idx;
			curveMesh->submeshes[1].numIndices = curveMesh->numIndices - idx;
			curveMesh->UpdateIndices();
		}
	}

	if(curveMesh == nil) {
		curveMesh = CreateMesh(GL_LINES, N, verts, numIndices, indices, sizeof(Vertex));
		curveMesh->submeshes[0].matID = MATID_WIRE;
		Mesh::Submesh sm;
		sm.numIndices = 0;
		sm.matID = MATID_WIRE_ACTIVE;
		curveMesh->submeshes.push_back(sm);
	}
}

vec3
Surface::Eval(float u, float v)
{
	vec4 out(0.0f);
	std::vector<float> basisU(numU);
	std::vector<float> basisV(numV);
	for(int i = 0; i < numU; i++) basisU[i] = evalBasis(u, i, degreeU, &knotsU[0]);
	for(int i = 0; i < numV; i++) basisV[i] = evalBasis(v, i, degreeV, &knotsV[0]);
	for(int iv = 0; iv < numV; iv++)
		for(int iu = 0; iu < numU; iu++)
			out += basisU[iu] * basisV[iv] * CVs[iv*numU + iu].pos;
	return vec3(out)/out.w;
}

int
Surface::FindParamU(float u)
{
	for(u32 i = 0; i < knotsU.size()-1; i++)
		if(knotsU[i] <= u && u <= knotsU[i+1])
			return i;
	return -1;
}

int
Surface::FindParamV(float v)
{
	for(u32 i = 0; i < knotsV.size()-1; i++)
		if(knotsV[i] <= v && v <= knotsV[i+1])
			return i;
	return -1;
}

void
Surface::FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs)
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
CreateTestSurface(void)
{
	Node *n = new Node("TestSurface");

	Surface *surf = new Surface;
	n->AttachMesh(surf);

	float s = 20.0f;
	ControlVertex cv;
	cv.parent = surf;

	surf->degreeU = 3;
	surf->degreeV = 3;
#define V(x, y, z) \
	cv.pos = vec4(x, y, z, s)/s; \
	surf->CVs.push_back(cv);

	V(-69.22764, -0, 12.77366)
	V(-48.72468, 0, 29.16251)
	V(-24.84476, 0, 39.52605)
	V(22.43265, -0, 45.79238)
	V(36.4229, 0, 28.9215)
	V(61.02645, 0, 6.74835)
	V(86.35364, -0, 20.96809)
	V(-69.22764, 9.286676, 12.77366)
	V(-48.72468, 9.286676, 29.16251)
	V(-24.84476, 9.286676, 39.52605)
	V(22.43265, 9.286676, 45.79238)
	V(36.4229, 9.286676, 28.9215)
	V(61.02645, 9.286676, 6.74835)
	V(86.35364, 9.286676, 20.96809)
	V(-68.13416, 23.51821, 6.114491)
	V(-48.19925, 23.51821, 22.27994)
	V(-26.91943, 23.51821, 33.20763)
	V(18.69658, 23.51821, 39.0488)
	V(27.88844, 23.51821, 30.80604)
	V(57.49908, 23.51821, -0.6488895)
	V(86.35364, 23.51821, 14.21974)
	V(-67.00163, 52.46369, -0.7825046)
	V(-47.65504, 52.46369, 15.15157)
	V(-29.06818, 52.46369, 26.66356)
	V(14.82709, 52.46369, 32.06438)
	V(19.04919, 52.46369, 32.75788)
	V(53.84574, 52.46369, -8.310311)
	V(86.35364, 52.46369, 7.230374)
	V(-67.86079, 70.07219, 4.449699)
	V(-48.06789, 70.07219, 20.5593)
	V(-27.43809, 70.07219, 31.62803)
	V(17.76257, 70.07219, 37.36291)
	V(25.75483, 70.07219, 31.27717)
	V(56.61724, 70.07219, -2.498198)
	V(86.35364, 70.07219, 12.53265)
#undef V
	surf->numU = 7;
	surf->knotsU.push_back(0);
	surf->knotsU.push_back(0);
	surf->knotsU.push_back(0);
	surf->knotsU.push_back(0);
	surf->knotsU.push_back(1);
	surf->knotsU.push_back(2);
	surf->knotsU.push_back(3);
	surf->knotsU.push_back(4);
	surf->knotsU.push_back(4);
	surf->knotsU.push_back(4);
	surf->knotsU.push_back(4);
	surf->numV = 5;
	surf->knotsV.push_back(0);
	surf->knotsV.push_back(0);
	surf->knotsV.push_back(0);
	surf->knotsV.push_back(0);
	surf->knotsV.push_back(1);
	surf->knotsV.push_back(2);
	surf->knotsV.push_back(2);
	surf->knotsV.push_back(2);
	surf->knotsV.push_back(2);

	return n;
}
