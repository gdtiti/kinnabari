// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

extern float3 g_view_pos;
extern float4 g_base_color;
extern float4 g_spec_param; // fresnel_scale, fresnel_bias, fresnel_pow, spec_pow
extern float4 g_bump_param; // binormal_factor, plx_u, plx_v
extern float4 g_env_param; // rgb: tint, w: level

sampler g_smp_base;
sampler g_smp_mask;
sampler g_smp_bump;
samplerCUBE g_smp_cube;

float Spec_pow(float d, float shin) {
#if 1
	return pow(abs(d), shin);
#else
	float x = d < 0.001 ? -9.96578407 : log2(d);
	return exp2(x*shin);
#endif
}

float Fresnel_val(float tcos, float scale, float bias, float pwr) {
	return saturate((Spec_pow(1.0f - tcos, pwr) * scale) + bias);
}

float Fresnel(float3 ivec, float3 norm, float scale, float bias, float pwr) {
	float tcos = dot(ivec, norm);
	return 1.0f - Fresnel_val(tcos, scale, bias, pwr);
}

float3 Decode_nmap(float4 ntex) {
	ntex += float4(0.0f, -0.5f, 0.0f, -0.5f);
	ntex += ntex;
	float nx = ntex.a;
	float ny = ntex.g;
	float nz = sqrt(1.0f - nx*nx - ny*ny);
	return float3(nx, ny, nz);
}

float3 NMap_normal(float3 nrm, float3 tng, float3 bnm, sampler smp, float2 uv) {
	float4 ntex = tex2D(smp, uv);
	float3 n = Decode_nmap(ntex);
	return normalize(n.x*tng + n.y*bnm + n.z*nrm);
}

float4 Cube_env(float3 nrm, float3 ivec, samplerCUBE smp, float lvl) {
	float3 rvec = reflect(ivec, nrm);
	return texCUBElod(smp, float4(rvec, lvl));
}

float3 Simple_spec(float3 pos, float3 nrm, float2 uv) {
	float3 ivec = normalize(pos - g_view_pos.xyz);
	float4 env = Cube_env(nrm, ivec, g_smp_cube, g_env_param.w);
	float4 mask = tex2D(g_smp_mask, uv);
	float fr_scale = g_spec_param.x;
	float fr_bias = g_spec_param.y;
	float fr_pwr = g_spec_param.z;
	float fr = Fresnel(ivec, nrm, fr_scale, fr_bias, fr_pwr);
	float3 spec = (env.rgb * env.a * fr) * g_env_param.rgb * mask.rgb;
	return spec;
}
