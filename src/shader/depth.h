// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

float3 Encode_D24(float z) {
	float z16 = z*(1<<8);
	float z16f = frac(z16);
	float z16i = z16 - z16f;

	float z8 = z16f*(1<<8);
	float z8f = frac(z8);
	float z8i = z8 - z8f;

	return float3(z8f, z8i / 255.0f, z16i / 255.0f);
}

float Decode_D24(float3 zvec) {
	return dot(zvec,
	           float3(
	           (1.0f/(1<<16)) - (1.0f/((1<<24) + 1)),
	           (1.0f/(1<<8)) - (1.0f/((1<<16) + 1)),
	           1.0f - (1.0f/(1<<8)) + 5e-8f
	           ));
}
