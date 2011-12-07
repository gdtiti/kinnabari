// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "xform.h"
#include "material.h"

void main(VTX vtx, out float4 cpos : POSITION, out PIX pix : TEXCOORD, out float4 clr : TEXCOORD5) {
	wmtx_t wm = Get_wmtx_skin(vtx);
	cpos = Xform(vtx, wm, pix);
	Xform_tangent(vtx, wm, g_bump_param.x, pix);
	clr = float4(0.0f, 0.0f, 0.0f, 1.0f);
}
