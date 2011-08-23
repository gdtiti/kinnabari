// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

sampler g_smp_img0;
sampler g_smp_img1;

extern float4x3 g_cc_mtx;

extern float3 g_in_black;
extern float3 g_in_inv_range; // 1 / (in_white - in_black)
extern float3 g_inv_gamma;
extern float3 g_out_black;
extern float3 g_out_range; // (out_white - out_black)

extern float3 g_lum_param; // x = scale, y = bias

static float3 c_luma_vec = {0.299, 0.587, 0.114};
static float3 c_vec_Cr = {0.5, -0.419, -0.081};
static float3 c_vec_Cb = {-0.169, -0.331, 0.5};

half4 CC_levels(sampler smp, float2 uv) {
	half4 c = half4(tex2D(smp, uv));
	half3 cadj = (c.rgb - (half3)g_in_black) * (half3)g_in_inv_range;
	cadj = (half3)saturate(cadj*(half3)g_out_range + (half3)g_out_black);
	return half4(pow(cadj.rgb, (half3)g_inv_gamma), (half)c.a);
}

float4 CC_lum(sampler smp, float2 uv) {
	float4 c = tex2D(smp, uv);
	float Y = dot(c.rgb, c_luma_vec);
	float Cr = dot(c.rgb, c_vec_Cr);
	float Cb = dot(c.rgb, c_vec_Cb);
	float scale = g_lum_param.x;
	float bias = g_lum_param.y;
	Y = Y*scale + bias;
	float r = Y + Cr*1.402;
	float g = Y - Cr*0.714 - Cb*0.344;
	float b = Y + Cb*1.772;
	return float4(r, g, b, c.a);
}

float4 CC_linear(sampler smp, float2 uv) {
	float4 c = tex2D(smp, uv);
	float3 cc = (mul(float4(c.rgb, 1.0), g_cc_mtx)).rgb;
	return float4(cc, c.a);
}