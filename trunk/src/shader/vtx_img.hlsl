// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

void main(
	float3 in_pos : POSITION,
	float4 in_tex : TEXCOORD,
	out float4 out_pos : POSITION,
	out float4 out_tex : TEXCOORD
) {
	out_pos = float4(in_pos.xyz, 1);
	out_tex = in_tex;
}
