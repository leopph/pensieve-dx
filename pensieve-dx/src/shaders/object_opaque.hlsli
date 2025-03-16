#ifndef OBJECT_OPAQUE_HLSLI
#define OBJECT_OPAQUE_HLSLI

#include "common.hlsli"
#include "types.hlsli"



struct PsIn {
  float4 position_cs : SV_Position;
  float3 position_ws : POSITION;
  float3 normal_ws : NORMAL;
  float2 uv : TEXCOORD;
  float3x3 tbn_mtx_ws : TBN;
};



ConstantBuffer<DrawParams> g_draw_params : register(b0, space0);
ConstantBuffer<CameraParams> g_cam_params : register(b1, space0);
ConstantBuffer<MeshParams> g_mesh_params : register(b2, space0);
ConstantBuffer<Material> g_material : register(b3, space0);
SamplerState g_sampler : register(s0, space0);



PsIn CalculateVertex(uint const vertex_idx, uint const instance_idx) {
  StructuredBuffer<float4> const positions = ResourceDescriptorHeap[g_mesh_params.pos_buf_idx];
  float4 const position_os = positions[vertex_idx];

  StructuredBuffer<float4> const normals = ResourceDescriptorHeap[g_mesh_params.norm_buf_idx];
  float3 const normal_os = normalize(normals[vertex_idx].xyz);

  StructuredBuffer<InstanceBufferData> const instance_data_buffer = ResourceDescriptorHeap[g_mesh_params.inst_buf_idx];
  InstanceBufferData const instance_data = instance_data_buffer[g_draw_params.instance_offset + instance_idx];

  float4 const position_ws = mul(position_os, instance_data.model_mtx);
  float4 const position_cs = mul(position_ws, g_cam_params.view_proj_mtx);
  float3 const normal_ws = normalize(mul(normal_os, (float3x3)instance_data.model_inv_transp_mtx));

  PsIn ps_in;
  ps_in.position_ws = position_ws.xyz;
  ps_in.position_cs = position_cs;
  ps_in.normal_ws = normal_ws;

  if (g_mesh_params.tan_buf_idx != INVALID_RESOURCE_IDX) {
    StructuredBuffer<float4> const tangents = ResourceDescriptorHeap[g_mesh_params.tan_buf_idx];
    float3 const tangent_os = normalize(tangents[vertex_idx].xyz);

    float3 tangent_ws = normalize(mul(tangent_os, (float3x3)instance_data.model_mtx));
    tangent_ws = normalize(tangent_ws - dot(tangent_ws, normal_ws) * normal_ws);
    float3 const bitangent_ws = cross(normal_ws, tangent_ws);
    ps_in.tbn_mtx_ws = float3x3(tangent_ws, bitangent_ws, normal_ws);
  } else {
    ps_in.tbn_mtx_ws = 0;
  }

  if (g_mesh_params.uv_buf_idx != INVALID_RESOURCE_IDX) {
    StructuredBuffer<float2> const uvs = ResourceDescriptorHeap[g_mesh_params.uv_buf_idx];
    ps_in.uv = uvs[vertex_idx];
  } else {
    ps_in.uv = float2(0, 0);
  }

  return ps_in;
}



uint3 UnpackIndices(uint const packed_indices) {
  return uint3(packed_indices & 0x3FF, (packed_indices >> 10) & 0x3FF, (packed_indices >> 20) & 0x3FF);
}



[outputtopology("triangle")][numthreads(MESHLET_MAX_VERTS, 1, 1)]
void ms_main(uint const gid : SV_GroupID, uint const gtid : SV_GroupThreadID,
          out vertices PsIn out_verts[MESHLET_MAX_VERTS],
          out indices uint3 out_tris[MESHLET_MAX_PRIMS]) {
  uint const meshlet_idx = gid / g_draw_params.instance_count;
  StructuredBuffer<Meshlet> const meshlets = ResourceDescriptorHeap[g_mesh_params.meshlet_buf_idx];
  Meshlet const meshlet = meshlets[meshlet_idx + g_draw_params.meshlet_offset];

  uint start_instance = gid % g_draw_params.instance_count;
  uint instance_count = 1;

  if (meshlet_idx == g_draw_params.meshlet_count - 1) {
    uint const instances_per_group = min(MESHLET_MAX_VERTS / meshlet.vertex_count,
                                         MESHLET_MAX_PRIMS / meshlet.primitive_count);

    uint const unpacked_group_count = (g_draw_params.meshlet_count - 1) * g_draw_params.instance_count;
    uint const packed_index = gid - unpacked_group_count;

    start_instance = packed_index * instances_per_group;
    instance_count = min(g_draw_params.instance_count - start_instance, instances_per_group);
  }

  uint const vert_count = meshlet.vertex_count * instance_count;
  uint const prim_count = meshlet.primitive_count * instance_count;

  SetMeshOutputCounts(vert_count, prim_count);

  if (gtid < vert_count) {
    uint const read_index = gtid % meshlet.vertex_count;
    uint const instance_id = gtid / meshlet.vertex_count;

    StructuredBuffer<uint> const vertex_indices = ResourceDescriptorHeap[g_mesh_params.vertex_idx_buf_idx];
    uint const vertex_index = vertex_indices[meshlet.vertex_offset + read_index];
    uint const instance_index = start_instance + instance_id;

    out_verts[gtid] = CalculateVertex(vertex_index, instance_index);
  }

  for (uint i = 0; i < 2; i++) {
    uint const primitive_id = gtid + i * 128;

    if (primitive_id < prim_count) {
      uint const read_index = primitive_id % meshlet.primitive_count;
      uint const instance_id = primitive_id / meshlet.primitive_count;

      StructuredBuffer<uint> const primitive_indices = ResourceDescriptorHeap[g_mesh_params.prim_idx_buf_idx];

      out_tris[primitive_id] = UnpackIndices(primitive_indices[meshlet.primitive_offset + read_index]) + (meshlet.
        vertex_count * instance_id);
    }
  }
}



