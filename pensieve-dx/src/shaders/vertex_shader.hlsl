#include "globals.hlsli"
#include "vs_out.hlsli"

VsOut main(const uint vertex_id : SV_VertexID) {
  const StructuredBuffer<float4> positions = ResourceDescriptorHeap[g_draw_data.pos_buf_idx];
  const StructuredBuffer<float2> uvs = ResourceDescriptorHeap[g_draw_data.uv_buf_idx];

  const float4 position_os = positions[vertex_id];
  const float2 uv = uvs[vertex_id];

  VsOut ret;
  ret.position_cs = mul(position_os, g_draw_data.mvp);
  ret.uv = uv;

  return ret;
}
