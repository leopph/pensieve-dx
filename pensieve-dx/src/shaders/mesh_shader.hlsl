#include "common.hlsli"
#include "globals.hlsli"
#include "instance_buffer.hlsli"
#include "meshlet.hlsli"
#include "ps_in.hlsli"

PsIn CalculateVertex(const uint vertex_idx, const uint instance_idx) {
  const StructuredBuffer<float4> positions = ResourceDescriptorHeap[g_draw_params.pos_buf_idx];
  const float4 position_os = positions[vertex_idx];

  const StructuredBuffer<float4> normals = ResourceDescriptorHeap[g_draw_params.norm_buf_idx];
  const float3 normal_os = normalize(normals[vertex_idx].xyz);

  const StructuredBuffer<InstanceBufferData> instance_data_buffer = ResourceDescriptorHeap[g_draw_params.inst_buf_idx];
  const InstanceBufferData instance_data = instance_data_buffer[g_draw_params.instance_offset + instance_idx];
    
  const float4 position_ws = mul(position_os, instance_data.model_mtx);
  const float4 position_cs = mul(position_ws, g_draw_params.view_proj_mtx);
  const float3 normal_ws = normalize(mul(normal_os, (float3x3) instance_data.model_inv_transp_mtx));

  PsIn ps_in;
  ps_in.position_ws = position_ws.xyz;
  ps_in.position_cs = position_cs;
  ps_in.normal_ws = normal_ws;

  if (g_draw_params.tan_buf_idx != INVALID_RESOURCE_IDX) {
    const StructuredBuffer<float4> tangents = ResourceDescriptorHeap[g_draw_params.tan_buf_idx];
    const float3 tangent_os = normalize(tangents[vertex_idx].xyz);

    float3 tangent_ws = normalize(mul(tangent_os, (float3x3) instance_data.model_mtx));
    tangent_ws = normalize(tangent_ws - dot(tangent_ws, normal_ws) * normal_ws);
    const float3 bitangent_ws = cross(normal_ws, tangent_ws);
    ps_in.tbn_mtx_ws = float3x3(tangent_ws, bitangent_ws, normal_ws);
  } else {
    ps_in.tbn_mtx_ws = 0;
  }

  if (g_draw_params.uv_buf_idx != INVALID_RESOURCE_IDX) {
    const StructuredBuffer<float2> uvs = ResourceDescriptorHeap[g_draw_params.uv_buf_idx];
    ps_in.uv = uvs[vertex_idx];
  } else {
    ps_in.uv = float2(0, 0);
  }

  return ps_in;
}

uint3 UnpackIndices(const uint packed_indices) {
  return uint3(packed_indices & 0x3FF, (packed_indices >> 10) & 0x3FF, (packed_indices >> 20) & 0x3FF);
}

[outputtopology("triangle")]
[numthreads(MESHLET_MAX_VERTS, 1, 1)]
void main(
  const uint gid : SV_GroupID,
  const uint gtid : SV_GroupThreadID,
  out vertices PsIn out_verts[MESHLET_MAX_VERTS],
  out indices uint3 out_tris[MESHLET_MAX_PRIMS]) {

  const uint meshlet_idx = gid / g_draw_params.instance_count;
  const StructuredBuffer<Meshlet> meshlets = ResourceDescriptorHeap[g_draw_params.meshlet_buf_idx];
  const Meshlet meshlet = meshlets[meshlet_idx + g_draw_params.meshlet_offset];

  uint start_instance = gid % g_draw_params.instance_count;
  uint instance_count = 1;

  if (meshlet_idx == g_draw_params.meshlet_count - 1) {
    const uint instances_per_group = min(MESHLET_MAX_VERTS / meshlet.vertex_count, MESHLET_MAX_PRIMS / meshlet.primitive_count);

    const uint unpacked_group_count = (g_draw_params.meshlet_count - 1) * g_draw_params.instance_count;
    const uint packed_index = gid - unpacked_group_count;

    start_instance = packed_index * instances_per_group;
    instance_count = min(g_draw_params.instance_count - start_instance, instances_per_group);
  }

  const uint vert_count = meshlet.vertex_count * instance_count;
  const uint prim_count = meshlet.primitive_count * instance_count;
  
  SetMeshOutputCounts(vert_count, prim_count);

  if (gtid < vert_count) {
    const uint read_index = gtid % meshlet.vertex_count;
    const uint instance_id = gtid / meshlet.vertex_count;

    const StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[g_draw_params.vertex_idx_buf_idx];
    const uint vertex_index = vertex_indices[meshlet.vertex_offset + read_index];
    const uint instance_index = start_instance + instance_id;

    out_verts[gtid] = CalculateVertex(vertex_index, instance_index);
  }

  for (uint i = 0; i < 2; i++) {
    const uint primitive_id = gtid + i * 128;

    if (primitive_id < prim_count) {
      const uint read_index = primitive_id % meshlet.primitive_count;
      const uint instance_id = primitive_id / meshlet.primitive_count;

      const StructuredBuffer<uint> primitive_indices = ResourceDescriptorHeap[g_draw_params.prim_idx_buf_idx];
  
      out_tris[primitive_id] = UnpackIndices(primitive_indices[meshlet.primitive_offset + read_index]) + (meshlet.vertex_count * instance_id);
    }
  }
}
