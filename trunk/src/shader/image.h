// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "color.h"

sampler g_smp_img0;
sampler g_smp_img1;

extern float4x3 g_cc_mtx;

extern float3 g_in_black;
extern float3 g_in_inv_range; // 1 / (in_white - in_black)
extern float3 g_inv_gamma;
extern float3 g_out_black;
extern float3 g_out_range; // (out_white - out_black)

extern float3 g_lum_param; // x = scale, y = bias

half4 CC_levels(sampler smp, float2 uv) {
	half4 c = half4(tex2D(smp, uv));
	half3 cadj = (c.rgb - (half3)g_in_black) * (half3)g_in_inv_range;
	cadj = (half3)saturate(cadj*(half3)g_out_range + (half3)g_out_black);
	return half4(pow(cadj.rgb, (half3)g_inv_gamma), (half)c.a);
}

float4 CC_lum(sampler smp, float2 uv) {
	float4 c = tex2D(smp, uv);
	c.rgb = pow(c.rgb, g_inv_gamma);
	float3 YCbCr = CLR_RGB_to_YCbCr(c.rgb);
	float Y = YCbCr.x;
	float scale = g_lum_param.x;
	float bias = g_lum_param.y;
	Y = Y*scale + bias;
	YCbCr.x = Y;
	c.rgb = CLR_YCbCr_to_RGB(YCbCr);
	return c;
}

float4 CC_linear(sampler smp, float2 uv) {
	float4 c = tex2D(smp, uv);
	float3 cc = (mul(float4(c.rgb, 1.0), g_cc_mtx)).rgb;
	return float4(cc, c.a);
}