#pragma once

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>

#define PI M_PI
#define TAU (2.0f*PI)

#define nil nullptr
#define nelem(array) (sizeof(array)/sizeof(array[0]))
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

extern int display_w, display_h;

void InitApp(void);
void InitGL(void* loadproc);
void InitScene(void);
void RenderScene(void);
void GUI(void);

bool ModCamera(void);
bool ModMarking(void);
bool ModNone(void);


#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat4;
using glm::dot;
using glm::cross;
using glm::normalize;
using glm::value_ptr;

extern int dragging;
extern bool startDragging, stopDragging;
extern bool dragCtrl, dragShift, dragAlt;
extern vec2 dragStart, dragEnd, dragDelta;

inline bool ModCamera(void) { return !dragCtrl && dragShift && dragAlt; }
inline bool ModMarking(void) { return dragCtrl && dragShift && !dragAlt; }
inline bool ModNone(void) { return !ModCamera() && !ModMarking(); }

template <typename T> T min(T a, T b) { return a < b ? a : b; }
template <typename T> T max(T a, T b) { return a > b ? a : b; }
template <typename T> T clamp(T a, T l, T h) { return a > h ? h : a < l ? l : a; }
template <typename T> T sq(T a) { return a*a; }

#define IM_VEC2_CLASS_EXTRA                                                     \
        constexpr ImVec2(const vec2 &f) : x(f.x), y(f.y) {}                   \
        operator vec2() const { return vec2(x,y); } 

void ForceColor(vec4 color, vec4 otherColor = vec4(0.0f));

static vec4 hullColor(138/255.0f, 72/255.0f, 51/255.0f, 1.0f);
static vec4 activeHullColor(228/255.0f, 172/255.0f, 121/255.0f, 1.0f);
static vec4 cvColor(155/255.0f, 0/255.0f, 40/255.0f, 1.0f);
static vec4 activeCvColor(255/255.0f, 255/255.0f, 0/255.0f, 1.0f);
static vec4 wireColor(0/255.0f, 4/255.0f, 96/255.0f, 1.0f);
static vec4 activeColor(1.0f, 1.0f, 1.0f, 1.0f);

struct Sphere;
struct Box;
struct Node;
struct Drawable;

struct Sphere
{
	vec3 center;
	float radius;

	void FromBox(const Box &box);
};

struct Box
{
	vec3 inf;
	vec3 sup;

	void Init(void);
	void ContainPoint(vec3 p);
};

bool IsPointInFrustum(vec3 point, const vec4 *planes);

struct Pickable
{
	bool selected;

	Pickable(void) : selected(false) {}
	virtual void SetSelected(bool sel) { selected = sel; }
	virtual void TransformDelta(const mat4 &mat) = 0;
	virtual vec3 GetPosition(void) = 0;
};

struct Node : public Pickable
{
	char *name;
	Node *root;
	Node *parent;
	Node *child;
	Node *next;	// sibling
	mat4 localMatrix;
	mat4 globalMatrix;

	Drawable *mesh;
	bool visible;
	bool isTreeOpen;	// for hierarchy view

	Node(const char *name);
	~Node(void);
	virtual void TransformDelta(const mat4 &mat);
	virtual vec3 GetPosition(void);

	void AddChild(Node *c);
	void RemoveChild(void);
	bool IsHigherThan(Node *node);
	void UpdateMatrices(void);
	void RecalculateLocal(void);
	void AttachMesh(Drawable *m);

private:
	void UpdateRoot(void);
};

struct Texture
{
	u32 tex;
	u32 width, height;
	void Bind(int n);
};
Texture *CreateTexture(u8 *data, u32 size);
extern Texture *iconsTex;

enum {
	DIRTY_SEL = 1,
	DIRTY_POS = 2
};

struct Drawable
{
	Sphere boundSphere;
	Box boundBox;

	int dirty;
	Node *node;	// not always set, but currently needed for xforming components

	Drawable(void) : dirty(DIRTY_SEL|DIRTY_POS), node(nil) {}
	virtual ~Drawable(void) {}
	virtual void DrawWire(bool active) = 0;
	virtual void DrawShaded(void) = 0;
	virtual void DrawHull(bool active) {}

	virtual bool IntersectRay(const mat4 &matrix, vec3 orig, vec3 dir, float &dist) = 0;
	virtual bool IntersectFrustum(const mat4 &matrix, const vec4 *planes) = 0;
	virtual void FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs) = 0;
};

