#include "ithil.h"
#include "glad/glad.h"
#include "lodepng/lodepng.h"

#include <glm/gtx/intersect.hpp>

#include "imgui.h"
#include "ImGuizmo.h"

#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <map>
#include <vector>
#include <algorithm>

#include "camera.h"

#include <rw.h>
#include <src/rwgta.h>

CCamera camera;

void
printlog(GLuint object)
{
        GLint log_length;
        char *log;

        if (glIsShader(object))
                glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
        else if (glIsProgram(object))
                glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
        else{
                fprintf(stderr, "printlog: Not a shader or a program\n");
                return;
        }

        log = (char*) malloc(log_length);
        if(glIsShader(object))
                glGetShaderInfoLog(object, log_length, NULL, log);
        else if(glIsProgram(object))
                glGetProgramInfoLog(object, log_length, NULL, log);
        fprintf(stderr, "%s", log);
        free(log);
}

GLint
compileshader(GLenum type, const char *src)
{
	GLint shader, success;

	shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if(!success){
		fprintf(stderr, "Error in shader\n");
		printlog(shader);
		return -1;
	}
	return shader;
}

GLint
linkprogram(GLint vs, GLint fs)
{
	GLint program, success;

	program = glCreateProgram();

	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if(!success){
		fprintf(stderr, "glLinkProgram:");
		printlog(program);
		return -1;
	}
	return program;
}


void
InitGL(void *loadproc)
{
        gladLoadGLLoader((GLADloadproc)loadproc);
}

void
InitApp(void)
{
	rw::Engine::init();
	gta::attachPlugins();
	rw::Engine::open(nil);
	rw::Engine::start();
}




Mesh *grid;

Node *sceneRoot;

std::vector<Material> materials;

/* Alias default material:
	color 0 150 255
	diffuse 0.8
   Alias default light
	ambient
		color 100 100 100
		intensity 1.0
		ambient shade 0.5
	directional
		color 255 255 255
		intensity 1.0
*/

Material
DefaultMaterial(void)
{
	Material mat;
	mat.colorSelector = vec4(0.0f);
	mat.ambient = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	mat.diffuse = vec4(0.8f, 0.8f, 0.8f, 1.0f);
	mat.specular = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	mat.emissive = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	mat.shininess = 0.0f;
	return mat;
}

Material defMat;
Material wireMat;
Material gridMat;

Lighting lighting;
Lighting defLighting;

Program *curProg;
Program defProg, cvProg;

void
Program::Use(void)
{
	curProg = this;
	glUseProgram(curProg->program);
}

mat4 proj;
mat4 view;
mat4 pv;
mat4 unpv;
mat4 worldMat;
mat4 normalMat;
vec3 eyePos;

void
SetCamera(void)
{
	ImGuiIO &io = ImGui::GetIO();
	glUniformMatrix4fv(curProg->u_view, 1, GL_FALSE, value_ptr(view));
	glUniformMatrix4fv(curProg->u_proj, 1, GL_FALSE, value_ptr(proj));
	glUniform3fv(curProg->u_eyePos, 1, value_ptr(eyePos));
	glUniform2fv(curProg->u_windowSize, 1, (GLfloat*)&io.DisplaySize);
}

void
SetWorldMatrix(const mat4 &world)
{
	worldMat = world;
	normalMat = glm::inverse(glm::transpose(world));
	glUniformMatrix4fv(curProg->u_world, 1, GL_FALSE, value_ptr(worldMat));
	glUniformMatrix4fv(curProg->u_normal, 1, GL_FALSE, value_ptr(normalMat));
}

void
SetMaterial(const Material &mat)
{
	glUniform4fv(curProg->u_matColorSelector, 1, value_ptr(mat.colorSelector));
	glUniform4fv(curProg->u_matAmbient, 1, value_ptr(mat.ambient));
	glUniform4fv(curProg->u_matDiffuse, 1, value_ptr(mat.diffuse));
	glUniform4fv(curProg->u_matSpecular, 1, value_ptr(mat.specular));
	glUniform4fv(curProg->u_matEmissive, 1, value_ptr(mat.emissive));
	glUniform1f(curProg->u_matShininess, mat.shininess);
}

void
SetLighting(const Lighting &lighting)
{
	glUniform4fv(curProg->u_ambient, 1, value_ptr(lighting.globalAmbient));
	glUniform4fv(curProg->u_lightDiffuse, 1, value_ptr(lighting.diffuse));
	glUniform4fv(curProg->u_lightSpecular, 1, value_ptr(lighting.specular));
	glUniform3fv(curProg->u_lightDirection, 1, value_ptr(lighting.direction));
}

#include "inc/shader.vert.inc"
#include "inc/shader.frag.inc"
#include "inc/cv.vert.inc"
#include "inc/tex.frag.inc"
#include "inc/icons.png.inc"

Texture *CreateTexture(u8 *data, u32 size)
{
	u8 *texdata;
	u32 width, height;
	u32 error = lodepng_decode32(&texdata, &width, &height, data, size);
	if(error) {
		fprintf(stderr, "error: decode png\n");
		return nil;
	}

	Texture *tex = new Texture;
	tex->width = width;
	tex->height = height;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex->tex);
	glTextureStorage2D(tex->tex, 1, GL_RGBA8, width, height);
	glTextureSubImage2D(tex->tex, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, texdata);
	free(texdata);

	return tex;
}
void
Texture::Bind(int n)
{
	glBindTextureUnit(n, tex);
}

Texture *iconsTex;

