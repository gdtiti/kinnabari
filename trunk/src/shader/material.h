// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

extern float3 g_view_pos;
extern float4 g_base_color;
extern float4 g_spec_param; // fresnel_scale, fresnel_bias, fresnel_pow, spec_pow

sampler g_smp_base;
sampler g_smp_mask;
sampler g_smp_bump;

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

