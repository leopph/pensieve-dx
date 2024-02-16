#ifndef MATERIAL_HLSLI
#define MATERIAL_HLSLI

#define INVALID_TEXTURE_IDX -1

struct Material {
  float3 base_color;
  uint base_texture_idx;
};

#endif
