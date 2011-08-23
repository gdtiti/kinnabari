// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "xform.h"
#include "shadow.h"

void main(float4 spos : TEXCOORD0, out float4 c : COLOR) {
	c = float4(spos.z / spos.w, 0, 0, 1);
}