void
InitScene(void)
{
	GLint vs = compileshader(GL_VERTEX_SHADER, shader_vert_src);
	GLint fs = compileshader(GL_FRAGMENT_SHADER, shader_frag_src);
	defProg.program = linkprogram(vs, fs);
	vs = compileshader(GL_VERTEX_SHADER, cv_vert_src);
	fs = compileshader(GL_FRAGMENT_SHADER, tex_frag_src);
	cvProg.program = linkprogram(vs, fs);

#define X(uniform) defProg.uniform = glGetUniformLocation(defProg.program, #uniform); \
                   cvProg.uniform = glGetUniformLocation(cvProg.program, #uniform);
	UNIFORMS
#undef X

	iconsTex = CreateTexture(icons_png, icons_png_len);

	grid = CreateGrid();
	Mesh *cube = CreateCube();
	Mesh *sphere = CreateSphere(1.0f);
	Mesh *torus = CreateTorus(2.0f, 0.5f);

	camera.m_position = vec3(0.0f, 8.0f, 4.0f);
	camera.m_target = vec3(0.0f, 0.0f, 0.0f);

	ImGuizmo::AllowAxisFlip(false);

	wireMat = DefaultMaterial();
	wireMat.ambient = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	wireMat.diffuse = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	wireMat.emissive = wireColor;
	materials.push_back(wireMat);
	wireMat.emissive = activeColor;
	materials.push_back(wireMat);
	wireMat.emissive = hullColor;
	materials.push_back(wireMat);
	wireMat.emissive = activeHullColor;
	materials.push_back(wireMat);

	defMat = DefaultMaterial();
	defMat.ambient = vec4(vec3(0, 128, 240)/255.0f*0.2f, 1.0f);
	defMat.diffuse = vec4(vec3(0, 128, 240)/255.0f*0.8f, 1.0f);
//	defMat.specular = vec4(vec3(255, 255, 255)/255.0f*0.8f, 1.0f);
//	defMat.shininess = 50.0f;
	materials.push_back(defMat);

	Material fooMat = defMat;
	fooMat.ambient = vec4(vec3(255, 0, 0)/255.0f*0.2f, 1.0f);
	fooMat.diffuse = vec4(vec3(255, 0, 0)/255.0f*0.8f, 1.0f);
	materials.push_back(fooMat);
	fooMat.ambient = vec4(vec3(0, 255, 0)/255.0f*0.2f, 1.0f);
	fooMat.diffuse = vec4(vec3(0, 255, 0)/255.0f*0.8f, 1.0f);
	materials.push_back(fooMat);
	fooMat.ambient = vec4(vec3(0, 0, 255)/255.0f*0.2f, 1.0f);
	fooMat.diffuse = vec4(vec3(0, 0, 255)/255.0f*0.8f, 1.0f);
	materials.push_back(fooMat);
	fooMat.ambient = vec4(vec3(0, 255, 255)/255.0f*0.2f, 1.0f);
	fooMat.diffuse = vec4(vec3(0, 255, 255)/255.0f*0.8f, 1.0f);
	materials.push_back(fooMat);
	fooMat.ambient = vec4(vec3(255, 0, 255)/255.0f*0.2f, 1.0f);
	fooMat.diffuse = vec4(vec3(255, 0, 255)/255.0f*0.8f, 1.0f);
	materials.push_back(fooMat);
	fooMat.ambient = vec4(vec3(255, 255, 0)/255.0f*0.2f, 1.0f);
	fooMat.diffuse = vec4(vec3(255, 255, 0)/255.0f*0.8f, 1.0f);
	materials.push_back(fooMat);
	fooMat.ambient = vec4(vec3(255, 255, 255)/255.0f*0.2f, 1.0f);
	fooMat.diffuse = vec4(vec3(255, 255, 255)/255.0f*0.8f, 1.0f);
	materials.push_back(fooMat);

	gridMat = DefaultMaterial();
	gridMat.ambient = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	gridMat.diffuse = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	gridMat.colorSelector.w = 1.0f;

	lighting.globalAmbient = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	lighting.diffuse = vec4(1.0f, 1.0f, 1.0f, 1.0f);
//	lighting.specular = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	lighting.specular = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	lighting.direction = normalize(vec3(1.0f, -1.0f, -1.0f));
	defLighting = lighting;


	Node *node;
	sceneRoot = new Node("Root");
	node = new Node("cube");
	node->visible = false;
	sceneRoot->AddChild(node);
	node->AttachMesh(cube);
	node->localMatrix = glm::translate(node->localMatrix, vec3(2.0f, 0.0f, 0.0f));
	node = new Node("monkey");
	node->visible = false;
	sceneRoot->AddChild(node);
	Polyset *ps = ReadObjFile("/u/aap/src/objstuff/monkey.obj");
	node->AttachMesh(ps);
	node->localMatrix = glm::translate(node->localMatrix, vec3(0.0f, 4.0f, 0.0f));
	node = new Node("torus");
	node->visible = false;
	sceneRoot->AddChild(node);
	node->AttachMesh(torus);
	node->localMatrix = glm::translate(node->localMatrix, vec3(-1.0f, -1.0f, 0.0f));
	Node *node1 = node;
	node = new Node("node4");
	node1->AddChild(node);
	node = new Node("sphere");
	node->visible = false;
	node1->AddChild(node);
	node->AttachMesh(sphere);
	node->visible = false;
	node->localMatrix = glm::translate(node->localMatrix, vec3(0.0f, 0.0f, 1.0f));

	node = CreateTeapot();
	node->visible = false;
	node->isTreeOpen = false;
	sceneRoot->AddChild(node);

//	node = ReadDffFile("files/kuruma.dff");
//	node->visible = false;
//	sceneRoot->AddChild(node);

	node = CreateTestCurve();
	sceneRoot->AddChild(node);

	node = CreateTestSurface();
	sceneRoot->AddChild(node);
}

std::list<Pickable*> selection;
void
ClearSelection(void)
{
	for(auto const &it : selection)
		it->SetSelected(false);
	selection.clear();
}
bool
IsSelected(Pickable *obj)
{
	return obj->selected;
}
void
Select(Pickable *obj)
{
	if(obj == sceneRoot || IsSelected(obj))
		return;
	obj->SetSelected(true);
	selection.push_front(obj);
}
void
Deselect(Pickable *obj)
{
	if(IsSelected(obj)) {
		selection.remove(obj);
		obj->SetSelected(false);
	}
}
void
ToggleSelection(Pickable *obj)
{
	if(IsSelected(obj))
		Deselect(obj);
	else
		Select(obj);
}



std::vector<Pickable*> pickSet;

void
EnumeratePickables(Node *node, std::vector<Node*> &nodes)
{
	if(!node->visible)
		return;
	if(node->mesh)
		nodes.push_back(node);
	for(Node *c = node->child; c; c = c->next)
		EnumeratePickables(c, nodes);
}

void
RayPickNodes(vec3 orig, vec3 dir)
{
	float dist = 1.0f;
	Node *node = nil;
	float d;
	pickSet.clear();
	std::vector<Node*> nodes;
	EnumeratePickables(sceneRoot, nodes);
	for(u32 i = 0; i < nodes.size(); i++)
		if(nodes[i]->mesh->IntersectRay(nodes[i]->globalMatrix, orig, dir, d) && d < dist) {
			dist = d;
			node = nodes[i];
		}
	if(node)
		pickSet.push_back(node);
}

void
FrustumPickNodes(const vec4 *planes)
{
	pickSet.clear();
	std::vector<Node*> nodes;
	EnumeratePickables(sceneRoot, nodes);
	for(u32 i = 0; i < nodes.size(); i++)
		if(nodes[i]->mesh->IntersectFrustum(nodes[i]->globalMatrix, planes))
			pickSet.push_back(nodes[i]);
}

void
FrustumPickCVs(const vec4 *planes)
{
	pickSet.clear();
	std::vector<Node*> nodes;
	EnumeratePickables(sceneRoot, nodes);
	for(u32 i = 0; i < nodes.size(); i++)
		nodes[i]->mesh->FrustumPickCVs(nodes[i]->globalMatrix, planes, pickSet);
}




enum Pass
{
	NONE,
	SHADE,
	HIDDEN_LINE,
	WIRE,
	SHADE_WIRE,
};
int pass;
bool renderModel = true;
bool renderShade = true;
bool renderHull = true;
bool defaultLight = true;

void
ForceColor(vec4 color, vec4 otherColor)
{
	wireMat.emissive = color;
	wireMat.ambient = otherColor;
	SetMaterial(wireMat);
}

void
DrawNode(Node *node)
{
	if(node->mesh == nil)
		return;

	SetWorldMatrix(node->globalMatrix);

	if(defaultLight) {
		defLighting.direction = normalize(camera.m_target - camera.m_position);
		SetLighting(defLighting);
	} else
		SetLighting(lighting);
	SetMaterial(defMat);

	switch(pass) {
	case HIDDEN_LINE:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		node->mesh->DrawShaded();
		glDisable(GL_BLEND);
		break;

	case SHADE:
	case SHADE_WIRE:
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0.0f, 200.0f);
		node->mesh->DrawShaded();
		glDisable(GL_POLYGON_OFFSET_FILL);
		if(pass != SHADE_WIRE)
			break;
	case WIRE:
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		node->mesh->DrawWire(IsSelected(node));
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		break;
	}
	if(renderHull)
		node->mesh->DrawHull(IsSelected(node));
}

