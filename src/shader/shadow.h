// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

sampler g_smp_shadow;

extern float3 g_shadow_dir;
extern float3 g_shadow_offs;
extern float4 g_shadow_color;
extern float4x4 g_shadow_proj;
extern float4 g_shadow_param;

struct RECV_PIX {
	float4 spos;
	float3 wpos;
	float3 wnrm;
};

float4 Cast_xform(VTX vtx, wmtx_t wm) {
	float3 wpos = mul(wm, float4(vtx.pos.xyz, 1));
	wpos += g_shadow_offs;
	return mul(float4(wpos.xyz, 1), g_view_proj);
}

float4 Recv_xform(VTX vtx, wmtx_t wm, out RECV_PIX pix) {
	pix = (RECV_PIX)0;
	float3 wpos = mul(wm, float4(vtx.pos.xyz, 1));
	float3 wnrm = normalize(mul(wm, float4(vtx.nrm.xyz, 0)).xyz);
	pix.wpos = wpos;
	pix.wnrm = wnrm;
	pix.spos = mul(float4(wpos, 1), g_shadow_proj);
	return Calc_cpos(wpos);
}

float Shadow_smp(sampler smp, float2 uv) {
	return tex2Dlod(smp, float4(uv.x, uv.y, 0, 0)).x;
}

float Shadow_std(sampler smp, float4 spos) {
	float3 tpos = spos.xyz / spos.w;
	float2 uv = tpos.xy;
	float z = tpos.z;
	float d = Shadow_smp(smp, uv);
	return d < z ? 1 : 0;
}

half Shadow_PCF2(sampler smp, float4 spos) {
	float r = g_shadow_param.x;
	float pix_r = g_shadow_param.y;

	spos.xyz /= spos.w;
	float2 uv00 = spos.xy;
	float2 uv11 = uv00 + r;

	float s00 = Shadow_smp(smp, float2(uv00.x, uv00.y));
	float s01 = Shadow_smp(smp, float2(uv00.x, uv11.y));
	float s10 = Shadow_smp(smp, float2(uv11.x, uv00.y));
	float s11 = Shadow_smp(smp, float2(uv11.x, uv11.y));

	half4 z = (half4)step(half4(s00, s01, s10, s11), spos.zzzz);
	half2 t = (half2)frac(uv00 * pix_r);
	half2 v = (half2)lerp(z.xy, z.zw, t.x);
	return (half)lerp(v.x, v.y, t.y);
}

half Shadow_PCF3(sampler smp, float4 spos) {
	float r = g_shadow_param.x;
	float pix_r = g_shadow_param.y;

	spos.xyz /= spos.w;
	float2 uv00 = spos.xy;
	float2 uv_1_1 = uv00 - r;
	float2 uv11 = uv00 + r;

	float s_1_1 = Shadow_smp(smp, float2(uv_1_1.x, uv_1_1.y));
	float s_10 = Shadow_smp(smp, float2(uv_1_1.x, uv00.y));
	float s_11 = Shadow_smp(smp, float2(uv_1_1.x, uv11.y));
	float s0_1 = Shadow_smp(smp, float2(uv00.x, uv_1_1.y));
	float s00 = Shadow_smp(smp, float2(uv00.x, uv00.y));
	float s01 = Shadow_smp(smp, float2(uv00.x, uv11.y));
	float s1_1 = Shadow_smp(smp, float2(uv11.x, uv_1_1.y));
	float s10 = Shadow_smp(smp, float2(uv11.x, uv00.y));
	float s11 = Shadow_smp(smp, float2(uv11.x, uv11.y));

	half3 z = (half3)spos.zzz;
	half3 z0 = (half3)step(half3(s_1_1, s_10, s_11), z);
	half3 z1 = (half3)step(half3( s0_1,  s00,  s01), z);
	half3 z2 = (half3)step(half3( s1_1,  s10,  s11), z);

	half2 t = (half2)frac(uv00 * pix_r);
	half3 s = (z0 + z1) + (z2 - z0)*t.x;
	return ((s.x + s.y) + (s.z - s.x)*t.y) / 4;
}

half Shadow_PCF4(sampler smp, float4 spos) {
	float r = g_shadow_param.x;
	float pix_r = g_shadow_param.y;

	spos.xyz /= spos.w;
	float2 uv00 = spos.xy;
	float2 uv_1_1 = uv00 - r;
	float2 uv11 = uv00 + r;
	float2 uv_2_2 = uv00 - r*2;

	float s_2_2 = Shadow_smp(smp, float2(uv_2_2.x, uv_2_2.y));
	float s_2_1 = Shadow_smp(smp, float2(uv_2_2.x, uv_1_1.y));
	float s_20 = Shadow_smp(smp, float2(uv_2_2.x, uv00.y));
	float s_21 = Shadow_smp(smp, float2(uv_2_2.x, uv11.y));

	float s_1_2 = Shadow_smp(smp, float2(uv_1_1.x, uv_2_2.y));
	float s_1_1 = Shadow_smp(smp, float2(uv_1_1.x, uv_1_1.y));
	float s_10 = Shadow_smp(smp, float2(uv_1_1.x, uv00.y));
	float s_11 = Shadow_smp(smp, float2(uv_1_1.x, uv11.y));

	float s0_2 = Shadow_smp(smp, float2(uv00.x, uv_2_2.y));
	float s0_1 = Shadow_smp(smp, float2(uv00.x, uv_1_1.y));
	float s00 = Shadow_smp(smp, float2(uv00.x, uv00.y));
	float s01 = Shadow_smp(smp, float2(uv00.x, uv11.y));

	float s1_2 = Shadow_smp(smp, float2(uv11.x, uv_2_2.y));
	float s1_1 = Shadow_smp(smp, float2(uv11.x, uv_1_1.y));
	float s10 = Shadow_smp(smp, float2(uv11.x, uv00.y));
	float s11 = Shadow_smp(smp, float2(uv11.x, uv11.y));

	half4 z = (half4)spos.zzzz;
	half4 z0 = (half4)step(half4(s_2_2, s_2_1, s_20, s_21), z);
	half4 z1 = (half4)step(half4(s_1_2, s_1_1, s_10, s_11), z);
	half4 z2 = (half4)step(half4( s0_2,  s0_1,  s00,  s01), z);
	half4 z3 = (half4)step(half4( s1_2,  s1_1,  s10,  s11), z);

	half2 t = (half2)frac(uv00 * pix_r);
	half4 s = (z0 + z1 + z2) + (z3 - z0)*t.x;
	return ((s.x + s.y + s.z) + (s.w - s.x)*t.y) / 9;
}

float Shadow(sampler smp, float4 spos) {
	//return Shadow_std(smp, spos);
	//return Shadow_PCF2(smp, spos);
	return Shadow_PCF3(smp, spos);
	//return Shadow_PCF4(smp, spos);
}
