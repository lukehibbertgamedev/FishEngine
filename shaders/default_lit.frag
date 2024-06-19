//glsl version 4.5
#version 450

// shader input (from vertex shader, the location is fine as they are different in/out variables)
layout (location = 0) in vec3 inColour;

//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform SceneData {
	vec4 fogColour;
	vec4 fogDistances;
	vec4 ambientColour;
	vec4 sunlightDirection;
	vec4 sunlightColour;
} sceneData;

void main()
{
	//return colour
	outFragColor = vec4(inColour + sceneData.ambientColour.xyz, 1.0f);
}