struct Material
{
	vec4 colorSelector;	// amb, diff, spec, emiss, 0 - material, 1 - vertex
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	vec4 emissive;
	float shininess;
};
enum MaterialID {
	MATID_WIRE,
	MATID_WIRE_ACTIVE,
	MATID_HULL,
	MATID_HULL_ACTIVE,
	MATID_DEFAULT,
};
extern std::vector<Material> materials;
// maybe remove some of these again
extern Material defMat;
extern Material wireMat;
extern Material gridMat;

struct Lighting
{
	vec4 globalAmbient;

	// for now: one hardcoded directional light
	vec4 diffuse;
	vec4 specular;
	vec3 direction;
};

// this is kinda dumb
struct Vertex {
	float pos[3];
	unsigned char color[4];
	float normal[3];
	float uv[2];
};

struct Mesh : public Drawable
{
	u32 primType;
	u32 numVertices;
	void *vertices;
	u32 numIndices;
	u16 *indices;
	u32 stride;
	struct Submesh {
		u32 numIndices;
		u32 firstIndex;
		i32 matID;
	};
	std::vector<Submesh> submeshes;

	u32 vao;
	u32 vbo, ibo;

	Mesh(void) : vao(0), vbo(0), ibo(0) {}
	virtual ~Mesh(void);
	virtual void DrawWire(bool active);
	virtual void DrawShaded(void);
	void DrawRaw(void);

	virtual bool IntersectRay(const mat4 &matrix, vec3 orig, vec3 dir, float &dist);
	virtual bool IntersectFrustum(const mat4 &matrix, const vec4 *planes);
	virtual void FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs) {};

	vec3 GetVertex(int i) { return *(vec3*)((u8*)vertices + i*stride); }

	void UpdateMesh(void);
	void UpdateIndices(void);
};
Mesh *CreateMesh(u32 primType, u32 numVertices, void *vertices, u32 numIndices, u16 *indices, u32 stride);

struct InstData {
	vec4 pos_sel;
	vec2 uv;
};
struct VertexMesh : public Mesh
{
	InstData *inst;
	u32 numInst;

	void UpdateInstanceData(void);
	void DrawVertices(bool active);
};
VertexMesh *CreateInstanceMesh(u32 primType, u32 numVertices, void *vertices, u32 numIndices, u16 *indices, u32 stride, InstData *instData, u32 nInst);

Mesh *CreateCube(void);
Mesh *CreateSphere(float r);
Mesh *CreateTorus(float r1, float r2);
Mesh *CreateGrid(void);



struct ControlVertex : public Pickable
{
	vec4 pos;
	Drawable *parent;

	ControlVertex(void) : parent(nil) {}
	virtual void SetSelected(bool sel);
	virtual void TransformDelta(const mat4 &mat);
	virtual vec3 GetPosition(void) { return pos; }
};

/* Still very unclear what kind of data structure to use here */

struct Polyset;

struct PolyIndex
{
	int pos;
	int tex;
	int norm;
	// TODO: more attributes
};

struct Polygon
{
	std::vector<int> indices;
	int matID;
};
inline bool operator<(const Polygon &p1, const Polygon &p2) { return p1.matID < p2.matID; }

struct Polyset : public Drawable
{
	std::vector<ControlVertex> vertices;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;
	// TODO: more attributes
	std::vector<Polygon> polygons;
	std::vector<PolyIndex> uniqueVertices;

	int numTriangles;
	int numEdges;		// need to generate wire once to know this
	int maxVertsEdges;	// sum of all edges/vertices per polygon, many doubles
	Mesh *shadedMesh;
	Mesh *wireMesh;
	VertexMesh *cvMesh;

	Polyset(void);
	virtual void DrawWire(bool active);
	virtual void DrawShaded(void);
	virtual void DrawHull(bool active);
	virtual bool IntersectRay(const mat4 &matrix, vec3 orig, vec3 dir, float &dist) { return shadedMesh->IntersectRay(matrix, orig, dir, dist); }
	virtual bool IntersectFrustum(const mat4 &matrix, const vec4 *planes) { return shadedMesh->IntersectFrustum(matrix, planes); }
	virtual void FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs);
	void UpdateCVs(void);
	void UpdateWire(void);
	void UpdateShaded(void);
	void Update(void);
};
Polyset *ReadObjFile(FILE *f);
Polyset *ReadObjFile(const char *path);
Node *ReadDffFile(const char *path);




