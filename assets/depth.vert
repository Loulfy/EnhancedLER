#version 460

#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_GOOGLE_include_directive: require

#include "ler_shader.hpp"

// Attributes
layout (location = 0) in vec3 inPos;

layout(set = 0, binding = 0) readonly buffer inInstBuffer { Instance props[]; };
layout(set = 0, binding = 1) readonly buffer inDrawBuffer { DrawCommand draws[]; };

layout (push_constant) uniform constants
{
    mat4 proj;
    mat4 view;
} PushConstants;

void main()
{
    uint drawIndex = draws[gl_DrawID].drawId;
    Instance inst = props[drawIndex];
    vec4 tmpPos = vec4(inPos.xyz, 1.0);

    gl_Position = PushConstants.proj * PushConstants.view * inst.model * tmpPos;
}