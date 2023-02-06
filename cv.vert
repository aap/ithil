#version 460

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_texCoord;
layout(location = 3) in vec4 in_cvPos;
layout(location = 4) in vec2 in_texOffset;

out vec4 v_color;
out vec2 v_texCoord;

uniform mat4 u_world;
uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec2 u_windowSize;

uniform vec4 u_matEmissive;	// used as inactive
uniform vec4 u_matAmbient;	// used as active

void main()
{
	vec3 Vw = vec3(u_world * vec4(in_cvPos.xyz, 1.0));
	vec3 Vv = vec3(u_view * vec4(Vw, 1.0));
	gl_Position = u_proj * vec4(Vv, 1.0);
//	gl_Position.xy += in_pos.xy*7/u_windowSize*gl_Position.w;

	gl_Position.xy *= u_windowSize/2/gl_Position.w;
	gl_Position.xy = floor(gl_Position.xy) + 0.5;
	gl_Position.xy += in_pos.xy*3.5;
	gl_Position.xy /= u_windowSize/2/gl_Position.w;

/*
	vec2 screen = floor((gl_Position.xy/gl_Position.w + 1)/2 * u_windowSize);
	screen += in_pos.xy*3.5;
	gl_Position.xy = (screen/u_windowSize *2 -1)*gl_Position.w;
*/

	v_color = mix(u_matEmissive, u_matAmbient, in_cvPos.w);
	v_texCoord = in_texCoord + in_texOffset;
}