static uint const kLightCount = 6;
static float3 const kLightDirs[kLightCount] = {
  float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1), float3(-1, 0, 0), float3(0, -1, 0), float3(0, 0, -1)
};


static float const kPi = 3.14159265;
static float const kGamma = 2.2;



float TrowbridgeReitzGgxNdf(float const n_dot_h, float const roughness) {
  float const roughness4 = pow(roughness, 4);
  return roughness4 / (kPi * pow(pow(n_dot_h, 2) * (roughness4 - 1) + 1, 2));
}



float SmithGeometry(float const n_dot_v, float const n_dot_l, float const roughness) {
  float const k = pow(roughness + 1, 2) / 8;
  return n_dot_v / (n_dot_v * (1 - k) + k) * n_dot_l / (n_dot_l * (1 - k) + k);
}



float3 SchlickFresnel(float const v_dot_h, float3 const f0) {
  return f0 + (1 - f0) * pow(2, (-5.55473 * v_dot_h - 6.98316) - v_dot_h);
}



float4 ps_main(PsIn const ps_in) : SV_Target {
  float3 base_color = g_material.base_color;
  float metallic = g_material.metallic;
  float roughness = g_material.roughness;
  float3 emission = g_material.emission_color;
  float3 normal = normalize(ps_in.normal_ws);

  if (g_mesh_params.uv_buf_idx != INVALID_RESOURCE_IDX) {
    if (g_material.base_color_map_idx != INVALID_RESOURCE_IDX) {
      Texture2D const base_color_map = ResourceDescriptorHeap[g_material.base_color_map_idx];
      base_color *= pow(base_color_map.Sample(g_sampler, ps_in.uv).rgb, kGamma);
    }

    if (g_material.metallic_map_idx != INVALID_RESOURCE_IDX) {
      Texture2D const metallic_map = ResourceDescriptorHeap[g_material.metallic_map_idx];
      metallic *= metallic_map.Sample(g_sampler, ps_in.uv).r;
    }

    if (g_material.roughness_map_idx != INVALID_RESOURCE_IDX) {
      Texture2D const roughness_map = ResourceDescriptorHeap[g_material.roughness_map_idx];
      roughness *= roughness_map.Sample(g_sampler, ps_in.uv).r;
    }

    if (g_material.emission_map_idx != INVALID_RESOURCE_IDX) {
      Texture2D const emission_map = ResourceDescriptorHeap[g_material.emission_map_idx];
      emission *= emission_map.Sample(g_sampler, ps_in.uv).rgb;
    }

    if (g_material.normal_map_idx != INVALID_RESOURCE_IDX && g_mesh_params.tan_buf_idx != INVALID_RESOURCE_IDX) {
      Texture2D const normal_map = ResourceDescriptorHeap[g_material.normal_map_idx];
      normal = normal_map.Sample(g_sampler, ps_in.uv).rgb * 2 - 1;
      normal = normalize(mul(normalize(normal), ps_in.tbn_mtx_ws));
    }
  }

  float3 const f0 = lerp(0.04, base_color, metallic);

  float3 const dir_to_cam = normalize(g_cam_params.camera_pos - ps_in.position_ws);
  float const n_dot_v = saturate(dot(normal, dir_to_cam));

  float3 const diffuse = base_color / kPi;

  float3 direct_lighting = 0;

  for (uint i = 0; i < kLightCount; i++) {
    float3 const dir_to_light = normalize(-kLightDirs[i]);
    float3 const halfway = normalize(dir_to_cam + dir_to_light);

    float const n_dot_l = saturate(dot(normal, dir_to_light));
    float const n_dot_h = saturate(dot(normal, halfway));
    float const v_dot_h = saturate(dot(dir_to_cam, halfway));

    float const n = TrowbridgeReitzGgxNdf(n_dot_h, roughness);
    float const g = SmithGeometry(n_dot_v, n_dot_l, roughness);
    float3 const f = SchlickFresnel(v_dot_h, f0);

    float3 const specular = n * g * f / (4 * n_dot_l * n_dot_v + 0.0001);

    float3 const specular_factor = f;
    float3 const diffuse_factor = (1 - specular_factor) * (1 - metallic);

    direct_lighting += n_dot_l * (diffuse_factor * diffuse + specular);
  }

  float3 const ambient_lighting = 0.03 * base_color;

  float3 out_color = ambient_lighting + direct_lighting + emission;
  out_color /= out_color + 1;
  out_color = pow(out_color, 1 / kGamma);

  return float4(out_color, 1);
}

#endif
