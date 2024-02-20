#include "globals.hlsli"
#include "vs_out.hlsli"
#include "material.hlsli"
#include "common.hlsli"

static const float3 kLightDir = normalize(float3(1, -0.5, 1));
static const float kPi = 3.14159265;
static const float3 kCameraPos = float3(0, 0, -5);

float Distribution(const float3 normal, const float3 halfway, const float roughness) {
  const float alpha2 = pow(pow(roughness, 2), 2);
  return alpha2 / (kPi * pow(pow(dot(normal, halfway), 2) * (alpha2 - 1) + 1, 2));
}

float Geometry(const float roughness, const float3 light_dir, const float3 view_dir, const float3 normal, const float3 halfway) {
  const float k = pow(roughness + 1, 2) / 8;
  const float n_dot_v = dot(normal, view_dir);
  const float n_dot_l = dot(normal, light_dir);
  return n_dot_v / (n_dot_v * (1 - k) + k) * n_dot_l / (n_dot_l * (1 - k) + k);
}

float3 Fresnel(const float3 f0, const float3 view_dir, const float3 halfway) {
  const float v_dot_h = dot(view_dir, halfway);
  return f0 + (1 - f0) * pow(2, (-5.55473 * v_dot_h - 6.98316) - v_dot_h);
}

float4 main(const VsOut vs_out) : SV_Target {
  const ConstantBuffer<Material> material = ResourceDescriptorHeap[g_draw_data.mtl_buf_idx];

  float3 base_color = material.base_color;
  float metallic = material.metallic;
  float roughness = material.roughness;

  if (material.base_color_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D base_color_map = ResourceDescriptorHeap[material.base_color_map_idx];
    base_color *= base_color_map.Sample(g_sampler, vs_out.uv).rgb;
  }

  if (material.metallic_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D metallic_map = ResourceDescriptorHeap[material.metallic_map_idx];
    metallic *= metallic_map.Sample(g_sampler, vs_out.uv).r;
  }

  if (material.roughness_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D roughness_map = ResourceDescriptorHeap[material.roughness_map_idx];
    roughness *= roughness_map.Sample(g_sampler, vs_out.uv).r;
  }

  const float3 normal = normalize(vs_out.normal);
  const float3 view_dir = normalize(kCameraPos);
  const float3 halfway = normalize(view_dir + kLightDir);
  const float3 f0 = lerp((float3) 0.04, base_color, metallic);

  float ndf = Distribution(normal, halfway, roughness);
  float geom = Geometry(roughness, kLightDir, view_dir, normal, halfway);
  float fresnel = Fresnel(f0, view_dir, halfway);

  return float4(base_color, 1);
}
