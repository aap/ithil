#version 460

in vec4 v_color;
in vec2 v_texCoord;
out vec4 frag_color;

uniform sampler2D tex0;

void main()
{
	vec4 tex = texture(tex0, v_texCoord);
	frag_color = v_color*tex;
	if(frag_color.a == 0.0)
		discard;
}
