// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#define D_ENVMAP_CUBE 0
#define D_ENVMAP_STRIP 1

extern float3 g_view_pos;
extern float4 g_base_color;
extern float4 g_spec_param; // fresnel_scale, fresnel_bias, fresnel_pow, spec_pow
extern float4 g_bump_param; // binormal_factor, plx_u, plx_v
extern float4 g_env_param; // rgb: tint, w: level

sampler g_smp_base;
sampler g_smp_mask;
sampler g_smp_bump;
samplerCUBE g_smp_cube;
sampler g_smp_env;

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

float4 Env_cube(float3 nrm, float3 ivec, samplerCUBE smp, float lvl) {
	float3 rvec = reflect(ivec, nrm);
	return texCUBElod(smp, float4(rvec, lvl));
}

float4 Env_strip(float3 nrm, float3 ivec, sampler2D smp, float lvl) {
	float3 v = reflect(ivec, nrm);
	float2 uv, org;
	float3 a = abs(v);
	if (a.x >= a.y && a.x >= a.z) {
		uv = float2(v.z / v.x, v.y / a.x);
		if (v.x > 0.0) {
			org = float2(0.0, 0.0);
			uv.x = -uv.x;
		} else {
			org = float2(1.0/6.0, 0.0);
			uv.x = -uv.x;
		}
	} else if (a.z >= a.x && a.z >= a.y) {
		uv = float2(v.x / a.z, -v.y / v.z);
		if (v.z > 0.0) {
			org = float2(4.0/6.0, 0.0);
			uv.y = -uv.y;
		} else {
			org = float2(5.0/6.0, 0.0);
			uv.y = -uv.y;
		}
	} else {
		uv = float2(v.x / a.y, -v.z / v.y);
		if (v.y > 0.0) {
			org = float2(2.0/6.0, 0.0);
		} else {
			org = float2(3.0/6.0, 0.0);
		}
	}
	uv = (uv + 1.0) * 0.5;
	uv.x /= 6.0;
	uv += org;
	return tex2Dlod(smp, float4(uv.x, uv.y, 0.0, lvl));
}

float3 Simple_spec(float3 pos, float3 nrm, float2 uv, int env_mode) {
	float3 ivec = normalize(pos - g_view_pos.xyz);
	float4 env;
	if (env_mode == D_ENVMAP_STRIP) {
		env = Env_strip(nrm, ivec, g_smp_env, g_env_param.w);
	} else {
		env = Env_cube(nrm, ivec, g_smp_cube, g_env_param.w);
	}
	float4 mask = tex2D(g_smp_mask, uv);
	float fr_scale = g_spec_param.x;
	float fr_bias = g_spec_param.y;
	float fr_pwr = g_spec_param.z;
	float fr = Fresnel(ivec, nrm, fr_scale, fr_bias, fr_pwr);
	float3 spec = (env.rgb * env.a * fr) * g_env_param.rgb * mask.rgb;
	return spec;
}