struct BezierSurface;

struct BezierSurface : public Drawable
{
	ControlVertex CVs[4*4];
	Mesh *surfaceMesh;
	Mesh *curveMesh;
	Mesh *hullMesh;
	VertexMesh *cvMesh;
	int matID;

	virtual ~BezierSurface(void);
	virtual void DrawWire(bool active);
	virtual void DrawShaded(void);
	virtual void DrawHull(bool active);
	virtual bool IntersectRay(const mat4 &matrix, vec3 orig, vec3 dir, float &dist) { return surfaceMesh->IntersectRay(matrix, orig, dir, dist); }
	virtual bool IntersectFrustum(const mat4 &matrix, const vec4 *planes) { return surfaceMesh->IntersectFrustum(matrix, planes); }
	virtual void FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs);
	void UpdateHull(void);
	void UpdateCVs(void);
	void UpdateCurve(void);
	void UpdateSurf(void);
	void Update(void);

	vec3 Eval(float u, float v);
};
Node *CreateTeapot(void);


struct Curve : public Drawable
{
	int degree;
	std::vector<ControlVertex> CVs;
	std::vector<float> knots;
	Mesh *curveMesh;
	Mesh *hullMesh;
	VertexMesh *cvMesh;
	std::vector<u8> activeSpans;

	Curve(void);
	virtual ~Curve(void);
	virtual void DrawWire(bool active);
	virtual void DrawShaded(void) {}
	virtual void DrawHull(bool active);
	virtual bool IntersectRay(const mat4 &matrix, vec3 orig, vec3 dir, float &dist) { return curveMesh->IntersectRay(matrix, orig, dir, dist); }
	virtual bool IntersectFrustum(const mat4 &matrix, const vec4 *planes) { return curveMesh->IntersectFrustum(matrix, planes); }
	virtual void FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs);
	void UpdateHull(void);
	void UpdateCVs(void);
	void UpdateCurve(void);
	void Update(void);

	vec3 Eval(float u);
	int FindParam(float u);
};
Node *CreateTestCurve(void);

struct Surface : public Drawable
{
	int degreeU, degreeV;
	int numU, numV;
	std::vector<ControlVertex> CVs;
	std::vector<float> knotsU, knotsV;
	Mesh *surfaceMesh;
	Mesh *curveMesh;
	Mesh *hullMesh;
	VertexMesh *cvMesh;
	std::vector<u8> activeSpansU, activeSpansV;
	int matID;

	Surface(void);
	virtual ~Surface(void);
	virtual void DrawWire(bool active);
	virtual void DrawShaded(void);
	virtual void DrawHull(bool active);
	virtual bool IntersectRay(const mat4 &matrix, vec3 orig, vec3 dir, float &dist) { return surfaceMesh->IntersectRay(matrix, orig, dir, dist); }
	virtual bool IntersectFrustum(const mat4 &matrix, const vec4 *planes) { return surfaceMesh->IntersectFrustum(matrix, planes); }
	virtual void FrustumPickCVs(const mat4 &matrix, const vec4 *planes, std::vector<Pickable*> &cvs);
	void UpdateHull(void);
	void UpdateCVs(void);
	void UpdateCurve(void);
	void UpdateSurface(void);
	void Update(void);

	vec3 Eval(float u, float v);
	int FindParamU(float u);
	int FindParamV(float v);
};
Node *CreateTestSurface(void);


#define UNIFORMS \
	X(u_world) \
	X(u_normal) \
	X(u_view) \
	X(u_proj) \
	X(u_eyePos) \
	X(u_windowSize) \
	X(u_matColorSelector) \
	X(u_matAmbient) \
	X(u_matDiffuse) \
	X(u_matSpecular) \
	X(u_matEmissive) \
	X(u_matShininess) \
	X(u_ambient) \
	X(u_lightDiffuse) \
	X(u_lightSpecular) \
	X(u_lightDirection)

struct Program
{
	i32 program;
#define X(uniform) i32 uniform;
UNIFORMS
#undef X
	void Use(void);
};
extern Program *curProg;
extern Program defProg, cvProg;

extern mat4 proj;
extern mat4 view;
extern mat4 pv;
extern mat4 unpv;
extern mat4 worldMat;
extern mat4 normalMat;
extern vec3 eyePos;

void SetCamera(void);
void SetWorldMatrix(const mat4 &world);
void SetMaterial(const Material &mat);
