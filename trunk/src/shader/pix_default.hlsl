// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "util.h"
#include "xform.h"
#include "light.h"
#include "fog.h"
#include "material.h"

void main(PIX pix : TEXCOORD, float4 clr : TEXCOORD5, out float4 c : COLOR) {
	float3 wpos = pix.wpos.xyz;
	float2 uv = pix.tex.xy;
	float4 tex = tex2D(g_smp_base, uv);
	float3 wn = NMap_normal(normalize(pix.wnrm), normalize(pix.wtng), normalize(pix.wbnm), g_smp_bump, uv);
	float3 lc = Calc_omni_lights(wpos, wn) + SH(wn) + g_ambient_color.rgb;
	lc += clr.rgb;
	c.rgb = tex.rgb * g_base_color.rgb * lc;
	c.rgb += Simple_spec(wpos, wn, uv, D_ENVMAP_CUBE);
	c.rgb = Fog(c.rgb, wpos, g_view_pos);
	c.a = tex.a;
}

