#ifndef DRAW_DATA_HLSLI
#define DRAW_DATA_HLSLI

struct DrawData {
  uint pos_buf_idx;
  uint norm_buf_idx;
  uint uv_buf_idx;
  uint mtl_buf_idx;
  row_major float4x4 model_mtx;
  row_major float4x4 view_proj_mtx;
};

#endif
