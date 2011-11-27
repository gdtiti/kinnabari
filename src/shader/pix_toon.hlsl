// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "util.h"
#include "xform.h"
#include "light.h"
#include "fog.h"
#include "material.h"

extern float4 g_toon_sun_dir;
extern float4 g_toon_sun_color;
extern float4 g_toon_sun_param; // scale, bias, pwr, blend
extern float4 g_toon_rim_color;
extern float4 g_toon_rim_param; // scale, bias, pwr
extern float4 g_toon_ctrl; // light_balance

void main(PIX pix : TEXCOORD, float4 clr : COLOR, out float4 c : COLOR) {
	float3 wn = normalize(pix.wnrm.xyz);

	float3 sun_clr = g_toon_sun_color.rgb;
	float sun_val = dot(-g_toon_sun_dir.xyz, wn);
	float sun_scale = g_toon_sun_param.x;
	float sun_bias = g_toon_sun_param.y;
	float sun_pwr = g_toon_sun_param.z;
	float3 sunc = Spec_pow(sun_val*sun_scale + sun_bias, sun_pwr) * sun_clr;

	float3 basec = g_base_color.rgb * clr.rgb;
	float3 lc = basec * sunc;
	half3 cA = (half3)lc;
	half3 cB = (half3)basec;

	half3 hi = (half3)1.0 - ((half3)1.0 - cA)*((half3)1.0 - 2.0h*(cB - (half3)0.5));
	half3 lo = cA * (2.0h*cB);
	half mx = 0.0h; if (cB.x > 0.5h) mx = 1.0h;
	half my = 0.0h; if (cB.y > 0.5h) my = 1.0h;
	half mz = 0.0h; if (cB.z > 0.5h) mz = 1.0h;
	half3 mask = half3(mx, my, mz);
	half3 toonc = hi*mask + lo*((half3)1.0-mask);
	c.rgb = lerp(basec, toonc, g_toon_sun_param.w);

	float3 ivec = normalize(pix.wpos.xyz - g_view_pos.xyz);
	float rim_scale = g_toon_rim_param.x;
	float rim_bias = g_toon_rim_param.y;
	float rim_pwr = g_toon_rim_param.z;
	float rim = Fresnel(ivec, wn, rim_scale, rim_bias, rim_pwr);
	c.rgb = saturate(lerp(c.rgb, c.rgb - (rim * ((float3)1.0 - g_toon_rim_color.rgb)), g_toon_rim_color.a));

	c.rgb = lerp(c.rgb, basec*SH(wn), g_toon_ctrl.x);
	c.rgb = Fog(c.rgb, pix.wpos.xyz, g_view_pos.xyz);

	c.a = 1.0f;
}

