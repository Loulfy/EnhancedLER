#version 450

#extension GL_EXT_nonuniform_qualifier: require
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_shader_8bit_storage: require

#include "ler_shader.hpp"

layout(set = 0, binding = 1) readonly buffer inMatBuffer { Material mats[]; };
layout(set = 0, binding = 2) uniform sampler2D textures[];

layout(set = 0, binding = 5) readonly buffer inInstBuffer { Instance props[]; };

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in flat uint inMatId;
layout (location = 4) in vec3 inPos;
layout (location = 5) in flat uint inInsId;

// Return Output
layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

void main()
{
    Material m = mats[inMatId];
    Instance inst = props[inInsId];

    outPosition = vec4(inPos, 1.0);

    // Calculate normal in tangent space
    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTangent);
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    vec3 tnorm = TBN * normalize(texture(textures[nonuniformEXT(m.norId)], inUV).xyz * 2.0 - vec3(1.0));
    outNormal = vec4(tnorm, 1.0);

    outAlbedo = texture(textures[nonuniformEXT(m.texId)], inUV) * vec4(m.color, 1.0);
}