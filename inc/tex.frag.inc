const char *tex_frag_src =
"#version 460\n"
"\n"
"in vec4 v_color;\n"
"in vec2 v_texCoord;\n"
"out vec4 frag_color;\n"
"\n"
"uniform sampler2D tex0;\n"
"\n"
"void main()\n"
"{\n"
"	vec4 tex = texture(tex0, v_texCoord);\n"
"	frag_color = v_color*tex;\n"
"	if(frag_color.a == 0.0)\n"
"		discard;\n"
"}\n"
;