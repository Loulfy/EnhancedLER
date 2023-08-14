#version 450

#extension GL_EXT_nonuniform_qualifier: require
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_GOOGLE_include_directive: require

#include "ler_shader.hpp"

layout(set = 0, binding = 1) readonly buffer inMatBuffer { Material mats[]; };
layout(set = 0, binding = 2) uniform sampler2D textures[];
layout(set = 0, binding = 3) uniform sampler2DArray shadowMap;

layout (set = 0, binding = 4) uniform UBO
{
    vec4 cascadeSplits;
    mat4 cascadeViewProjMat[4];
} ubo;

layout(set = 0, binding = 5) readonly buffer inInstBuffer { Instance props[]; };

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in flat uint inMatId;
layout (location = 4) in vec3 inViewPos;
layout (location = 5) in vec3 inPos;
layout (location = 6) in flat uint inInsId;

const mat4 biasMat = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 1.0, 0.0,
0.5, 0.5, 0.0, 1.0 );

// Return Output
layout (location = 0) out vec4 outFragColor;

#define ambient 0.1
#define SHADOW_MAP_CASCADE_COUNT 4

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
{
    float shadow = 1.0;
    float bias = 0.005;

    if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
        float dist = texture(shadowMap, vec3(shadowCoord.st + offset, cascadeIndex)).r;
        if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
            shadow = ambient;
        }
    }
    return shadow;

}

float filterPCF(vec4 sc, uint cascadeIndex)
{
    ivec2 texDim = textureSize(shadowMap, 0).xy;
    float scale = 0.75;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 1;

    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
            count++;
        }
    }
    return shadowFactor / count;
}

void main()
{
    Material m = mats[inMatId];
    Instance inst = props[inInsId];

    // Get cascade index for the current fragment's view position
    uint cascadeIndex = 0;
    for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT-1; ++i) {
        if(inViewPos.z < ubo.cascadeSplits[i]) {
            cascadeIndex = i + 1;
        }
    }

    // Depth compare for shadowing
    vec4 shadowCoord = (biasMat * ubo.cascadeViewProjMat[cascadeIndex]) * inst.model * vec4(inPos, 1.0);

    // Calculate normal in tangent space
    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTangent);
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    vec3 tnorm = TBN * normalize(texture(textures[nonuniformEXT(m.norId)], inUV).xyz * 2.0 - vec3(1.0));
    //outNormal = vec4(tnorm, 1.0);

    //outFragColor = vec4(0.0, 0.0, 0.0, 1.0);
    //outFragColor = texture(textures[nonuniformEXT(m.texId)], inUV);// * vec4(m.color, 1.0);

    vec3 L = normalize(vec3(0,50,0.3));
    N = normalize(tnorm);

    float NdotL = max(0.0, dot(N, L));
    vec3 diff = texture(textures[nonuniformEXT(m.texId)], inUV).xyz * m.color;// * NdotL;
    //vec4 diff = texture(textures[nonuniformEXT(m.texId)], inUV) * NdotL;

    float shadow = textureProj(shadowCoord, vec2(0.0), cascadeIndex);

    //float shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);

    outFragColor = vec4(diff, 1.0);
    //outFragColor = vec4(shadowCoord.st, 0, 1);
    //outFragColor = diff * shadow;

    //float depth = texture(shadowMap, inUV).r * 2.0 - vec3(1.0);
    //outFragColor = vec4(vec3((depth)), 1.0);

    /*switch(cascadeIndex) {
        case 0 :
        outFragColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
        break;
        case 1 :
        outFragColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
        break;
        case 2 :
        outFragColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
        break;
        case 3 :
        outFragColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
        break;
    }*/
}