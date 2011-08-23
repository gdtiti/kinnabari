// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "xform.h"
#include "light.h"
#include "material.h"

void main(PIX pix : TEXCOORD, out float4 c : COLOR) {
	float3 wn = normalize(pix.wnrm.xyz);
	c.rgb = (half3)g_base_color.rgb * SH(wn);
	c.a = 1.0f;
}
