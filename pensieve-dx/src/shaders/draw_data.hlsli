#ifndef DRAW_DATA_HLSLI
#define DRAW_DATA_HLSLI

struct DrawData {
  uint pos_buf_idx;
  uint uv_buf_idx;
  uint mtl_buf_idx;
  uint pad;
  row_major float4x4 mvp;
};

#endif
