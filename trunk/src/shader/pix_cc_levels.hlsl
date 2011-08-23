// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

#include "image.h"

half4 main(float2 uv : TEXCOORD0) : COLOR {
	return CC_levels(g_smp_img0, uv);
}
