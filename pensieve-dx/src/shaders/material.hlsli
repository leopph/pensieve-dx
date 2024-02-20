#ifndef MATERIAL_HLSLI
#define MATERIAL_HLSLI

struct Material {
  float3 base_color;
  float metallic;
  float roughness;
  float3 emission_color;
  uint base_color_map_idx;
  uint metallic_map_idx;
  uint roughness_map_idx;
  uint emission_map_idx;
};

#endif
