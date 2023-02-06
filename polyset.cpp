#include "ithil.h"
#include "glad/glad.h"

#include <glm/gtx/intersect.hpp>

#include <ctype.h>
#include <stdio.h>
#include <float.h>
#include <algorithm>

#include <rw.h>
#include <src/rwgta.h>

Polyset::Polyset(void) : numTriangles(0), numEdges(0), maxVertsEdges(0), shadedMesh(nil), wireMesh(nil), cvMesh(nil) {}

void
Polyset::UpdateCVs(void)
{
	if(!dirty)
		return;

	InstData *instData;
	if(cvMesh)
		instData = cvMesh->inst;
	else
		instData = new InstData[vertices.size()];
	for(u32 i = 0; i < vertices.size(); i++) {
		instData[i].pos_sel = vec4(vec3(vertices[i].pos), vertices[i].selected);
		instData[i].uv = vec2(0.0f, 0.0f);	// dot
	}
	if(cvMesh) {
		cvMesh->UpdateInstanceData();
		return;
	}

	// create CV mesh
	static Vertex vertices_[] = {
		{ { -1.0f, -1.0f, 0.0f }, {   255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.25f } },
		{ { -1.0f,  1.0f, 0.0f }, {   255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
		{ {  1.0f, -1.0f, 0.0f }, {   255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.25f } },
		{ {  1.0f,  1.0f, 0.0f }, {   255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.0f } },
	};
	static u16 indices[] = {
		0, 1, 2,
		2, 1, 3,
	};
	for(u32 i = 0; i < nelem(vertices_); i++) {
		float n = 1.0f/sqrt(sq(vertices_[i].pos[0]) + sq(vertices_[i].pos[1]) + sq(vertices_[i].pos[2]));
		vertices_[i].normal[0] = vertices_[i].pos[0]*n;
		vertices_[i].normal[1] = vertices_[i].pos[1]*n;
		vertices_[i].normal[2] = vertices_[i].pos[2]*n;
	}

	Vertex *verts = new Vertex[nelem(vertices_)];
	u16 *inds = new u16[nelem(indices)];
	memcpy(verts, vertices_, sizeof(vertices_));
	memcpy(inds, indices, sizeof(indices));

	cvMesh = CreateInstanceMesh(GL_TRIANGLES, nelem(vertices_), verts, nelem(indices), inds, sizeof(Vertex), instData, vertices.size());
}

static int
FindEdge(int i0, int i1, u16 *indices, int numIndices)
{
	for(int i = 0; i < numIndices; i+=2)
		if(indices[i] == i0 && indices[i+1] == i1)
			return i;
	return -1;
}

static int
FindIndex(PolyIndex &idx, PolyIndex *indices, int numIndices)
{
	for(int i = 0; i < numIndices; i++)
		if(indices[i].pos == idx.pos &&
		   indices[i].tex == idx.tex &&
		   indices[i].norm == idx.norm)
			return i;
	return -1;
}
			

void
Polyset::UpdateWire(void)
{
	Vertex *verts;
	u16 *indices;
	if(dirty & DIRTY_POS || wireMesh == nil) {
		if(wireMesh)
			verts = (Vertex*)wireMesh->vertices;
		else
			verts = new Vertex[vertices.size()];

		for(u32 i = 0; i < vertices.size(); i++) {
			verts[i].pos[0] = vertices[i].pos.x;
			verts[i].pos[1] = vertices[i].pos.y;
			verts[i].pos[2] = vertices[i].pos.z;
			verts[i].color[0] = 0;
			verts[i].color[1] = 0;
			verts[i].color[2] = 0;
			verts[i].color[3] = 255;
		}
		if(wireMesh)
			wireMesh->UpdateMesh();
	}

	int numIndices = 0;
	if(dirty & DIRTY_SEL || wireMesh == nil) {
		if(wireMesh)
			indices = wireMesh->indices;
		else
			indices = new u16[maxVertsEdges*2];
		int idx = 0;
		int idxu = maxVertsEdges*2;
		for(u32 i = 0; i < polygons.size(); i++) {
			Polygon &p = polygons[i];
			for(u32 j = 0; j < p.indices.size(); j++) {
				int t;
				int i0 = uniqueVertices[p.indices[j]].pos;
				int i1 = uniqueVertices[p.indices[(j+1) % p.indices.size()]].pos;
				if(i0 > i1)
					t = i0, i0 = i1, i1 = t;
				if(FindEdge(i0, i1, indices, idx) < 0 &&
				   FindEdge(i0, i1, indices+idxu, maxVertsEdges*2-idxu) < 0) {
					if(vertices[i0].selected || vertices[i1].selected) {
						indices[--idxu] = i1;
						indices[--idxu] = i0;
					} else {
						indices[idx++] = i0;
						indices[idx++] = i1;
					}
					numIndices += 2;
				}
				assert(numIndices <= maxVertsEdges*2);
			}
		}
		memmove(&indices[idx], &indices[idxu], (maxVertsEdges*2-idxu)*sizeof(u16));
		if(wireMesh) {
			wireMesh->submeshes[0].numIndices = idx;
			wireMesh->submeshes[1].numIndices = wireMesh->numIndices - idx;
			wireMesh->UpdateIndices();
		}
	}

	if(wireMesh == nil) {
		wireMesh = CreateMesh(GL_LINES, vertices.size(), verts, numIndices, indices, sizeof(Vertex));
		wireMesh->submeshes[0].matID = MATID_WIRE;
		Mesh::Submesh sm;
		sm.numIndices = 0;
		sm.matID = MATID_WIRE_ACTIVE;
		wireMesh->submeshes.push_back(sm);
	}
}

void
Polyset::UpdateShaded(void)
{
	if(!(dirty & DIRTY_POS))
		return;

	Vertex *verts;
	if(shadedMesh)
		verts = (Vertex*)shadedMesh->vertices;
	else
		verts = new Vertex[uniqueVertices.size()];

	for(u32 i = 0; i < uniqueVertices.size(); i++) {
		PolyIndex idx = uniqueVertices[i];
		Vertex *vx = &verts[i];
		vec3 pos = vertices[idx.pos].pos;
		vx->pos[0] = pos.x;
		vx->pos[1] = pos.y;
		vx->pos[2] = pos.z;
		if(idx.norm >= 0) {
			vec3 n = normals[idx.norm];
			vx->normal[0] = n.x;
			vx->normal[1] = n.y;
			vx->normal[2] = n.z;
		}
		vx->color[0] = 255;
		vx->color[1] = 255;
		vx->color[2] = 255;
		vx->color[3] = 255;
		if(idx.tex >= 0) {
			vec2 uv = uvs[idx.tex];
			vx->uv[0] = uv.x;
			vx->uv[1] = uv.y;
		}
	}
	if(shadedMesh) {
		shadedMesh->UpdateMesh();
		return;
	}

	std::vector<Mesh::Submesh> submeshes;
	Mesh::Submesh sm;
	u16 *indices = new u16[numTriangles*3];
	int idx = 0;
	sm.matID = -1;
	for(u32 i = 0; i < polygons.size(); i++) {
		Polygon &p = polygons[i];
		if(sm.matID != p.matID) {
			if(sm.matID >= 0)
				submeshes.push_back(sm);
			sm.matID = p.matID;
			sm.numIndices = 0;
		}
		for(u32 j = 2; j < p.indices.size(); j++) {
			indices[idx++] = p.indices[0];
			indices[idx++] = p.indices[j-1];
			indices[idx++] = p.indices[j];
			sm.numIndices += 3;
		}
	}
	assert(idx == numTriangles*3);
	if(sm.matID >= 0)
		submeshes.push_back(sm);

	shadedMesh = CreateMesh(GL_TRIANGLES, uniqueVertices.size(), verts, numTriangles*3, indices, sizeof(Vertex));
	shadedMesh->submeshes = submeshes;
}

void
Polyset::Update(void)
{
	UpdateCVs();
	UpdateWire();
	UpdateShaded();
	dirty = 0;
}

void
Polyset::DrawWire(bool active)
{
	Update();

	if(active) {
		ForceColor(activeColor);
		wireMesh->DrawRaw();
	} else
		wireMesh->DrawShaded();
}

void
Polyset::DrawShaded(void)
{
	Update();

	shadedMesh->DrawShaded();
}

void
Polyset::DrawHull(bool active)
{
	Update();

	// only vertices here
	cvMesh->DrawVertices(active);
}

void
Polyset::FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs)
{
	vec4 localPlanes[6];
	mat4 mt = glm::transpose(matrix);
	for(int i = 0; i < 6; i++)
		localPlanes[i] = mt * planes[i];

	for(u32 i = 0; i < vertices.size(); i++)
		if(IsPointInFrustum(vertices[i].pos, localPlanes))
			cvs.push_back(&vertices[i]);
}



// strip comments and return pointer to first non-space char
char*
strip(char *line)
{
	char *p;
	for(p = line; *p; p++){
		if(*p == '#' || *p == '\n'){
			*p = '\0';
			break;
		}
	}
	p = line;
	while(isspace(*p)) p++;
	return p;
}

// skip over non-space chars, set first space to NUL and
// return pointer to next non-space
char*
advance(char *p)
{
	while(*p && !isspace(*p)) p++;
	if(*p){
		*p++ = '\0';
		while(isspace(*p)) p++;
	}
	return p;
}

void
readfloats(char *p, float *f, int n)
{
	int i;
	char *pp;
	for(i = 0; i < n && *p; i++){
		pp = p;
		p = advance(p);
		f[i] = strtof(pp, nil);
	}
}

PolyIndex
readindex(char *p)
{
	PolyIndex ind = { -1, -1, -1 };
	ind.pos = strtol(p, &p, 10) - 1;
	if(*p++ == '/'){
		if(*p != '/')
			ind.tex = strtol(p, &p, 10) - 1;
		if(*p == '/')
			ind.norm = strtol(p+1, &p, 10) - 1;
	}
	return ind;
}




Polyset*
ReadObjFile(FILE *f)
{
	char line[256], *p, *tok;

	Polyset *ps = new Polyset;

	ps->maxVertsEdges = 0;
	ps->numTriangles = 0;
	while(fgets(line, sizeof(line)-1, f)) {
		tok = strip(line);
		p = advance(tok);
		if(strcmp(tok, "v") == 0) {
			float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			readfloats(p, v, 4);
			ControlVertex pv;
			pv.pos = vec4(v[0], v[1], v[2], 1.0f);
			pv.parent = ps;
			ps->vertices.push_back(pv);
		} else if(strcmp(tok, "vt") == 0) {
			float vt[3] = { 0.0f, 0.0f, 0.0f };
			readfloats(p, vt, 3);
			ps->uvs.push_back(vec2(vt[0], vt[1]));
		} else if(strcmp(tok, "vn") == 0) {
			float vn[3] = { 0.0f, 0.0f, 0.0f };
			readfloats(p, vn, 3);
			ps->normals.push_back(vec3(vn[0], vn[1], vn[2]));
		} else if(strcmp(tok, "f") == 0) {
			Polygon f;
//			f.matID = MATID_DEFAULT + ps->polygons.size()%7;
			f.matID = MATID_DEFAULT;
			while(*p) {
				tok = p;
				p = advance(p);
				PolyIndex idx = readindex(tok);
				int i = FindIndex(idx, &ps->uniqueVertices[0], ps->uniqueVertices.size());
				if(i < 0) {
					ps->uniqueVertices.push_back(idx);
					i = ps->uniqueVertices.size()-1;
				}
				f.indices.push_back(i);
			}
			ps->maxVertsEdges += f.indices.size();
			ps->numTriangles += f.indices.size()-2;
			ps->polygons.push_back(f);
		}
	}
	sort(ps->polygons.begin(), ps->polygons.end());

	return ps;
}

Polyset*
ReadObjFile(const char *path)
{
	FILE *f = fopen(path, "r");
	if(f == nil)
		return nil;
	Polyset *ps = ReadObjFile(f);
	fclose(f);
	return ps;
}


Polyset*
ConvertGeometry(rw::Geometry *geo)
{
	using namespace rw;

	Polyset *ps = new Polyset;

	MorphTarget *m = &geo->morphTargets[0];
	ps->vertices.resize(geo->numVertices);
	ps->uniqueVertices.resize(geo->numVertices);
	for(int i = 0; i < geo->numVertices; i++) {
		ps->vertices[i].parent = ps;
		ps->vertices[i].pos.x = m->vertices[i].x;
		ps->vertices[i].pos.y = m->vertices[i].y;
		ps->vertices[i].pos.z = m->vertices[i].z;
		ps->vertices[i].pos.w = 1.0f;
		ps->uniqueVertices[i].pos = i;
		ps->uniqueVertices[i].tex = i;
		ps->uniqueVertices[i].norm = i;
	}

	if(geo->flags & Geometry::NORMALS) {
		ps->normals.resize(geo->numVertices);
		for(int i = 0; i < geo->numVertices; i++) {
			ps->normals[i].x = m->normals[i].x;
			ps->normals[i].y = m->normals[i].y;
			ps->normals[i].z = m->normals[i].z;
		}
	}

	if(geo->numTexCoordSets > 0) {
		ps->uvs.resize(geo->numVertices);
		for(int i = 0; i < geo->numVertices; i++) {
			ps->uvs[i].x = geo->texCoords[0][i].u;
			ps->uvs[i].y = geo->texCoords[0][i].v;
		}
	}

	ps->polygons.resize(geo->numTriangles);
	Polygon p;
	p.indices.resize(3);
	p.matID = MATID_DEFAULT;
	for(int i = 0; i < geo->numTriangles; i++) {
		p.indices[0] = geo->triangles[i].v[0];
		p.indices[1] = geo->triangles[i].v[1];
		p.indices[2] = geo->triangles[i].v[2];
		ps->polygons[i] = p;
	}
	ps->maxVertsEdges = geo->numTriangles*3;
	ps->numTriangles = geo->numTriangles;

	sort(ps->polygons.begin(), ps->polygons.end());

	return ps;
}

Node*
ConvertFrame(rw::Frame *frame)
{
	using namespace rw;

	Node *n = new Node(gta::getNodeName(frame));
	if(strstr(n->name, "_dam") || strstr(n->name, "_vlo"))
		n->visible = false;
	Matrix *m = &frame->matrix;
	n->localMatrix[0][0] = m->right.x;
	n->localMatrix[0][1] = m->right.y;
	n->localMatrix[0][2] = m->right.z;
	n->localMatrix[1][0] = m->up.x;
	n->localMatrix[1][1] = m->up.y;
	n->localMatrix[1][2] = m->up.z;
	n->localMatrix[2][0] = m->at.x;
	n->localMatrix[2][1] = m->at.y;
	n->localMatrix[2][2] = m->at.z;
	n->localMatrix[3][0] = m->pos.x;
	n->localMatrix[3][1] = m->pos.y;
	n->localMatrix[3][2] = m->pos.z;

	FORLIST(lnk, frame->objectList) {
		Object *obj = (Object*)ObjectWithFrame::fromFrame(lnk);
		if(obj->type == Atomic::ID)
			n->AttachMesh(ConvertGeometry(((Atomic*)obj)->geometry));
	}


	for(Frame *f = frame->child; f; f = f->next) {
		Node *cn = ConvertFrame(f);
		n->AddChild(cn);
	}
	return n;
}

Node*
ReadDffFile(const char *path)
{
	using namespace rw;

	StreamFile in;
	if(!in.open(path, "rb"))
		return nil;
	ChunkHeaderInfo header;
	readChunkHeaderInfo(&in, &header);
	if(header.type == ID_UVANIMDICT){
		UVAnimDictionary *dict = UVAnimDictionary::streamRead(&in);
		currentUVAnimDictionary = dict;
		readChunkHeaderInfo(&in, &header);
	}
	if(header.type != ID_CLUMP){
		in.close();
		return 0;
	}
	Clump *c = Clump::streamRead(&in);
	in.close();
	if(c == nil){
		fprintf(stderr, "Error: couldn't read clump\n");
		return nil;
	}

	Node *n = ConvertFrame(c->getFrame());

	c->destroy();

	return n;
}
