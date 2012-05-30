// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "depth.h"

void main(float4 cpos : TEXCOORD0, out float4 c : COLOR) {
	float z = cpos.z / cpos.w;
	c.rgb = Encode_D24(z);
	c.a = 1.0;
}
