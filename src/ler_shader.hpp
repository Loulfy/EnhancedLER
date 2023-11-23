//
// Created by loulfy on 06/08/2023.
//

#ifndef LER_SHADER_HPP
#define LER_SHADER_HPP

#ifdef LER_SHADER_COMMON
#include <glm/glm.hpp>
using namespace glm;
#define ALIGNAS(x) alignas(x)
#define VARINIT(x) = x
#else
#define ALIGNAS(x)
#define VARINIT(x)
#endif

struct Material
{
    ALIGNAS(4) uint texId VARINIT(0);
    ALIGNAS(4) uint norId VARINIT(0);
    ALIGNAS(16) vec3 color VARINIT(glm::vec3(1.f));
};

struct MeshDraw

{
    ALIGNAS(4) uint meshletOffset;
    ALIGNAS(4) uint meshletCount;
};

struct Meshly
{
    ALIGNAS(16) vec3 center;
    ALIGNAS(4) float radius;
    ALIGNAS(4) int8_t coneAxis[3];
    ALIGNAS(4) int8_t coneCutoff;

    ALIGNAS(4) uint vertexOffset;
    ALIGNAS(4) uint triangleOffset;
    ALIGNAS(4) uint8_t vertexCount;
    ALIGNAS(4) uint8_t triangleCount;
};

struct Meshlet
{
    ALIGNAS(4) uint countIndex VARINIT(0);
    ALIGNAS(4) uint firstIndex VARINIT(0);
    ALIGNAS(4) uint vertexOffset VARINIT(0);
};

struct Instance
{
    ALIGNAS(16) mat4 model VARINIT(glm::mat4(1.f));
    ALIGNAS(16) vec4 bounds VARINIT(glm::vec4(0.f));
    ALIGNAS(16) vec3 bbmin VARINIT(glm::vec3(0.f));
    ALIGNAS(16) vec3 bbmax VARINIT(glm::vec3(0.f));
    ALIGNAS(4) uint matId VARINIT(0);
    ALIGNAS(4) uint lodId VARINIT(0);
};

struct Frustum
{
    ALIGNAS(16) vec4 planes[6];
    ALIGNAS(16) vec4 corners[8];
    ALIGNAS(16) vec4 camera VARINIT(vec4(0.f));
    ALIGNAS(4) uint num VARINIT(0);
    ALIGNAS(4) uint cull VARINIT(0);
};

struct DrawCommand
{
    ALIGNAS(4) uint countIndex VARINIT(0);
    ALIGNAS(4) uint instanceCount VARINIT(0);
    ALIGNAS(4) uint firstIndex VARINIT(0);
    ALIGNAS(4) uint baseVertex VARINIT(0);
    ALIGNAS(4) uint baseInstance VARINIT(0);
    ALIGNAS(4) uint drawId VARINIT(0);
};

struct CullData
{
    ALIGNAS(16) mat4 proj;
    ALIGNAS(16) mat4 view;
    ALIGNAS(4) bool depthPass;
};

#endif //LER_SHADER_HPP
