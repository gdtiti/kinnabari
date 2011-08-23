// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "xform.h"
#include "shadow.h"

void main(RECV_PIX pix : TEXCOORD, out float4 c : COLOR) {
	float4 cpos = pix.spos.xyzz/pix.spos.w;
	clip(cpos);
	clip(1-cpos);

	float shadow = Shadow(g_smp_shadow, pix.spos);
	c.rgb = g_shadow_color.rgb;
	c.a = shadow * g_shadow_color.a;
}
