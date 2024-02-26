#include "common.hlsli"
#include "globals.hlsli"
#include "instance_buffer.hlsli"
#include "meshlet.hlsli"
#include "ps_in.hlsli"

[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void main(
  in uint group_id : SV_GroupID,
  in uint group_thread_id : SV_GroupThreadID,
  out vertices PsIn out_verts[128],
  out indices uint3 out_indices[128]) {
  const uint meshlet_idx = group_id / g_draw_data.inst_count;
  const uint inst_idx = group_id % g_draw_data.inst_count;

  const StructuredBuffer<Meshlet> meshlets = ResourceDescriptorHeap[g_draw_data.meshlet_buf_idx];
  const Meshlet meshlet = meshlets[meshlet_idx];
  
  SetMeshOutputCounts(meshlet.vertex_count, meshlet.primitive_count);

  if (group_thread_id < meshlet.vertex_count) {
    const StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[g_draw_data.vertex_idx_buf_idx];
    const StructuredBuffer<float4> positions = ResourceDescriptorHeap[g_draw_data.pos_buf_idx];
    const StructuredBuffer<float4> normals = ResourceDescriptorHeap[g_draw_data.norm_buf_idx];
    const StructuredBuffer<float4> tangents = ResourceDescriptorHeap[g_draw_data.tan_buf_idx];
    const StructuredBuffer<InstanceBufferData> instance_data_buffer = ResourceDescriptorHeap[g_draw_data.inst_buf_idx];
    
    const uint vertex_id = vertex_indices[meshlet.vertex_offset + group_thread_id];
    
    const float4 position_os = positions[vertex_id];
    const float3 normal_os = normalize(normals[vertex_id].xyz);
    const float3 tangent_os = normalize(tangents[vertex_id].xyz);
    const InstanceBufferData instance_data = instance_data_buffer[inst_idx];
    
    const float4 position_ws = mul(position_os, instance_data.model_mtx);
    const float4 position_cs = mul(position_ws, g_draw_data.view_proj_mtx);
    const float3 normal_ws = normalize(mul(normal_os, (float3x3) instance_data.normal_mtx));
    float3 tangent_ws = normalize(mul(tangent_os, (float3x3) instance_data.model_mtx));
    tangent_ws = normalize(tangent_ws - dot(tangent_ws, normal_ws) * normal_ws);
    const float3 bitangent_ws = cross(normal_ws, tangent_ws);
    const float3x3 tbn_mtx_ws = float3x3(tangent_ws, bitangent_ws, normal_ws);
    
    out_verts[group_thread_id].position_ws = position_ws.xyz;
    out_verts[group_thread_id].position_cs = position_cs;
    out_verts[group_thread_id].normal_ws = normal_ws;
    out_verts[group_thread_id].tbn_mtx_ws = tbn_mtx_ws;
    
    if (g_draw_data.uv_buf_idx != INVALID_RESOURCE_IDX) {
      const StructuredBuffer<float2> uvs = ResourceDescriptorHeap[g_draw_data.uv_buf_idx];
      out_verts[group_thread_id].uv = uvs[vertex_id];
    } else {
      out_verts[group_thread_id].uv = float2(0, 0);
    }
  }

  if (group_thread_id < meshlet.primitive_count) {
    const StructuredBuffer<uint> primitive_indices = ResourceDescriptorHeap[g_draw_data.prim_idx_buf_idx];
    const uint packed_indices = primitive_indices[meshlet.primitive_offset + group_thread_id];
    
    out_indices[group_thread_id] = uint3(packed_indices & 0x3FF, (packed_indices >> 10) & 0x3FF, (packed_indices >> 20) & 0x3FF);
  }
}
