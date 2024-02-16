#include "globals.hlsli"
#include "vs_out.hlsli"
#include "material.hlsli"

float4 main(const VsOut vs_out) : SV_Target {
  const ConstantBuffer<Material> material = ResourceDescriptorHeap[g_draw_data.mtl_buf_idx];

  if (material.base_texture_idx == INVALID_TEXTURE_IDX) {
    return float4(material.base_color, 1);
  }

  const Texture2D base_texture = ResourceDescriptorHeap[material.base_texture_idx];
  return float4(material.base_color, 1) * base_texture.Sample(g_sampler, vs_out.uv);
}
