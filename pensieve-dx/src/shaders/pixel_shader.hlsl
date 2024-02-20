#include "globals.hlsli"
#include "vs_out.hlsli"
#include "material.hlsli"
#include "common.hlsli"

static const float3 kLightDir = normalize(float3(0, 0, 1));
static const float kPi = 3.14159265;
static const float3 kCameraPos = float3(0, 0, -5);
static const float kGamma = 2.2;

float TrowbridgeReitzGgxNdf(const float3 normal, const float3 halfway, const float roughness) {
  const float roughness2 = pow(pow(roughness, 2), 2);
  return roughness2 / (kPi * pow(pow(dot(normal, halfway), 2) * (roughness2 - 1) + 1, 2));
}

float SmithGeometry(const float3 normal, const float3 view_dir, const float3 light_dir, const float roughness) {
  const float k = pow(roughness + 1, 2) / 8;
  const float n_dot_v = dot(normal, view_dir);
  const float n_dot_l = dot(normal, light_dir);
  return n_dot_v / (n_dot_v * (1 - k) + k) * n_dot_l / (n_dot_l * (1 - k) + k);
}

float3 SchlickFresnel(const float3 halfway, const float3 view_dir, const float3 f0) {
  const float v_dot_h = dot(view_dir, halfway);
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

  const float3 normal = normalize(vs_out.normal);
  const float3 dir_to_light = normalize(-kLightDir);
  const float3 dir_to_cam = normalize(kCameraPos - vs_out.position_ws);
  const float3 halfway = normalize(dir_to_cam + dir_to_light);
  const float3 f0 = lerp((float3) 0.04, base_color, metallic);

  const float n = TrowbridgeReitzGgxNdf(normal, halfway, roughness);
  const float g = SmithGeometry(normal, dir_to_cam, dir_to_light, roughness);
  const float3 f = SchlickFresnel(halfway, dir_to_cam, f0);

  const float n_dot_l = dot(normal, dir_to_light);
  const float n_dot_v = dot(normal, dir_to_cam);

  const float3 diffuse = base_color / kPi;
  const float3 specular = n * g * f / (4 * n_dot_l * n_dot_v);

  const float3 specular_factor = f;
  const float3 diffuse_factor = (1 - specular_factor) * (1 - metallic);

  float3 out_color = emission + diffuse_factor * diffuse + specular_factor * specular;
  out_color /= out_color + 1;
  out_color = pow(out_color, 1 / kGamma);

  return float4(out_color, 1);
}
