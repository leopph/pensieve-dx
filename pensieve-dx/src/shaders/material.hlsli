#ifndef MATERIAL_HLSLI
#define MATERIAL_HLSLI

struct Material {
  float3 base_color;
  float metallic;
  float roughness;
  uint base_texture_idx;
  uint metallic_texture_idx;
  uint roughness_texture_idx;
};

#endif
