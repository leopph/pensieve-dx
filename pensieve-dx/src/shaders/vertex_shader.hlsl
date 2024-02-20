#include "globals.hlsli"
#include "vs_out.hlsli"
#include "common.hlsli"

VsOut main(const uint vertex_id : SV_VertexID) {
  VsOut vs_out;

  const StructuredBuffer<float4> positions = ResourceDescriptorHeap[g_draw_data.pos_buf_idx];
  vs_out.position_ws = mul(positions[vertex_id], g_draw_data.model_mtx).xyz;
  vs_out.position_cs = mul(float4(vs_out.position_ws, 1), g_draw_data.view_proj_mtx);

  const StructuredBuffer<float4> normals = ResourceDescriptorHeap[g_draw_data.norm_buf_idx];
  vs_out.normal_ws = normalize(mul(normalize(normals[vertex_id].xyz), (float3x3)g_draw_data.normal_mtx));

  const StructuredBuffer<float4> tangents = ResourceDescriptorHeap[g_draw_data.tan_buf_idx];
  float3 tangent_ws = normalize(mul(normalize(tangents[vertex_id].xyz), (float3x3) g_draw_data.model_mtx));
  tangent_ws = normalize(tangent_ws - dot(tangent_ws, vs_out.normal_ws) * vs_out.normal_ws);
  const float3 bitangent_ws = cross(vs_out.normal_ws, tangent_ws);
  vs_out.tbn_mtx_ws = float3x3(tangent_ws, bitangent_ws, vs_out.normal_ws);

  if (g_draw_data.uv_buf_idx != INVALID_RESOURCE_IDX) {
    const StructuredBuffer<float2> uvs = ResourceDescriptorHeap[g_draw_data.uv_buf_idx];
    vs_out.uv = uvs[vertex_id];
  } else {
    vs_out.uv = float2(0, 0);
  }

  return vs_out;
}