void
DrawNodeAndChildren(Node *node)
{
	if(!node->visible)
		return;
	DrawNode(node);
	for(Node *c = node->child; c; c = c->next)
		DrawNodeAndChildren(c);
}

void
RenderScene(void)
{
	mat4 world;

	sceneRoot->UpdateMatrices();

	camera.m_aspectRatio = (float)display_w/display_h;
	camera.m_fov = 1.0f;
	camera.Process();
	camera.update();

	proj = camera.m_projMat;
	view = camera.m_viewMat;
	pv = proj*view;
	unpv = glm::inverse(pv);
	eyePos = camera.m_position;

	glEnable(GL_DEPTH_TEST);

	defProg.Use();
	SetCamera();

	if(renderModel && renderShade) {
		pass = SHADE_WIRE;
		DrawNodeAndChildren(sceneRoot);
	} else if(renderModel) {
		pass = WIRE;
		DrawNodeAndChildren(sceneRoot);
	} else if(renderShade) {
		pass = SHADE;
		DrawNodeAndChildren(sceneRoot);
	} else {
		pass = NONE;
		// Hull and stuff
		DrawNodeAndChildren(sceneRoot);
	}

	world = mat4(1.0f);
	SetWorldMatrix(world);
	SetMaterial(gridMat);
	grid->DrawRaw();

	glBindVertexArray(0);
}





