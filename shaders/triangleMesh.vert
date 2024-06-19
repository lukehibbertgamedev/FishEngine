//we will be using glsl version 4.5 syntax
#version 450

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;

layout (location = 0) out vec3 outColor;

// camera buffer uniform block
layout(set = 0, binding = 0) uniform CameraBuffer {
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
} cameraData;

// push constants block
layout( push_constant ) uniform constants
{
	vec4 data;
	mat4 matrix;
} PushConstants;

void main()
{
	mat4 transformMatrix = (cameraData.viewProjectionMatrix * PushConstants.matrix);
	gl_Position = transformMatrix * vec4(vPosition, 1.0f);
	outColor = vColor;
}