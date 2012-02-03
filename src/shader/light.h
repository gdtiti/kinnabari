// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

extern bool g_omni_sw0;
extern bool g_omni_sw1;
extern bool g_omni_sw2;
extern bool g_omni_sw3;

// {float3 color; float start;}, {float3 pos; float falloff;}
// falloff = 1/(end-start)
extern float4 g_omni_param[2*4];

extern float4 g_SH[7];
extern float4 g_ambient_color;


float Dist_attn(float dist, float start, float falloff) {
	return (1.0 - min(max((dist-start)*falloff, 0.0), 1.0));
}

float3 Omni_diffuse(int light_no, float3 pos, float3 norm) {
	float3 lcolor = g_omni_param[light_no*2 + 0].xyz;
	float3 lpos = g_omni_param[light_no*2 + 1].xyz;
	float start = g_omni_param[light_no*2 + 0].w;
	float falloff = g_omni_param[light_no*2 + 1].w;
	float3 lvec = lpos - pos;
	float dist2 = dot(lvec, lvec);
	float inv_dist = rsqrt(dist2);
	float3 ivec = lvec * inv_dist; // normalize
	float dist = dist2 * inv_dist;
	float attn = Dist_attn(dist, start, falloff);
	return max(dot(ivec, norm), 0) * lcolor * attn;
}

float3 Calc_omni_lights(float3 pos, float3 nrm) {
	float3 diff = 0;
	if (g_omni_sw0) {
		diff = Omni_diffuse(0, pos, nrm);
		if (g_omni_sw1) {
			diff += Omni_diffuse(1, pos, nrm);
			if (g_omni_sw2) {
				diff += Omni_diffuse(2, pos, nrm);
				if (g_omni_sw3) {
					diff += Omni_diffuse(3, pos, nrm);
				}
			}
		}
	}
	return diff;
}


half3 SH(float3 norm) {
	half4 n = half4(norm, 1.0);
	half3 v0 = half3(
		dot(n, g_SH[0]),
		dot(n, g_SH[1]),
		dot(n, g_SH[2])
	);
	half4 nn = n.xyzx * n.yzzz;
	half3 v1 = half3(
		dot(nn, g_SH[3]),
		dot(nn, g_SH[4]),
		dot(nn, g_SH[5])
	);
	half3 shc = v0 + v1;
	shc += (n.x*n.x - n.y*n.y) * half3(g_SH[6].xyz);
	return shc;
}

half3 SH_rev(float3 norm) {
	half4 n = half4(norm, 1.0);
	half3 v0 = half3(
		dot(n, g_SH[0]),
		dot(n, g_SH[1]),
		dot(n, g_SH[2])
	);
	half4 nn = n.xyzx * n.yzzz;
	half3 v1 = half3(
		dot(nn, g_SH[3]),
		dot(nn, g_SH[4]),
		dot(nn, g_SH[5])
	);
	half3 v2 = (n.x*n.x - n.y*n.y) * half3(g_SH[6].xyz);
	half3 shc = v0 + v1 + v2;

	half3 v0_rev = half3(
		dot(-n, g_SH[0]),
		dot(-n, g_SH[1]),
		dot(-n, g_SH[2])
	);
	half3 shc_rev = v0_rev + v1 + v2;

	return shc + shc_rev*0.25h;
}

half3 SH2(float3 norm) {
	half4 n = half4(norm, 1.0);
	half3 shc = half3(
		dot(n, g_SH[0]),
		dot(n, g_SH[1]),
		dot(n, g_SH[2])
	);
	return shc;
}
