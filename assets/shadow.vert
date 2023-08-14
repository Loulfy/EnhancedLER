#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Attributes
layout (location = 0) in vec3 inPos;

struct Instance
{
    mat4 model;
    vec3 bbmin;
    vec3 bbmax;
    uint matId;
    uint lodId;
};

layout(set = 0, binding = 0) readonly buffer inInstBuffer { Instance props[]; };

layout (push_constant) uniform constants
{
    mat4 viewProj;
} PushConstants;

void main()
{
    Instance inst = props[gl_InstanceIndex]; // gl_DrawID
    vec4 tmpPos = vec4(inPos.xyz, 1.0);

    //gl_Position = PushConstants.viewProj * vec4(inPos.xyz, 1.0);
    gl_Position = PushConstants.viewProj * inst.model * tmpPos;
}