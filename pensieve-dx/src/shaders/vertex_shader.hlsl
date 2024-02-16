#include "globals.hlsli"
#include "vs_out.hlsli"
#include "common.hlsli"

VsOut main(const uint vertex_id : SV_VertexID) {
  VsOut ret;

  const StructuredBuffer<float4> positions = ResourceDescriptorHeap[g_draw_data.pos_buf_idx];
  ret.position_cs = mul(positions[vertex_id], g_draw_data.mvp);

  if (g_draw_data.uv_buf_idx != INVALID_RESOURCE_IDX) {
    const StructuredBuffer<float2> uvs = ResourceDescriptorHeap[g_draw_data.uv_buf_idx];
    ret.uv = uvs[vertex_id];
  } else {
    ret.uv = float2(0, 0);
  }

  return ret;
}
