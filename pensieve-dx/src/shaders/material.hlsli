#ifndef MATERIAL_HLSLI
#define MATERIAL_HLSLI

struct Material {
  float3 base_color;
  uint base_texture_idx;
  uint base_texture_sampler_idx;
  float3 pad;
};

#endif
