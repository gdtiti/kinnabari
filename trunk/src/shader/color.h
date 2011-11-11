// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

static float3 c_luma_vec = {0.299, 0.587, 0.114}; // Rec.601
static float3 c_vec_Cb = {-0.169, -0.331, 0.5};
static float3 c_vec_Cr = {0.5, -0.419, -0.081};

float3 CLR_RGB_to_YCbCr(float3 c) {
	float Y = dot(c.rgb, c_luma_vec); // Y'
	float Cb = dot(c.rgb, c_vec_Cb);
	float Cr = dot(c.rgb, c_vec_Cr);
	return float3(Y, Cb, Cr);
}

float3 CLR_YCbCr_to_RGB(float3 c) {
	float Y = c.x;
	float Cb = c.y;
	float Cr = c.z;
	float r = Y + Cr*1.402;
	float g = Y - Cr*0.714 - Cb*0.344;
	float b = Y + Cb*1.772;
	return float3(r, g, b);
}