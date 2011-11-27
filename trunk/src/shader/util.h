// This file is part of Kinnabari.
// Copyright 2011 Sergey Chaban <sergey.chaban@gmail.com>
// Released under the terms of Version 3 of the GNU General Public License.
// See LICENSE.txt for details.

static float4x4 c_bez_mtx = {
	{-1.0f, 3.0f, -3.0f, 1.0f},
	{3.0f, -6.0f, 3.0f, 0.0f},
	{-3.0f, 3.0f, 0.0f, 0.0f},
	{1.0f, 0.0f, 0.0f, 0.0f}
};

float Bezier_01(float p1, float p2, float t) {
	float tt = t*t;
	float ttt = tt*t;
	float4 v = float4(0.0f, p1, p2, 1.0f);
	return dot(v, mul(float4(ttt, tt, t, 1.0f), c_bez_mtx));
}