#include <stdio.h>
#include <imgui.h>
#include "ImGuizmo.h"

#include "ithil.h"

#include "camera.h"

using namespace glm;

#define DEGTORAD(r) ((r)/180.0f*M_PI)

bool enableGizmo;

void
CCamera::Process(void)
{
	ImGuiIO &io = ImGui::GetIO();

	if(dragging && ModCamera()) {
		float scale = 1.0f/io.Framerate;
		float mouseSens = scale*10.0f;
		float zoomSens = 0.5f;

		// Mouse
		float dx = -io.MouseDelta.x;
		float dy = -io.MouseDelta.y;
		if(dragging == 1) {
			orbit(DEGTORAD(dx)*mouseSens, -DEGTORAD(dy)*mouseSens);
		} else if(dragging == 2) {
			zoom(dy*mouseSens*zoomSens);
		} else if(dragging == 3) {
			float dist = distanceToTarget();
			pan(dx*mouseSens*dist/100.0f, -dy*mouseSens*dist/100.0f);
		}
	}

#if 0
	bool overGizmo = enableGizmo && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());
	bool overWindow = (io.WantCaptureMouse || io.WantCaptureKeyboard) && !overGizmo;
	if(overWindow || draggingR || draggingL) {
		enableGizmo = true;
		ImGuizmo::Enable(enableGizmo);
		return;
	}

	float scale = 1.0f/io.Framerate;
	float mouseSens = scale*10.0f;
	float zoomSens = 0.5f;
//	float sensitivity = 1.0f;

	// Mouse
	float dx = -io.MouseDelta.x;
	float dy = -io.MouseDelta.y;
#if 0
	// first person
	if(io.MouseDown[0]){
		if(/*input.AltDown() && */io.KeyCtrl){
			dolly(dy*mouseSens);
		}else{
			turn(DEGTORAD(dx)/2.0f*mouseSens, DEGTORAD(dy)/2.0f*mouseSens);
		}
	}
#endif

	// roughly Alias/Maya controls
	if(ModCamera()) {
		enableGizmo = true;
		if(io.MouseDown[0]) {
			orbit(DEGTORAD(dx)*mouseSens, -DEGTORAD(dy)*mouseSens);
			enableGizmo = false;
		} else if(io.MouseDown[1]) {
			zoom(dy*mouseSens*zoomSens);
			enableGizmo = false;
		} else if(io.MouseDown[2]) {
			float dist = distanceToTarget();
			pan(dx*mouseSens*dist/100.0f, -dy*mouseSens*dist/100.0f);
			enableGizmo = false;
		}
		ImGuizmo::Enable(enableGizmo);
	}
#endif

#if 0
	float maxSpeed = 500.0f;
	float speedInc = 2.0f;

	// Keyboard
	static float speed = 0.0f;
	if(input.KeyDown('W'))
		speed += speedInc;
	else if(input.KeyDown('S'))
		speed -= speedInc;
	else
		speed = 0.0f;
	if(speed > maxSpeed) speed = maxSpeed;
	if(speed < -maxSpeed) speed = -maxSpeed;
	dolly(speed*scale);

	static float sidespeed = 0.0f;
	if(input.KeyDown('A'))
		sidespeed -= speedInc;
	else if(input.KeyDown('D'))
		sidespeed += speedInc;
	else
		sidespeed = 0.0f;
	if(sidespeed > maxSpeed) sidespeed = maxSpeed;
	if(sidespeed < -maxSpeed) sidespeed = -maxSpeed;
	pan(sidespeed*scale, 0.0f);


	if(input.KeyJustDown('P'))
		printf("campos: %.2f, %.2f, %.2f\n", m_position.x, m_position.y, m_position.z);
/*
	// Pad
	CPad *pad = CPad::GetPad(0);
	sensitivity = 1.0f;
	if(pad->NewState.r2){
		sensitivity = 2.0f;
		if(pad->NewState.l2)
			sensitivity = 4.0f;
	}else if(pad->NewState.l2)
		sensitivity = 0.5f;
	if(pad->NewState.square) zoom(0.4f*sensitivity*scale);
	if(pad->NewState.Cross) zoom(-0.4f*sensitivity*scale);
	orbit(pad->NewState.getLeftX()/25.0f*sensitivity*scale,
	                -pad->NewState.getLeftY()/25.0f*sensitivity*scale);
	turn(-pad->NewState.getRightX()/25.0f*sensitivity*scale,
	               pad->NewState.getRightY()/25.0f*sensitivity*scale);
	if(pad->NewState.up)
		dolly(2.0f*sensitivity*scale);
	if(pad->NewState.down)
		dolly(-2.0f*sensitivity*scale);

//	if(IsButtonJustDown(pad, start)){
//		printf("cam.position: %f, %f, %f\n", m_position.x, m_position.y, m_position.z);
//		printf("cam.target: %f, %f, %f\n", m_target.x, m_target.y, m_target.z);
//	}
*/
#endif
}