Node *dragParent, *dragChild;
void
HierarchyPanel(Node *node)
{
	ImGuiIO &io = ImGui::GetIO();
	Node *child = nil;
	ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if(node->isTreeOpen)
		node_flags |= ImGuiTreeNodeFlags_DefaultOpen;
	if(IsSelected(node))
		node_flags |= ImGuiTreeNodeFlags_Selected;
	if(node->child == nil)
		node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	node->isTreeOpen = ImGui::TreeNodeEx(node, node_flags, node->name);
	if(ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		if(io.KeyAlt)
			Deselect(node);
		else if(io.KeyCtrl)
			ToggleSelection(node);
		else if(io.KeyShift)
			Select(node);
		else {
			ClearSelection();
			Select(node);
		}
	}
	if(ImGui::BeginDragDropSource()) {
		ImGui::SetDragDropPayload("_TREENODE", &node, sizeof(Node*));
//		ImGui::Text("This is a drag and drop source");
		ImGui::EndDragDropSource();
	}
	if(ImGui::BeginDragDropTarget()) {
		const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("_TREENODE");
		if(payload) {
			memcpy(&child, payload->Data, sizeof(child));
			dragParent = node;
			dragChild = child;
		}
		ImGui::EndDragDropTarget();
	}
	if(node->child && node->isTreeOpen){
		for(Node *child = node->child; child; child = child->next)
			HierarchyPanel(child);
		ImGui::TreePop();
	}
}

static ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
static ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::WORLD;
void
TransformPanel(mat4 &matrix)
{
	if(ImGui::IsKeyPressed(ImGuiKey_W))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if(ImGui::IsKeyPressed(ImGuiKey_E))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if(ImGui::IsKeyPressed(ImGuiKey_R))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	if(ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if(ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if(ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(value_ptr(matrix), matrixTranslation, matrixRotation, matrixScale);
	ImGui::InputFloat3("Tr", matrixTranslation);
	ImGui::InputFloat3("Rt", matrixRotation);
	ImGui::InputFloat3("Sc", matrixScale);
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, value_ptr(matrix));

	if(mCurrentGizmoOperation != ImGuizmo::SCALE) {
		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
	}
}

bool showHierarchyWindow = true;
bool showMaterialWindow = false;
bool showDemoWindow = false;


int dragging;
bool startDragging;
bool stopDragging;
bool dragCtrl;
bool dragShift;
bool dragAlt;
vec2 dragStart, dragEnd, dragDelta;

void
DragStuff(void)
{
	ImGuiIO &io = ImGui::GetIO();
	if(stopDragging)
		dragging = 0;
	startDragging = false;
	stopDragging = false;
	if(!dragging) {
//		if(!io.WantCaptureMouse && !io.WantCaptureKeyboard && ModNone()) {
		if(!io.WantCaptureMouse && !io.WantCaptureKeyboard) {
			if(ImGui::IsMouseDragging(0, 0.0f))
				dragging = 1;
			else if(ImGui::IsMouseDragging(1, 0.0f))
				dragging = 2;
			else if(ImGui::IsMouseDragging(2, 0.0f))
				dragging = 3;
		}
		if(dragging) {
			dragCtrl = io.KeyCtrl;
			dragShift = io.KeyShift;
			dragAlt = io.KeyAlt;
			dragStart = ImGui::GetMousePos();
			dragEnd = ImGui::GetMousePos();
			startDragging = true;
//printf("start dragging %d   %d %d %d\n", dragging, dragCtrl, dragShift, dragAlt);
		}
	} else {
		dragDelta = vec2(ImGui::GetMousePos()) - dragEnd;
		dragEnd = ImGui::GetMousePos();
		if(!ImGui::IsMouseDragging(dragging-1, 0.0f)) {
			stopDragging = true;
//printf("stop dragging\n");
		}
	}
}


int
GetMarkingSelection(void)
{
	vec2 dist = dragEnd - dragStart;
	if(length(dist) < 20.0f)
		return -1;

	float angle = atan2(-dist.y, dist.x);
	angle += TAU/16.0f;
	if(angle < 0.0f) angle += TAU;
	return angle/TAU*8;
}
vec3
NormalizedToScreen(vec3 p)
{
	ImGuiIO &io = ImGui::GetIO();
	p.x = (p.x+1.0f)/2.0f * io.DisplaySize.x;
	p.y = (-p.y+1.0f)/2.0f * io.DisplaySize.y;
	return p;
}

vec3
ScreenToWorld(vec2 screen, float z)
{
	ImGuiIO &io = ImGui::GetIO();
	float x = (screen.x/io.DisplaySize.x)*2.0f - 1.0f;
	float y = -((screen.y/io.DisplaySize.y)*2.0f - 1.0f);
	vec4 p = vec4(x, y, z, 1.0f);
	p = unpv * p;
	p = p / p.w;
	return p;
}

vec4
plane(vec3 p1, vec3 p2, vec3 p3)
{
	vec3 n = normalize(cross(p2-p1, p3-p1));
	float d = dot(n, p1);
	return vec4(n, -d);
}

struct Tool
{
	virtual const char *ParentMenu(void) = 0;
	virtual void Use(void) = 0;
};
struct PickTool : public Tool
{
	virtual const char *ParentMenu(void) { return "Pick"; }
	virtual void Use(void);
} pickTool;
struct PickCVTool : public Tool
{
	virtual const char *ParentMenu(void) { return "Pick"; }
	virtual void Use(void);
} pickCVTool;


struct XformTool : public Tool
{
	virtual const char *ParentMenu(void) { return "Xform"; }
	virtual void Use(void);
	virtual void Xform(void) = 0;
};
struct TranslateTool : public XformTool
{
	virtual void Xform(void);
} translateTool;
struct RotateTool : public XformTool
{
	virtual void Xform(void);
} rotateTool;
struct ScaleTool : public XformTool
{
	virtual void Xform(void);
} scaleTool;

Tool *currentTool = &pickTool;
Tool *lastPickTool = &pickTool;		// hack

void
PickTool::Use(void)
{
	if(ModNone()) {
		if(dragging) {
			ImDrawList *draw_list = ImGui::GetBackgroundDrawList();
			draw_list->AddRect(dragStart, dragEnd, 0xFFFFFFFF, 0.0f, ImDrawFlags_None, 1.0f);
		}
		if(stopDragging) {
			float minx = min(dragStart.x, dragEnd.x);
			float maxx = max(dragStart.x, dragEnd.x);
			float miny = min(dragStart.y, dragEnd.y);
			float maxy = max(dragStart.y, dragEnd.y);
			if(maxx-minx == 0.0f && maxy-miny == 0.0f) {
				vec3 pf = ScreenToWorld(vec2(minx,miny), 1.0f);
				vec3 pn = ScreenToWorld(vec2(minx,miny), -1.0f);
				RayPickNodes(pn, pf-pn);
			} else {
				if(minx == maxx) maxx += 0.1f;
				if(miny == maxy) maxy += 0.1f;

				// get frustum points
				vec3 tln = ScreenToWorld(vec2(minx, miny), -1.0f);
				vec3 trn = ScreenToWorld(vec2(maxx, miny), -1.0f);
				vec3 bln = ScreenToWorld(vec2(minx, maxy), -1.0f);
				vec3 brn = ScreenToWorld(vec2(maxx, maxy), -1.0f);
				vec3 tlf = ScreenToWorld(vec2(minx, miny), 1.0f);
				vec3 trf = ScreenToWorld(vec2(maxx, miny), 1.0f);
				vec3 blf = ScreenToWorld(vec2(minx, maxy), 1.0f);
				vec3 brf = ScreenToWorld(vec2(maxx, maxy), 1.0f);

				vec4 planes[] = {
					plane(trn, brn, tln),	// near
					plane(brf, trf, blf),	// far
					plane(brn, trn, brf),	// right
					plane(trn, tln, trf),	// top
					plane(tln, bln, tlf),	// left
					plane(bln, brn, blf),	// bottom
				};

				FrustumPickNodes(planes);
			}

			switch(dragging) {
			case 1:
				for(auto const &it : pickSet)
					ToggleSelection(it);
				break;
			case 2:
				for(auto const &it : pickSet)
					Deselect(it);
				break;
			case 3:
				ClearSelection();
				for(auto const &it : pickSet)
					Select(it);
				break;
			}
			pickSet.clear();
		}
	}
}

void
PickCVTool::Use(void)
{
	if(ModNone()) {
		if(dragging) {
			ImDrawList *draw_list = ImGui::GetBackgroundDrawList();
			draw_list->AddRect(dragStart, dragEnd, 0xFFFFFFFF, 0.0f, ImDrawFlags_None, 1.0f);
		}
		if(stopDragging) {
			float minx = min(dragStart.x, dragEnd.x);
			float maxx = max(dragStart.x, dragEnd.x);
			float miny = min(dragStart.y, dragEnd.y);
			float maxy = max(dragStart.y, dragEnd.y);
			if(maxx-minx == 0.0f && maxy-miny == 0.0f) {
				vec3 pf = ScreenToWorld(vec2(minx,miny), 1.0f);
				vec3 pn = ScreenToWorld(vec2(minx,miny), -1.0f);
//				RayPickNodes(pn, pf-pn);
			} else {
				if(minx == maxx) maxx += 0.1f;
				if(miny == maxy) maxy += 0.1f;

				// get frustum points
				vec3 tln = ScreenToWorld(vec2(minx, miny), -1.0f);
				vec3 trn = ScreenToWorld(vec2(maxx, miny), -1.0f);
				vec3 bln = ScreenToWorld(vec2(minx, maxy), -1.0f);
				vec3 brn = ScreenToWorld(vec2(maxx, maxy), -1.0f);
				vec3 tlf = ScreenToWorld(vec2(minx, miny), 1.0f);
				vec3 trf = ScreenToWorld(vec2(maxx, miny), 1.0f);
				vec3 blf = ScreenToWorld(vec2(minx, maxy), 1.0f);
				vec3 brf = ScreenToWorld(vec2(maxx, maxy), 1.0f);

				vec4 planes[] = {
					plane(trn, brn, tln),	// near
					plane(brf, trf, blf),	// far
					plane(brn, trn, brf),	// right
					plane(trn, tln, trf),	// top
					plane(tln, bln, tlf),	// left
					plane(bln, brn, blf),	// bottom
				};

				FrustumPickCVs(planes);
			}

			switch(dragging) {
			case 1:
				for(auto const &it : pickSet)
					ToggleSelection(it);
				break;
			case 2:
				for(auto const &it : pickSet)
					Deselect(it);
				break;
			case 3:
				ClearSelection();
				for(auto const &it : pickSet)
					Select(it);
				break;
			}
			pickSet.clear();
		}
	}
}

// colors: background: 161 161 161
//         grid mesh: 142 142 142
//         grid center: 0 0 0
//         model: 0 4 96		255 255 255
//         mesh: 138 72 51		255 255 255
//         hull: 138 72 51		228 172 121
//		...
//         axes: 204 R, 150 G, 255 B

// TODO: this is probably wrong
// when objects are updated in wrong order
// also transforming objects and their components at the same time
void
TransformSelection(const mat4 &delta)
{
	for(auto const &it : selection)
		it->TransformDelta(delta);
	for(auto const &it : selection) {
		Node *node = dynamic_cast<Node*>(it);
		if(node)
			node->RecalculateLocal();
	}
}

void
XformTool::Use(void)
{
	if(dragShift) {
		lastPickTool->Use();
		return;
	}

	// TOOD: use lastPickTool here
	if(ModNone()) {
		if(!selection.empty() && dragging) {
			Xform();
		}
		if(selection.empty() && stopDragging && dragStart.x == dragEnd.x && dragStart.y == dragEnd.y) {
			vec3 pf = ScreenToWorld(dragEnd, 1.0f);
			vec3 pn = ScreenToWorld(dragEnd, -1.0f);
			RayPickNodes(pn, pf-pn);
			for(auto const &it : pickSet)
				Select(it);
			pickSet.clear();
		}
	}
}

void
TranslateTool::Xform(void)
{
	// indexed by mouse button
	vec3 axes[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } };

	static vec3 prevPoint;
	mat4 delta;

	vec3 pf = ScreenToWorld(dragEnd, 1.0f);
	vec3 pn = ScreenToWorld(dragEnd, -1.0f);
	vec3 dir = normalize(pf-pn);
	float rayDist;
	// TODO: This still sucks a bit
	if(dragging == 2) {
		vec3 normal = camera.m_position - camera.m_target;
		normal.z = 0.0f;
		intersectRayPlane(pn, dir, vec3(0.0f), normalize(normal), rayDist);
	} else
		intersectRayPlane(pn, dir, vec3(0.0f), vec3(0.0f, 0.0f, 1.0f), rayDist);
	vec3 point = pn + rayDist*dir;

	vec3 dvec = point - prevPoint;
	prevPoint = point;
	if(startDragging) return;

	vec3 xxx = axes[dragging-1]*dot(axes[dragging-1], dvec);
	delta = glm::translate(mat4(1.0f), xxx);

	TransformSelection(delta);
}

float xformSensitivity = 1/100.0f;

void
RotateTool::Xform(void)
{
	// indexed by mouse button
	vec3 axes[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } };

	mat4 delta;

	vec3 pivot = selection.front()->GetPosition();
	float d = xformSensitivity*(dragDelta.x - dragDelta.y);
	delta = mat4(1.0f);
	delta = glm::translate(delta, pivot);
	delta = glm::rotate(delta, d, axes[dragging-1]);
	delta = glm::translate(delta, -pivot);

	TransformSelection(delta);
}

