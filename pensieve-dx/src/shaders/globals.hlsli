#ifndef GLOBALS_HLSLI
#define GLOBALS_HLSLI

#include "draw_params.hlsli"

ConstantBuffer<DrawParams> g_draw_params : register(b0, space0);
SamplerState g_sampler : register(s0, space0);

#endif
