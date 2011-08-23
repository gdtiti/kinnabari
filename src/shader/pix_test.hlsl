// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "xform.h"
#include "light.h"
#include "material.h"

extern float g_fparam[2];

void main(PIX pix : TEXCOORD, out float4 c : COLOR) {
	float3 wn = normalize(pix.wnrm.xyz);
	float3 ldiff = Calc_omni_lights(pix.wpos, pix.wnrm);
	ldiff += SH(wn);
	float4 tex = tex2D(g_smp_base, pix.tex.xy);
	c.rgb = tex.rgb * g_base_color.rgb * ldiff;
	c.rgb *= g_fparam[0] + g_fparam[1];
	c.a = 1.0f;
}
