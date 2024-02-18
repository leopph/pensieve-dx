#include "globals.hlsli"
#include "vs_out.hlsli"
#include "material.hlsli"
#include "common.hlsli"

float4 main(const VsOut vs_out) : SV_Target {
  const ConstantBuffer<Material> material = ResourceDescriptorHeap[g_draw_data.mtl_buf_idx];

  float3 base_color = material.base_color;
  float metallic = material.metallic;
  float roughness = material.roughness;

  if (material.base_color_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D base_color_map = ResourceDescriptorHeap[material.base_color_map_idx];
    base_color *= base_color_map.Sample(g_sampler, vs_out.uv).rgb;
  }

  if (material.metallic_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D metallic_map = ResourceDescriptorHeap[material.metallic_map_idx];
    metallic *= metallic_map.Sample(g_sampler, vs_out.uv).r;
  }

  if (material.roughness_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D roughness_map = ResourceDescriptorHeap[material.roughness_map_idx];
    roughness *= roughness_map.Sample(g_sampler, vs_out.uv).r;
  }

  const Texture2D base_texture = ResourceDescriptorHeap[material.base_texture_idx];
  return float4(material.base_color, 1) * base_texture.Sample(g_sampler, vs_out.uv);
  return float4(base_color, 1);
}
