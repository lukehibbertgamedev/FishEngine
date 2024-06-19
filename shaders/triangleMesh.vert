
#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 texCoord;

layout(set = 0, binding = 0) uniform  CameraBuffer{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
} cameraData;

struct ObjectData{
	mat4 modelMatrix;
};

//all object matrices. std140 matches the usage of arrays in cpp.
layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer{

	ObjectData objects[];
} objectBuffer;

//push constants block
layout( push_constant ) uniform constants
{
 	vec4 data;
 	mat4 matrix;
} PushConstants;

void main()
{
	mat4 instModelMatrix = objectBuffer.objects[gl_BaseInstance].modelMatrix;
	mat4 transformMatrix = (cameraData.viewProjectionMatrix * instModelMatrix);
	gl_Position = transformMatrix * vec4(vPosition, 1.0f);
	outColor = vColor;
	texCoord = vTexCoord;
}