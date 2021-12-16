#type vertex
#version 430 core

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoords;
layout(location = 3) in vec4 inNormals;

uniform mat4 uModel;
uniform mat4 uViewProjection;

void main() {
	gl_Position = uViewProjection * uModel * inPos;
}

