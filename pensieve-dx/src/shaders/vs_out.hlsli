#ifndef VS_OUT_HLSLI
#define VS_OUT_HLSLI

struct VsOut {
  float4 position_cs : SV_Position;
  float2 uv : TEXCOORD;
};

#endif
