#ifndef GLOBALS_HLSLI
#define GLOBALS_HLSLI

#include "draw_data.hlsli"

ConstantBuffer<DrawData> g_draw_data : register(b0, space0);
SamplerState g_sampler : register(s0, space0);

#endif
