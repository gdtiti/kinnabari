// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

extern float4 g_fog_param; // start, inv_range, curve_p1, curve_p2
extern float4 g_fog_color;

float Fog_curve(float t) {
	float p1 = g_fog_param.z;
	float p2 = g_fog_param.w;
	return Bezier_01(p1, p2, t);
}

float Fog_val(float3 pos, float3 eye) {
	float start = g_fog_param.x;
	float inv_range = g_fog_param.y;
	float dist = distance(pos, eye);
	float t = saturate((dist - start) * inv_range);
	return Fog_curve(t);
}

float3 Fog(float3 clr, float3 pos, float3 eye) {
	float val = clamp(Fog_val(pos, eye), 0, g_fog_color.a);
	return lerp(clr, g_fog_color.rgb, val);
}
