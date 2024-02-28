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
  out indices uint3 out_indices[256]) {
  const uint meshlet_idx = group_id / g_draw_params.inst_count;
  const uint inst_idx = group_id % g_draw_params.inst_count;

  const StructuredBuffer<Meshlet> meshlets = ResourceDescriptorHeap[g_draw_params.meshlet_buf_idx];
  const Meshlet meshlet = meshlets[meshlet_idx];
  
  SetMeshOutputCounts(meshlet.vertex_count, meshlet.primitive_count);

  if (group_thread_id < meshlet.vertex_count) {
    const StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[g_draw_params.vertex_idx_buf_idx];
    const StructuredBuffer<float4> positions = ResourceDescriptorHeap[g_draw_params.pos_buf_idx];
    const StructuredBuffer<float4> normals = ResourceDescriptorHeap[g_draw_params.norm_buf_idx];
    const StructuredBuffer<float4> tangents = ResourceDescriptorHeap[g_draw_params.tan_buf_idx];
    const StructuredBuffer<InstanceBufferData> instance_data_buffer = ResourceDescriptorHeap[g_draw_params.inst_buf_idx];
    
    const uint vertex_id = vertex_indices[meshlet.vertex_offset + group_thread_id];
    
    const float4 position_os = positions[vertex_id];
    const float3 normal_os = normalize(normals[vertex_id].xyz);
    const float3 tangent_os = normalize(tangents[vertex_id].xyz);
    const InstanceBufferData instance_data = instance_data_buffer[g_draw_params.instance_offset + inst_idx];
    
    const float4 position_ws = mul(position_os, instance_data.model_mtx);
    const float4 position_cs = mul(position_ws, g_draw_params.view_proj_mtx);
    const float3 normal_ws = normalize(mul(normal_os, (float3x3) instance_data.normal_mtx));
    float3 tangent_ws = normalize(mul(tangent_os, (float3x3) instance_data.model_mtx));
    tangent_ws = normalize(tangent_ws - dot(tangent_ws, normal_ws) * normal_ws);
    const float3 bitangent_ws = cross(normal_ws, tangent_ws);
    const float3x3 tbn_mtx_ws = float3x3(tangent_ws, bitangent_ws, normal_ws);
    
    out_verts[group_thread_id].position_ws = position_ws.xyz;
    out_verts[group_thread_id].position_cs = position_cs;
    out_verts[group_thread_id].normal_ws = normal_ws;
    out_verts[group_thread_id].tbn_mtx_ws = tbn_mtx_ws;
    
    if (g_draw_params.uv_buf_idx != INVALID_RESOURCE_IDX) {
      const StructuredBuffer<float2> uvs = ResourceDescriptorHeap[g_draw_params.uv_buf_idx];
      out_verts[group_thread_id].uv = uvs[vertex_id];
    } else {
      out_verts[group_thread_id].uv = float2(0, 0);
    }
  }

  for (uint i = 0; i < 2; i++) {
    const uint primitive_idx = group_thread_id + i * 128;

    if (primitive_idx < meshlet.primitive_count) {
      const StructuredBuffer<uint> primitive_indices = ResourceDescriptorHeap[g_draw_params.prim_idx_buf_idx];
      const uint packed_indices = primitive_indices[meshlet.primitive_offset + primitive_idx];
    
      out_indices[primitive_idx] = uint3(packed_indices & 0x3FF, (packed_indices >> 10) & 0x3FF, (packed_indices >> 20) & 0x3FF);
    }
  }
}