/*
void
CCamera::DrawTarget(void)
{
	float dist = distanceToTarget()/20.0f;
	rw::Vector x = { dist, 0.0f, 0.0f };
	rw::Vector y = { 0.0f, dist, 0.0f };
	rw::Vector z = { 0.0f, 0.0f, dist };
	RenderAxesWidget(this->m_target, x, y, z);
}
*/

void
CCamera::update(void)
{
	m_viewMat = glm::lookAt(m_position, m_target, m_localup);
	m_projMat = glm::perspective(m_fov, m_aspectRatio, m_near, m_far);
}


void
CCamera::setTarget(vec3 target)
{
	m_position = m_position - (m_target - target);
	m_target = target;
}

float
CCamera::getHeading(void)
{
	vec3 dir = m_target - m_position;
	float a = atan2(dir.y, dir.x)-PI/2.0f;
	return m_localup.z < 0.0f ? a-PI : a;
}

void
CCamera::turn(float yaw, float pitch)
{
	vec3 dir = m_target - m_position;
	vec3 zaxis = { 0.0f, 0.0f, 1.0f };
	quat r = angleAxis(yaw, zaxis);
	dir = r * dir; //Rotate(dir, r);
	m_localup = r * m_localup; //Rotate(m_localup, r);

	vec3 right = normalize(cross(dir, m_localup));
	r = angleAxis(pitch, right);
	dir = r * dir; //Rotate(dir, r);
	m_localup = normalize(cross(right, dir));
	if(m_localup.z >= 0.0) m_up.z = 1.0;
	else m_up.z = -1.0f;

	m_target = m_position + dir;
}

void
CCamera::orbit(float yaw, float pitch)
{
	vec3 dir = m_target - m_position;
	vec3 zaxis = { 0.0f, 0.0f, 1.0f };
	quat r = angleAxis(yaw, zaxis);
	dir = r * dir; //Rotate(dir, r);
	m_localup = r * m_localup; //Rotate(m_localup, r);

	vec3 right = normalize(cross(dir, m_localup));
	r = angleAxis(-pitch, right);
	dir = r * dir; //Rotate(dir, r);
	m_localup = normalize(cross(right, dir));
	if(m_localup.z >= 0.0) m_up.z = 1.0;
	else m_up.z = -1.0f;

	m_position = m_target - dir;
}

void
CCamera::dolly(float dist)
{
	vec3 dir = normalize(m_target - m_position)*dist;
	m_position = m_position + dir;
	m_target = m_target + dir;
}

void
CCamera::zoom(float dist)
{
	vec3 dir = m_target - m_position;
	float curdist = length(dir);
	if(dist >= curdist)
		dist = curdist-0.3f;
	dir = normalize(dir)*dist;
	m_position = m_position + dir;
}

void
CCamera::pan(float x, float y)
{
	vec3 dir = normalize(m_target - m_position);
	vec3 right = normalize(cross(dir, m_up));
	vec3 localup = normalize(cross(right, dir));
	dir = right*x + localup*y;
	m_position = m_position + dir;
	m_target = m_target + dir;
}

void
CCamera::setDistanceFromTarget(float dist)
{
	vec3 dir = m_position - m_target;
	dir = normalize(dir)*dist;
	m_position = m_target + dir;
}

float
CCamera::distanceTo(vec3 v)
{
	return length(m_position - v);
}

float
CCamera::distanceToTarget(void)
{
	return length(m_position - m_target);
}

// calculate minimum distance to a sphere at the target
// so the whole sphere is visible
/*
float
CCamera::minDistToSphere(float r)
{
	float t = min(m_rwcam->viewWindow.x, m_rwcam->viewWindow.y);
	float a = atan(t);	// half FOV angle
	return r/sin(a);
}
*/

CCamera::CCamera()
{
	m_position = vec3(0.0f, 6.0f, 0.0f);
	m_target = vec3(0.0f, 0.0f, 0.0f);

	m_up = vec3(0.0f, 0.0f, 1.0f);
	m_localup = m_up;
	m_fov = 70.0f;
	m_aspectRatio = 1.0f;
	m_near = 0.1f;
	m_far = 100.0f;
}

/*
bool
CCamera::IsSphereVisible(rw::Sphere *sph, rw::Matrix *xform)
{
	rw::Sphere sphere = *sph;
	rw::Vector::transformPoints(&sphere.center, &sphere.center, 1, xform);
	return m_rwcam->frustumTestSphere(&sphere) != rw::Camera::SPHEREOUTSIDE;
}
*/
