#version 460

#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_shader_8bit_storage: require

#include "ler_shader.hpp"

// Attributes
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inTangent;

layout(set = 0, binding = 0) readonly buffer inInstBuffer { Instance props[]; };
layout(set = 0, binding = 6) readonly buffer inDrawBuffer { DrawCommand draws[]; };

layout (push_constant) uniform constants
{
    mat4 proj;
    mat4 view;
} PushConstants;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outTangent;
layout (location = 3) out uint outMatId;
layout (location = 4) out vec3 outViewPos;
layout (location = 5) out vec3 outPos;
layout (location = 6) out uint outInsId;

void main()
{
    //gl_Position = PushConstants.proj * PushConstants.view * vec4(inPos.xyz, 1.0);

    uint drawIndex = draws[gl_DrawID].drawId;
    Instance inst = props[drawIndex]; // gl_DrawID or gl_InstanceIndex
    vec4 tmpPos = vec4(inPos.xyz, 1.0);

    gl_Position = PushConstants.proj * PushConstants.view * inst.model * tmpPos;
    //gl_Position = PushConstants.proj * inst.model * tmpPos;

    // Normal in world space
    mat3 mNormal = transpose(inverse(mat3(inst.model)));
    outNormal = mNormal * normalize(inNormal);
    outTangent = mNormal * normalize(inTangent);

    outUV = inUV.xy;

    // Material
    outMatId = inst.matId;

    //outShadowCoord = ( biasMat * ubo.light * inst.model ) * vec4(inPos, 1.0);

    outViewPos = (PushConstants.view * inst.model * tmpPos).xyz;
    //outPos = (inst.model * tmpPos).xyz;
    outPos = inPos;
    outInsId = drawIndex;
}