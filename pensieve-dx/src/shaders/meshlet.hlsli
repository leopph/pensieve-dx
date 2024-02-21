#ifndef MESHLET_HLSLI
#define MESHLET_HLSLI

struct Meshlet {
  uint vertex_count;
  uint vertex_offset;
  uint primitive_count;
  uint primitive_offset;
};

#endif