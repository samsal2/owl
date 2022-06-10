#version 450

/* there is no need to use different samplers, just lazy */

layout (set = 1, binding = 0) uniform sampler sampler0;
layout (set = 1, binding = 1) uniform texture2D texture0;

layout (set = 2, binding = 0) uniform sampler bloom_sampler;
layout (set = 2, binding = 1) uniform texture2D bloom_texture;

layout (set = 3, binding = 0) uniform sampler sunrays_sampler;
layout (set = 3, binding = 1) uniform texture2D sunrays_texture;

layout (set = 4, binding = 0) uniform sampler dithering_sampler;
layout (set = 4, binding = 1) uniform texture2D dithering_texture;

layout (set = 5, binding = 0) uniform sampler sampler4;
layout (set = 5, binding = 1) uniform texture2D texture0;

layout (location = 0) in vec2 vUv;
layout (location = 1) in vec2 vL;
layout (location = 2) in vec2 vR;
layout (location = 3) in vec2 vT;
layout (location = 4) in vec2 vB;


