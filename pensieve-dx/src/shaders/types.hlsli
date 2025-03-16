#ifndef TYPES_HLSLI
#define TYPES_HLSLI



struct DrawParams {
  uint meshlet_count;
  uint meshlet_offset;
  uint instance_count;
  uint instance_offset;
};



struct CameraParams {
  row_major float4x4 view_proj_mtx;
  float3 camera_pos;
  float pad;
};



struct MeshParams {
  uint pos_buf_idx;
  uint norm_buf_idx;
  uint tan_buf_idx;
  uint uv_buf_idx;

  uint vertex_idx_buf_idx;
  uint prim_idx_buf_idx;
  uint meshlet_buf_idx;
  uint inst_buf_idx;
};



struct Meshlet {
  uint vertex_count;
  uint vertex_offset;
  uint primitive_count;
  uint primitive_offset;
};



struct InstanceBufferData {
  row_major float4x4 model_mtx;
  row_major float4x4 model_inv_transp_mtx;
};



struct Material {
  float3 base_color;
  float metallic;

  float roughness;
  float3 emission_color;

  uint base_color_map_idx;
  uint metallic_map_idx;
  uint roughness_map_idx;
  uint emission_map_idx;

  uint normal_map_idx;
  float3 pad;
};

#endif