void
ScaleTool::Xform(void)
{
	mat4 delta;

	vec3 pivot = selection.front()->GetPosition();
	float d = xformSensitivity*(dragDelta.x - dragDelta.y);
	delta = mat4(1.0f);
	delta = glm::translate(delta, pivot);
	delta = glm::scale(delta, vec3(1.0f+d));
	delta = glm::translate(delta, -pivot);

	TransformSelection(delta);
}

bool
MarkingButton(const char *id, float x, float y, bool active)
{
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
//ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings;

	ImGuiStyle style = ImGui::GetStyle();
	ImVec2 pad = style.FramePadding;
	ImVec2 textsz = ImGui::CalcTextSize(id);
	ImVec2 btnsz(textsz.x+2*pad.x, textsz.y+2*pad.y);

	ImGui::SetNextWindowPos(dragStart, 0, ImVec2(x, y));
//	ImGui::SetNextWindowSize(textsz);
	static char winstr[256];
	sprintf(winstr, "%s_win", id);
	ImGui::Begin(winstr, nil, flags);

	ImVec2 oldPos = ImGui::GetCursorPos();
	ImGui::SetCursorPos(oldPos);
	ImVec2 cursor = ImGui::GetCursorScreenPos();


	ImGui::InvisibleButton(id, ImVec2(textsz.x+2*pad.x, textsz.y+2*pad.y));
// BUG: this can highlight two different buttons
	bool hovering = active || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped);
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	ImU32 col = hovering ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : 0xFF000000;
	draw_list->AddRectFilled(ImVec2(cursor.x, cursor.y), ImVec2(cursor.x+2*pad.x+textsz.x, cursor.y+2*pad.y+textsz.y), col, 0.0f, ImDrawFlags_None);

	ImGui::SetCursorPos(ImVec2(oldPos.x+pad.x, oldPos.y+pad.y));
	ImGui::Text(id);

	ImGui::End();

	return active && stopDragging;
}

void
MarkingMenu(void)
{
	if(ModMarking() && dragging) {
		int sector = GetMarkingSelection();

/* for reference
		MarkingButton("button1",  0.5f,  2.0f, sector == 2);
		MarkingButton("button2", -1.0f,  0.5f, sector == 0);
		MarkingButton("button3",  0.5f, -1.0f, sector == 6);
		MarkingButton("button4",  2.0f,  0.5f, sector == 4);

		MarkingButton("button5", -1.0f,  2.0f, sector == 1);
		MarkingButton("button6", -1.0f, -1.0f, sector == 7);
		MarkingButton("button7",  2.0f, -1.0f, sector == 5);
		MarkingButton("button8",  2.0f,  2.0f, sector == 3);
*/
		if(dragging == 1) {
			if(MarkingButton("Nothing",  0.5f,  2.0f, sector == 2))
				ClearSelection();
			if(MarkingButton("CV", -1.0f,  0.5f, sector == 0))
				currentTool = lastPickTool = &pickCVTool;
			if(MarkingButton("Object", 0.5f, -1.0f, sector == 6))
				currentTool = lastPickTool = &pickTool;
		} else if(dragging == 3) {
			if(MarkingButton("Move",  0.5f,  2.0f, sector == 2))
				currentTool = &translateTool;
			if(MarkingButton("Scale", -1.0f,  0.5f, sector == 0))
				currentTool = &scaleTool;
			if(MarkingButton("Rotate", 0.5f, -1.0f, sector == 6))
				currentTool = &rotateTool;
		} else if(dragging == 2) {
			if(MarkingButton("Toggle Model", 0.5f,  2.0f, sector == 2))
				renderModel = !renderModel;
			if(MarkingButton("Toggle Shade", -1.0f,  0.5f, sector == 0))
				renderShade = !renderShade;
			if(MarkingButton("Default Light", -1.0f,  2.0f, sector == 1))
				defaultLight = !defaultLight;
			if(MarkingButton("Toggle Hull", 0.5f,  -1.0f, sector == 6))
				renderHull = !renderHull;
		}

		ImDrawList *draw_list = ImGui::GetForegroundDrawList();
		draw_list->AddLine(dragStart, dragEnd, 0xFF00FFFF, 3.0f);
	}
}

