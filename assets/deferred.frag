#version 460
#define ambient 0.f

layout (set = 0, binding = 0) uniform sampler2D samplerPosition;
layout (set = 0, binding = 1) uniform sampler2D samplerNormal;
layout (set = 0, binding = 2) uniform sampler2D samplerAlbedo;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragcolor;

void main()
{
    // Get G-Buffer values
    vec3 fragPos = texture(samplerPosition, inUV).rgb;
    vec3 normal = texture(samplerNormal, inUV).rgb;
    vec4 albedo = texture(samplerAlbedo, inUV);

    outFragcolor.rgb = albedo.rgb;
    outFragcolor.a = 1.0;
}