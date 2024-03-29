#ifndef DRAW_PARAMS_HLSLI
#define DRAW_PARAMS_HLSLI

struct DrawParams {
  uint meshlet_count;
  uint meshlet_offset;
  uint instance_count;
  uint instance_offset;

  uint pos_buf_idx;
  uint norm_buf_idx;
  uint tan_buf_idx;
  uint uv_buf_idx;

  uint vertex_idx_buf_idx;
  uint prim_idx_buf_idx;
  uint meshlet_buf_idx;
  uint mtl_buf_idx;

  float3 camera_pos;
  uint inst_buf_idx;

  row_major float4x4 view_proj_mtx;
};

#endif