void
DummyWidth(float w)
{
	ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::Dummy(ImVec2(w,10));
	ImGui::SetCursorPos(cursor);
}

struct AlMenuState
{
	bool justClosed;
	bool closeNextFrame;
	ImU32 lastSelection;
	const char *label;

	AlMenuState(void) : justClosed(false), closeNextFrame(false), lastSelection(0), label(nil) {}
};
AlMenuState *currentAlMenu;

std::map<u32,AlMenuState> alMenus;
bool popupDefaultAction;

ImVec2 popupSize;
ImVec2 popupPos;
bool
BeginAlMenu(const char *id)
{
	static int frame;

	currentAlMenu = &alMenus[ImGui::GetID(id)];

	ImVec2 sz(76,60);
	ImVec2 tmpPos = ImGui::GetCursorScreenPos();
	bool hilight = strcmp(id, currentTool->ParentMenu()) == 0;
	if(hilight)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
	ImGui::Button(id, sz);
	if(hilight)
		ImGui::PopStyleColor();
	popupDefaultAction = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && currentAlMenu->closeNextFrame;
	currentAlMenu->justClosed = false;
	// as a hack open the popup briefly to initialize the default action. will close immediately
	if(currentAlMenu->label == nil || ImGui::IsItemActive()) {
		popupPos = tmpPos;
		frame = 0;
		ImGui::OpenPopup(id);
	}

	// don't show immediately
	if(frame < 10)
		ImGui::SetNextWindowPos(ImVec2(-1000,-1000));
	else
		ImGui::SetNextWindowPos(ImVec2(popupPos.x, popupPos.y-popupSize.y));
	if(ImGui::BeginPopup(id)) {
		popupSize = ImGui::GetWindowSize();
		frame++;

		DummyWidth(200.0f);

		return true;
	}

	return false;
}

void
EndAlMenu(void)
{
	if(currentAlMenu->closeNextFrame) {
		currentAlMenu->justClosed = true;
		ImGui::CloseCurrentPopup();
	}
	currentAlMenu->closeNextFrame = false;
// BUG: this doesn't work with our opening hack above
	if(ImGui::IsMouseReleased(0) && !currentAlMenu->justClosed)
		currentAlMenu->closeNextFrame = true;
	ImGui::EndPopup();
}

// what an absolutely horrible function
bool
AlMenuEntry(const char *id, bool *pOptions = nil, bool *pToggle = nil)
{
	bool ret = false;
	ImU32 nID = ImGui::GetID(id);

	// set a default selection if there is none
	if(currentAlMenu->lastSelection == 0) {
		currentAlMenu->lastSelection = nID;
		currentAlMenu->label = id;
	}

	// draw option box in the most horrible way
	ImVec2 rmin, rmax;
	if(pOptions) {
		*pOptions = false;
		ImVec2 cur = ImGui::GetCursorPos();
		rmin = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##options", ImVec2(15,15));
		ImU32 col = ImGui::GetColorU32(ImGuiCol_Header);
		if(ImGui::IsItemHovered()) {
			col = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
			if(currentAlMenu->closeNextFrame) {
				currentAlMenu->lastSelection = nID;
				currentAlMenu->label = id;
				*pOptions = true;
				ret = true;
			}
		}
		rmin.x -= 1;
		rmin.y -= 1;
		rmax = ImVec2(rmin.x + 15, rmin.y + 15);
		ImGui::GetWindowDrawList()->AddRectFilled(rmin, rmax, col);

		cur.x += 20;
		ImGui::SetCursorPos(cur);
	}

	bool selected = currentAlMenu->lastSelection == nID;
	if(popupDefaultAction && selected) {
		if(pToggle)
			*pToggle = !*pToggle;
		ret = true;
	}

	// what a horrible hackjob. need to figure out these numbers
	if(selected) {
		rmin = ImGui::GetCursorScreenPos();
		rmin.x -= 4;
		rmin.y -= 2;
		rmax = ImVec2(rmin.x + ImGui::GetContentRegionAvail().x, rmin.y + ImGui::GetFrameHeight());
		rmax.x += 8;
		rmax.y -= 2;
		ImGui::GetWindowDrawList()->AddRectFilled(rmin, rmax, ImGui::GetColorU32(ImGuiCol_Header));
	}
	if(ImGui::MenuItem(id, "", pToggle)) {
		currentAlMenu->lastSelection = nID;
		currentAlMenu->label = id;
		ret = true;
		currentAlMenu->justClosed = true;
	}

	return ret;
}

void
AlMenuIndicator(const char *id)
{
	AlMenuState *menu = &alMenus[ImGui::GetID(id)];
	ImVec2 sz(76,20);
	if(menu->label)
		ImGui::Button(menu->label, sz);
	else
		ImGui::Button("##", sz);
	ImGui::SameLine();
}

