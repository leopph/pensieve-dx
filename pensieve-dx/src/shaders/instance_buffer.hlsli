#ifndef INSTANCE_BUFFER_HLSLI
#define INSTANCE_BUFFER_HLSLI

struct InstanceBufferData {
  row_major float4x4 model_mtx;
  row_major float4x4 model_inv_transp_mtx;
};

#endif