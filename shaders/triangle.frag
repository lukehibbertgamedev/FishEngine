//glsl version 4.5
#version 450

// shader input (from vertex shader, the location is fine as they are different in/out variables)
layout (location = 0) in vec3 inColour;

//output write
layout (location = 0) out vec4 outFragColor;

void main()
{
	//return colour
	outFragColor = vec4(inColour, 1.0f);
}