void
ControlWindow()
{
	ImGuiIO &io = ImGui::GetIO();

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings;
	ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y-100));
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 100));
	ImGui::Begin("Control", nil, flags);

	if(BeginAlMenu("File")) {
		if(AlMenuEntry("Open"))
			printf("open\n");
		if(AlMenuEntry("Save"))
			printf("save\n");
		EndAlMenu();
	}
	ImGui::SameLine();
	if(BeginAlMenu("Pick")) {
		bool options = false;
		if(AlMenuEntry("Nothing"))
			ClearSelection();
//		if(AlMenuEntry("Object", &options))
		if(AlMenuEntry("Object"))
			currentTool = lastPickTool = &pickTool;
		if(AlMenuEntry("CVs"))
			currentTool = lastPickTool = &pickCVTool;
		EndAlMenu();
	}
	ImGui::SameLine();
	if(BeginAlMenu("Xform")) {
		if(AlMenuEntry("Translate"))
			currentTool = &translateTool;
		if(AlMenuEntry("Rotate"))
			currentTool = &rotateTool;
		if(AlMenuEntry("Scale"))
			currentTool = &scaleTool;
		EndAlMenu();
	}
	ImGui::SameLine();
	if(BeginAlMenu("Window")) {
		AlMenuEntry("Hierarchy", nil, &showHierarchyWindow);
		AlMenuEntry("Material", nil, &showMaterialWindow);
		AlMenuEntry("Demo", nil, &showDemoWindow);
		EndAlMenu();
	}
	ImGui::SameLine();
	if(BeginAlMenu("Disp\nTools")) {
		AlMenuEntry("Toggle Model", nil, &renderModel);
		AlMenuEntry("Toggle Shade", nil, &renderShade);
		AlMenuEntry("Toggle Hull", nil, &renderHull);
		EndAlMenu();
	}

	AlMenuIndicator("File");
	ImGui::SameLine();
	AlMenuIndicator("Pick");
	ImGui::SameLine();
	AlMenuIndicator("Xform");
	ImGui::SameLine();
	AlMenuIndicator("Window");
	ImGui::SameLine();
	AlMenuIndicator("Disp\nTools");

	ImGui::End();
}


#include <string>
#include <filesystem>

std::vector<std::string> dirEntries;
std::filesystem::path currentPath = "/u/aap";

bool
WalkDirectory(const std::string &entry)
{
	using namespace std;
	filesystem::path newPath;
	if(entry == "..")
		newPath = currentPath.parent_path();
	else
		newPath = currentPath / entry;
printf("new path: %s\n", newPath.c_str());
	if(filesystem::is_directory(newPath)) {
		currentPath = newPath;
		dirEntries.clear();
		dirEntries.push_back("..");
		for(const auto &entry : filesystem::directory_iterator(currentPath))
			dirEntries.push_back(entry.path().filename());
		sort(dirEntries.begin(), dirEntries.end());
	}
	return true;
}

void
FileWindow(void)
{
	ImGui::Begin("Browse");

	if(dirEntries.empty()) {
		dirEntries.push_back("..");
		for(const auto &entry : std::filesystem::directory_iterator(currentPath))
			dirEntries.push_back(entry.path().filename());
		std::sort(dirEntries.begin(), dirEntries.end());
	}

	std::string walkTo;
	ImGui::Text(currentPath.c_str());
	for(auto const &it : dirEntries) {
		if(ImGui::Selectable(it.c_str())) {
			walkTo = it;
		}
	}
	if(walkTo != "")
		WalkDirectory(walkTo);
	ImGui::End();
}

int selectedMaterial = -1;

void
MaterialWindow(void)
{
	int i = 0;
	for(int j = MATID_DEFAULT; j < (int)materials.size(); j++) {
		static char mname[256];
		sprintf(mname, "material_%d", i);

		bool sel = selectedMaterial == j;
		if(ImGui::Selectable(mname, &sel))
			selectedMaterial = j;
		i++;
	}

	if(selectedMaterial < 0)
		return;

	Material *m = &materials[selectedMaterial];
	ImGui::Text("material");
	ImGui::ColorEdit4("Ambient", (float*)&m->ambient);
	ImGui::ColorEdit4("Diffuse", (float*)&m->diffuse);
	ImGui::ColorEdit4("Specular", (float*)&m->specular);
	ImGui::ColorEdit4("Emissive", (float*)&m->emissive);
	ImGui::DragFloat("Shininess", &m->shininess);
}

void
GUI(void)
{
	ImGuiIO &io = ImGui::GetIO();

	DragStuff();
	currentTool->Use();
	MarkingMenu();

	ControlWindow();

//	FileWindow();

	if(showHierarchyWindow) {
		ImGui::Begin("Hierarchy", &showHierarchyWindow);

		dragParent = nil;
		dragChild = nil;
		HierarchyPanel(sceneRoot);
		if(dragParent && dragChild) {
			if(!dragChild->IsHigherThan(dragParent))
				dragParent->AddChild(dragChild);
		}
		if(!selection.empty()) {
			Node *node = dynamic_cast<Node*>(selection.front());
			if(node) {
				ImGui::Checkbox("visible", &node->visible);
				if(mCurrentGizmoMode == ImGuizmo::WORLD) {
					mat4 inv = node->parent ? inverse(node->parent->globalMatrix) : mat4(1.0f);
					TransformPanel(node->globalMatrix);
					node->localMatrix = inv * node->globalMatrix;
				} else {
					TransformPanel(node->localMatrix);
					node->UpdateMatrices();
				}
			}
		}

		ImGui::End();
	}

	if(showMaterialWindow) {
		ImGui::Begin("Materials", &showMaterialWindow);
		MaterialWindow();
		ImGui::End();
	}

	if(showDemoWindow)
		ImGui::ShowDemoWindow(&showDemoWindow);

	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

	// this sucks quite a bit
	if(!selection.empty()) {
		Node *node = dynamic_cast<Node*>(selection.front());
		if(node) {
			mat4 xform = node->globalMatrix;
			mat4 prev = xform;
			ImGuizmo::Manipulate(value_ptr(view), value_ptr(proj), mCurrentGizmoOperation, mCurrentGizmoMode,
						value_ptr(xform), NULL, NULL);
			if(ImGuizmo::IsUsing()) {
				mat4 delta = xform * inverse(prev);
				TransformSelection(delta);
			}
		}
	}
}
