// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "xform.h"

void main(VTX vtx, out float4 cpos : POSITION, out PIX pix : TEXCOORD, out float4 clr : COLOR) {
	cpos = Xform(vtx, Get_wmtx(vtx), pix);
	clr = vtx.clr;
}
