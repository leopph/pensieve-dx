#ifndef DRAW_DATA_HLSLI
#define DRAW_DATA_HLSLI

struct DrawData {
  uint pos_buf_idx;
  uint uv_buf_idx;
  uint mtl_buf_idx;
};

ConstantBuffer<DrawData> g_draw_data : register(b0, space0);

#endif
