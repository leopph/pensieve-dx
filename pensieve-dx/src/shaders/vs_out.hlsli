#ifndef VS_OUT_HLSLI
#define VS_OUT_HLSLI

struct VsOut {
  float4 position_cs : SV_Position;
  float3 position_ws : POSITION;
  float3 normal : NORMAL;
  float2 uv : TEXCOORD;
};

#endif
