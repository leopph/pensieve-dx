#include "globals.hlsli"
#include "vs_out.hlsli"
#include "material.hlsli"
#include "common.hlsli"

static const float3 kLightDir = normalize(float3(0, 0, 1));
static const float kPi = 3.14159265;
static const float3 kCameraPos = float3(0, 0, -5);
static const float kGamma = 2.2;

float TrowbridgeReitzGgxNdf(const float n_dot_h, const float roughness) {
  const float roughness4 = pow(roughness, 4);
  return roughness4 / (kPi * pow(pow(n_dot_h, 2) * (roughness4 - 1) + 1, 2));
}

float SmithGeometry(const float n_dot_v, const float n_dot_l, const float roughness) {
  const float k = pow(roughness + 1, 2) / 8;
  return n_dot_v / (n_dot_v * (1 - k) + k) * n_dot_l / (n_dot_l * (1 - k) + k);
}

float3 SchlickFresnel(const float v_dot_h, const float3 f0) {
  return f0 + (1 - f0) * pow(2, (-5.55473 * v_dot_h - 6.98316) - v_dot_h);
}

float4 main(const VsOut vs_out) : SV_Target {
  const ConstantBuffer<Material> material = ResourceDescriptorHeap[g_draw_data.mtl_buf_idx];

  float3 base_color = material.base_color;
  float metallic = material.metallic;
  float roughness = material.roughness;
  float3 emission = material.emission_color;

  if (material.base_color_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D base_color_map = ResourceDescriptorHeap[material.base_color_map_idx];
    base_color *= pow(base_color_map.Sample(g_sampler, vs_out.uv).rgb, kGamma);
  }

  if (material.metallic_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D metallic_map = ResourceDescriptorHeap[material.metallic_map_idx];
    metallic *= metallic_map.Sample(g_sampler, vs_out.uv).r;
  }

  if (material.roughness_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D roughness_map = ResourceDescriptorHeap[material.roughness_map_idx];
    roughness *= roughness_map.Sample(g_sampler, vs_out.uv).r;
  }

  if (material.emission_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D emission_map = ResourceDescriptorHeap[material.emission_map_idx];
    emission *= emission_map.Sample(g_sampler, vs_out.uv).rgb;
  }

  float3 normal;

  if (material.normal_map_idx != INVALID_RESOURCE_IDX) {
    const Texture2D normal_map = ResourceDescriptorHeap[material.normal_map_idx];
    normal = normal_map.Sample(g_sampler, vs_out.uv).rgb * 2 - 1;
    normal = normalize(mul(normalize(normal), vs_out.tbn_mtx_ws));
  } else {
    normal = normalize(vs_out.normal_ws);
  }

  const float3 f0 = lerp(0.04, base_color, metallic);

  const float3 dir_to_light = normalize(-kLightDir);
  const float3 dir_to_cam = normalize(kCameraPos - vs_out.position_ws);
  const float3 halfway = normalize(dir_to_cam + dir_to_light);

  const float n_dot_l = saturate(dot(normal, dir_to_light));
  const float n_dot_v = saturate(dot(normal, dir_to_cam));
  const float n_dot_h = saturate(dot(normal, halfway));
  const float v_dot_h = saturate(dot(dir_to_cam, halfway));

  const float n = TrowbridgeReitzGgxNdf(n_dot_h, roughness);
  const float g = SmithGeometry(n_dot_v, n_dot_l, roughness);
  const float3 f = SchlickFresnel(v_dot_h, f0);

  const float3 diffuse = base_color / kPi;
  const float3 specular = n * g * f / (4 * n_dot_l * n_dot_v + 0.0001);

  const float3 specular_factor = f;
  const float3 diffuse_factor = (1 - specular_factor) * (1 - metallic);

  const float3 lighting = n_dot_l * (diffuse_factor * diffuse + specular);

  float3 out_color = lighting + emission;
  out_color /= out_color + 1;
  out_color = pow(out_color, 1 / kGamma);

  return float4(out_color, 1);
}
