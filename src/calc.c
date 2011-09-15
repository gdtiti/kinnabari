/*
 * Kinnabari
 * Copyright 2011 Sergey Chaban
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Sergey Chaban <sergey.chaban@gmail.com>
 */

#include "system.h"
#include "calc.h"

QMTX g_identity = {
	{1.0f, 0.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 1.0f, 0.0f},
	{0.0f, 0.0f, 0.0f, 1.0f}
};

static D_FORCE_INLINE void SinCos(float rad, float res[2]) {
#if D_KISS || defined(_WIN64)
	res[0] = sinf(rad);
	res[1] = cosf(rad);
#else
#	if defined (__GNUC__)
	asm (
		"fsincos"
		: "=t" (res[1]), "=u" (res[0])
		: "0" (rad)
	);
#	else
	__asm {
		fld rad
		mov eax, res
		fsincos
		fstp dword ptr [eax + 4]
		fstp dword ptr [eax]
	}
#	endif
#endif
}

float F_min(float x, float y) {
#if D_KISS
	return D_MIN(x, y);
#else
	UVEC v;
	v.qv = _mm_min_ss(_mm_set_ss(x), _mm_set_ss(y));
	return v.f[0];
#endif
}

float F_max(float x, float y) {
#if D_KISS
	return D_MAX(x, y);
#else
	UVEC v;
	v.qv = _mm_max_ss(_mm_set_ss(x), _mm_set_ss(y));
	return v.f[0];
#endif
}

void VEC_cpy(VEC vdst, VEC vsrc) {
	int i;
	for (i = 0; i < 3; ++i) vdst[i] = vsrc[i];
}

void VEC_add(VEC v0, VEC v1, VEC v2) {
	int i;
	for (i = 0; i < 3; ++i) v0[i] = v1[i] + v2[i];
}

void VEC_sub(VEC v0, VEC v1, VEC v2) {
	int i;
	for (i = 0; i < 3; ++i) v0[i] = v1[i] - v2[i];
}

void VEC_mul(VEC v0, VEC v1, VEC v2) {
	int i;
	for (i = 0; i < 3; ++i) v0[i] = v1[i] * v2[i];
}

void VEC_div(VEC v0, VEC v1, VEC v2) {
	int i;
	for (i = 0; i < 3; ++i) v0[i] = v1[i] / v2[i];
}

void VEC_cross(VEC v0, VEC v1, VEC v2) {
	float v1_0 = v1[0];
	float v1_1 = v1[1];
	float v1_2 = v1[2];
	float v2_0 = v2[0];
	float v2_1 = v2[1];
	float v2_2 = v2[2];
	v0[0] = v1_1*v2_2 - v1_2*v2_1;
	v0[1] = v1_2*v2_0 - v1_0*v2_2;
	v0[2] = v1_0*v2_1 - v1_1*v2_0;
}

float VEC_dot(VEC v0, VEC v1) {
	return (v0[0]*v1[0] + v0[1]*v1[1] + v0[2]*v1[2]);
}

float VEC_mag2(VEC v) {
	return VEC_dot(v, v);
}

float VEC_mag(VEC v) {
	return sqrtf(VEC_mag2(v));
}

void V4_store(float* p, QVEC v) {
#if D_KISS
	UVEC v0;
	int i;
	v0.qv = v;
	for (i = 0; i < 4; ++i) *p++ = v0.f[i];
#else
	_mm_store_ps(p, v);
#endif
}

float V4_at(QVEC v, int idx) {
	UVEC v0;
	v0.qv = v;
	return v0.f[idx];
}

QVEC V4_set(float x, float y, float z, float w) {
#if D_KISS
	UVEC v0;
	v0.x = x;
	v0.y = y;
	v0.z = z;
	v0.w = w;
	return v0.qv;
#else
	if (0) {
		return _mm_set_ps(w, z, y, x);
	} else {
		return _mm_unpacklo_ps(_mm_unpacklo_ps(_mm_set_ss(x), _mm_set_ss(z)), _mm_unpacklo_ps(_mm_set_ss(y), _mm_set_ss(w)));
	}
#endif
}

QVEC V4_set_vec(float x, float y, float z) {
	return V4_set(x, y, z, 0.0f);
}

QVEC V4_set_pnt(float x, float y, float z) {
	return V4_set(x, y, z, 1.0f);
}

QVEC V4_fill(float x) {
#if D_KISS
	UVEC v0;
	int i;
	for (i = 0; i < 4; ++i) v0.f[i] = x;
	return v0.qv;
#else
	return _mm_set_ps1(x);
#endif
}

QVEC V4_load(float* p) {
#if D_KISS
	UVEC v0;
	int i;
	for (i = 0; i < 4; ++i) v0.f[i] = *p++;
	return v0.qv;
#else
	return _mm_load_ps(p);
#endif
}

QVEC V4_zero() {
#if D_KISS
	UVEC v0;
	v0.x = 0.0f;
	v0.y = 0.0f;
	v0.z = 0.0f;
	v0.w = 0.0f;
	return v0.qv;
#else
	return _mm_setzero_ps();
#endif
}

QVEC V4_set_w0(QVEC v) {
#if D_KISS
	UVEC v0;
	v0.qv = v;
	v0.w = 0.0f;
	return v0.qv;
#else
	return _mm_shuffle_ps(v, _mm_unpackhi_ps(v, _mm_setzero_ps()), _MM_SHUFFLE(3, 0, 1, 0));
#endif
}

QVEC V4_set_w1(QVEC v) {
#if D_KISS
	UVEC v0;
	v0.qv = v;
	v0.w = 1.0f;
	return v0.qv;
#else
	return _mm_shuffle_ps(v, _mm_unpackhi_ps(v, _mm_set1_ps(1.0f)), _MM_SHUFFLE(3, 0, 1, 0));
#endif
}

QVEC V4_set_w(QVEC v, float w) {
#if D_KISS
	UVEC v0;
	v0.qv = v;
	v0.w = w;
	return v0.qv;
#else
	return _mm_shuffle_ps(v, _mm_unpackhi_ps(v, _mm_set1_ps(w)), _MM_SHUFFLE(3, 0, 1, 0));
#endif
}

QVEC V4_scale(QVEC v, float s) {
#if D_KISS
	UVEC v0;
	int i;
	v0.qv = v;
	for (i = 0; i < 4; ++i) {v0.f[i] *= s;}
	return v0.qv;
#else
	return _mm_mul_ps(v, _mm_set_ps1(s));
#endif
}

QVEC V4_normalize(QVEC v) {
	if (V4_same_xyz(v, V4_zero())) {
		return v;
	}
	return V4_set_w0(V4_scale(v, 1.0f / V4_mag(v)));
}

QVEC V4_add(QVEC a, QVEC b) {
#if D_KISS
	UVEC v0;
	UVEC v1;
	UVEC v2;
	int i;
	v1.qv = a;
	v2.qv = b;
	for (i = 0; i < 4; ++i) {v0.f[i] = v1.f[i] + v2.f[i];}
	return v0.qv;
#else
	return _mm_add_ps(a, b);
#endif
}

QVEC V4_sub(QVEC a, QVEC b) {
#if D_KISS
	UVEC v0;
	UVEC v1;
	UVEC v2;
	int i;
	v1.qv = a;
	v2.qv = b;
	for (i = 0; i < 4; ++i) {v0.f[i] = v1.f[i] - v2.f[i];}
	return v0.qv;
#else
	return _mm_sub_ps(a, b);
#endif
}

QVEC V4_mul(QVEC a, QVEC b) {
#if D_KISS
	UVEC v0;
	UVEC v1;
	UVEC v2;
	int i;
	v1.qv = a;
	v2.qv = b;
	for (i = 0; i < 4; ++i) {v0.f[i] = v1.f[i] * v2.f[i];}
	return v0.qv;
#else
	return _mm_mul_ps(a, b);
#endif
}

QVEC V4_div(QVEC a, QVEC b) {
#if D_KISS
	UVEC v0;
	UVEC v1;
	UVEC v2;
	int i;
	v1.qv = a;
	v2.qv = b;
	for (i = 0; i < 4; ++i) {v0.f[i] = v1.f[i] / v2.f[i];}
	return v0.qv;
#else
	return _mm_div_ps(a, b);
#endif
}

QVEC V4_combine(QVEC a, float sa, QVEC b, float sb) {
	return V4_add(V4_scale(a, sa), V4_scale(b, sb));
}

QVEC V4_lerp(QVEC a, QVEC b, float bias) {
	return V4_add(a, V4_scale(V4_sub(b, a), bias));
}

QVEC V4_cross(QVEC a, QVEC b) {
#if D_KISS
	UVEC v0;
	UVEC v1;
	UVEC v2;
	v1.qv = a;
	v2.qv = b;
	v0.f[0] = v1.y*v2.z - v1.z*v2.y;
	v0.f[1] = v1.z*v2.x - v1.x*v2.z;
	v0.f[2] = v1.x*v2.y - v1.y*v2.x;
	v0.f[3] = 0.0f;
	return v0.qv;
#else
	return _mm_sub_ps(
		_mm_mul_ps(
			_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)),
			_mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2))),
		_mm_mul_ps(
			_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2)),
			_mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1)))
	);
#endif
}

D_FORCE_INLINE float V4_dot(QVEC a, QVEC b) {
	UVEC v1;
	UVEC v2;
	v1.qv = a;
	v2.qv = b;
	return (v1.x*v2.x + v1.y*v2.y + v1.z*v2.z);
}

float V4_dot4(QVEC a, QVEC b) {
#if D_KISS
	UVEC v1;
	UVEC v2;
	v1.qv = a;
	v2.qv = b;
	return (v1.x*v2.x + v1.y*v2.y + v1.z*v2.z + v1.w*v2.w);
#else
	float res;
	QVEC t = V4_mul(a, b);
	t = _mm_hadd_ps(t, t);
	if (0) {
		t = _mm_hadd_ps(t, t);
	} else {
		__m128i hi = _mm_srli_epi64(D_M128I(t), 32);
		t = _mm_add_ss(t, D_M128(hi));
	}
	_mm_store_ss(&res, t);
	return res;
#endif
}

D_FORCE_INLINE float V4_triple(QVEC v0, QVEC v1, QVEC v2) {
	return V4_dot4(V4_cross(v0, v1), v2);
}

float V4_mag2(QVEC v) {
	return V4_dot(v, v);
}

float V4_mag(QVEC v) {
	return sqrtf(V4_mag2(v));
}

float V4_dist(QVEC v0, QVEC v1) {
	return V4_mag(V4_sub(v1, v0));
}

float V4_dist2(QVEC v0, QVEC v1) {
	return V4_mag2(V4_sub(v1, v0));
}

QVEC V4_min(QVEC a, QVEC b) {
#if D_KISS
	UVEC v0;
	UVEC v1;
	UVEC v2;
	int i;
	v1.qv = a;
	v2.qv = b;
	for (i = 0; i < 4; ++i) {v0.f[i] = D_MIN(v1.f[i], v2.f[i]);}
	return v0.qv;
#else
	return _mm_min_ps(a, b);
#endif
}

QVEC V4_max(QVEC a, QVEC b) {
#if D_KISS
	UVEC v0;
	UVEC v1;
	UVEC v2;
	int i;
	v1.qv = a;
	v2.qv = b;
	for (i = 0; i < 4; ++i) {v0.f[i] = D_MAX(v1.f[i], v2.f[i]);}
	return v0.qv;
#else
	return _mm_max_ps(a, b);
#endif
}

QVEC V4_abs(QVEC v) {
#if D_KISS
	UVEC v0;
	UVEC v1;
	int i;
	v1.qv = v;
	for (i = 0; i < 4; ++i) {v0.f[i] = fabsf(v1.f[i]);}
	return v0.qv;
#else
	static D_DATA_ALIGN16(int abs_mask[4]) = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};
	return _mm_and_ps(v, _mm_load_ps((float*)abs_mask));
#endif
}

QVEC V4_inv(QVEC v) {
#if D_KISS
	UVEC v0;
	UVEC v1;
	int i;
	v1.qv = v;
	for (i = 0; i < 4; ++i) {v0.f[i] = 1.0f/v1.f[i];}
	return v0.qv;
#else
	__m128i ione = _mm_insert_epi16(_mm_setzero_si128(), 0x3F80, 1);
	__m128 one = D_M128(ione);
	return _mm_div_ps(_mm_shuffle_ps(one, one, 0), v);
#endif
}

QVEC V4_rcp(QVEC v) {
#if D_KISS
	return V4_inv(v);
#else
	__m128 tmp = _mm_rcp_ps(v);
	__m128 tmp2 = _mm_mul_ps(tmp, tmp);
	return _mm_sub_ps(_mm_add_ps(tmp, tmp), _mm_mul_ps(v, tmp2));
#endif
}

int V4_same(QVEC a, QVEC b) {
#if D_KISS
	UVEC v1;
	UVEC v2;
	int i;
	v1.qv = a;
	v2.qv = b;
	for (i = 0; i < 4; ++i) {if (v1.f[i] != v2.f[i]) return 0;}
	return 1;
#else
	return (0xF == _mm_movemask_ps(_mm_cmpeq_ps(a, b)));
#endif
}

int V4_same_xyz(QVEC a, QVEC b) {
#if D_KISS
	UVEC v1;
	UVEC v2;
	int i;
	v1.qv = a;
	v2.qv = b;
	for (i = 0; i < 3; ++i) {if (v1.f[i] != v2.f[i]) return 0;}
	return 1;
#else
	return (7 == (7 & _mm_movemask_ps(_mm_cmpeq_ps(a, b))));
#endif
}

int V4_eq(QVEC a, QVEC b) {
#if D_KISS
	UVEC v1;
	UVEC v2;
	int i, mask;
	v1.qv = a;
	v2.qv = b;
	mask = 0;
	for (i = 0; i < 4; ++i) {
		if (v1.f[i] == v2.f[i]) mask |= 1<<i;
	}
	return mask;
#else
	return _mm_movemask_ps(_mm_cmpeq_ps(a, b));
#endif
}

void V4_print(QVEC v) {
	UVEC tv;
	tv.qv = v;
	SYS_log("<%.4f %.4f %.4f %.4f>", tv.x, tv.y, tv.z, tv.w);
}

void MTX_cpy(MTX mdst, MTX msrc) {
#if D_KISS
	memcpy(mdst, msrc, sizeof(MTX));
#else
	_mm_store_ps(mdst[0], _mm_load_ps(msrc[0]));
	_mm_store_ps(mdst[1], _mm_load_ps(msrc[1]));
	_mm_store_ps(mdst[2], _mm_load_ps(msrc[2]));
	_mm_store_ps(mdst[3], _mm_load_ps(msrc[3]));
#endif
}

void MTX_clear(MTX m) {
	QVEC zero = V4_zero();
	V4_store(m[0], zero);
	V4_store(m[1], zero);
	V4_store(m[2], zero);
	V4_store(m[3], zero);
}

void MTX_unit(MTX m) {
	MTX_cpy(m, g_identity);
}

void MTX_transpose(MTX m0, MTX m1) {
#if D_KISS
	float t;
	m0[0][0] = m1[0][0];
	m0[1][1] = m1[1][1];
	m0[2][2] = m1[2][2];
	m0[3][3] = m1[3][3];
	t = m1[0][1]; m0[0][1] = m1[1][0]; m0[1][0] = t;
	t = m1[0][2]; m0[0][2] = m1[2][0]; m0[2][0] = t;
	t = m1[0][3]; m0[0][3] = m1[3][0]; m0[3][0] = t;
	t = m1[1][2]; m0[1][2] = m1[2][1]; m0[2][1] = t;
	t = m1[1][3]; m0[1][3] = m1[3][1]; m0[3][1] = t;
	t = m1[2][3]; m0[2][3] = m1[3][2]; m0[3][2] = t;
#else
	__m128 r0 = D_VEC128(m1[0]);
	__m128 r1 = D_VEC128(m1[1]);
	__m128 r2 = D_VEC128(m1[2]);
	__m128 r3 = D_VEC128(m1[3]);
	__m128 t0 = _mm_unpacklo_ps(r0, r1);
	__m128 t1 = _mm_unpackhi_ps(r0, r1);
	__m128 t2 = _mm_unpacklo_ps(r2, r3);
	__m128 t3 = _mm_unpackhi_ps(r2, r3);
	_mm_store_ps(m0[0], _mm_movelh_ps(t0, t2));
	_mm_store_ps(m0[1], _mm_movehl_ps(t2, t0));
	_mm_store_ps(m0[2], _mm_movelh_ps(t1, t3));
	_mm_store_ps(m0[3], _mm_movehl_ps(t3, t1));
#endif
}

void MTX_transpose_sr(MTX m0, MTX m1) {
#if D_KISS
	float t;
	m0[0][0] = m1[0][0];
	m0[1][1] = m1[1][1];
	m0[2][2] = m1[2][2];
	t = m1[0][1]; m0[0][1] = m1[1][0]; m0[1][0] = t;
	t = m1[0][2]; m0[0][2] = m1[2][0]; m0[2][0] = t;
	t = m1[1][2]; m0[1][2] = m1[2][1]; m0[2][1] = t;
	m0[0][3] = 0.0f;
	m0[1][3] = 0.0f;
	m0[2][3] = 0.0f;
	V4_store(m0[3], V4_load(m1[3]));
#else
	__m128 r0 = D_VEC128(m1[0]);
	__m128 r1 = D_VEC128(m1[1]);
	__m128 r2 = D_VEC128(m1[2]);
	__m128 r3 = _mm_setzero_ps();
	__m128 t0 = _mm_unpacklo_ps(r0, r1);
	__m128 t1 = _mm_unpackhi_ps(r0, r1);
	__m128 t2 = _mm_unpacklo_ps(r2, r3);
	__m128 t3 = _mm_unpackhi_ps(r2, r3);
	_mm_store_ps(m0[0], _mm_movelh_ps(t0, t2));
	_mm_store_ps(m0[1], _mm_movehl_ps(t2, t0));
	_mm_store_ps(m0[2], _mm_movelh_ps(t1, t3));
	if (m0 != m1) _mm_store_ps(m0[3], D_VEC128(m1[3]));
#endif
}

void MTX_invert(MTX m0, MTX m1) {
	float det;
	float a0, a1, a2, a3, a4, a5;
	float b0, b1, b2, b3, b4, b5;

	a0 = m1[0][0]*m1[1][1] - m1[0][1]*m1[1][0];
	a1 = m1[0][0]*m1[1][2] - m1[0][2]*m1[1][0];
	a2 = m1[0][0]*m1[1][3] - m1[0][3]*m1[1][0];
	a3 = m1[0][1]*m1[1][2] - m1[0][2]*m1[1][1];
	a4 = m1[0][1]*m1[1][3] - m1[0][3]*m1[1][1];
	a5 = m1[0][2]*m1[1][3] - m1[0][3]*m1[1][2];

	b0 = m1[2][0]*m1[3][1] - m1[2][1]*m1[3][0];
	b1 = m1[2][0]*m1[3][2] - m1[2][2]*m1[3][0];
	b2 = m1[2][0]*m1[3][3] - m1[2][3]*m1[3][0];
	b3 = m1[2][1]*m1[3][2] - m1[2][2]*m1[3][1];
	b4 = m1[2][1]*m1[3][3] - m1[2][3]*m1[3][1];
	b5 = m1[2][2]*m1[3][3] - m1[2][3]*m1[3][2];

	det = a0*b5 - a1*b4 + a2*b3 + a3*b2 - a4*b1 + a5*b0;

	if (det == 0.0f) {
		MTX_clear(m0);
	} else {
		int i;
		float inv_det;
		QMTX minv;

		minv[0][0] =  m1[1][1]*b5 - m1[1][2]*b4 + m1[1][3]*b3;
		minv[1][0] = -m1[1][0]*b5 + m1[1][2]*b2 - m1[1][3]*b1;
		minv[2][0] =  m1[1][0]*b4 - m1[1][1]*b2 + m1[1][3]*b0;
		minv[3][0] = -m1[1][0]*b3 + m1[1][1]*b1 - m1[1][2]*b0;

		minv[0][1] = -m1[0][1]*b5 + m1[0][2]*b4 - m1[0][3]*b3;
		minv[1][1] =  m1[0][0]*b5 - m1[0][2]*b2 + m1[0][3]*b1;
		minv[2][1] = -m1[0][0]*b4 + m1[0][1]*b2 - m1[0][3]*b0;
		minv[3][1] =  m1[0][0]*b3 - m1[0][1]*b1 + m1[0][2]*b0;

		minv[0][2] =  m1[3][1]*a5 - m1[3][2]*a4 + m1[3][3]*a3;
		minv[1][2] = -m1[3][0]*a5 + m1[3][2]*a2 - m1[3][3]*a1;
		minv[2][2] =  m1[3][0]*a4 - m1[3][1]*a2 + m1[3][3]*a0;
		minv[3][2] = -m1[3][0]*a3 + m1[3][1]*a1 - m1[3][2]*a0;

		minv[0][3] = -m1[2][1]*a5 + m1[2][2]*a4 - m1[2][3]*a3;
		minv[1][3] =  m1[2][0]*a5 - m1[2][2]*a2 + m1[2][3]*a1;
		minv[2][3] = -m1[2][0]*a4 + m1[2][1]*a2 - m1[2][3]*a0;
		minv[3][3] =  m1[2][0]*a3 - m1[2][1]*a1 + m1[2][2]*a0;

		inv_det = 1.0f / det;
		for (i = 0; i < 4; ++i) {
			V4_store(m0[i], V4_scale(V4_load(minv[i]), inv_det));
		}
	}
}

void _MTX_invert_fast_static(MTX m0, MTX m1) {
	MTX_invert_fast(m0, m1);
}

D_FORCE_INLINE void MTX_invert_fast(MTX m0, MTX m1) {
#if D_KISS
	UVEC tvec;
	float t;

	tvec.qv = V4_load(m1[3]);
	m0[0][0] = m1[0][0];
	m0[1][1] = m1[1][1];
	m0[2][2] = m1[2][2];
	t = m1[0][1]; m0[0][1] = m1[1][0]; m0[1][0] = t;
	t = m1[0][2]; m0[0][2] = m1[2][0]; m0[2][0] = t;
	t = m1[1][2]; m0[1][2] = m1[2][1]; m0[2][1] = t;
	m0[3][0] = -(tvec.x*m0[0][0] + tvec.y*m0[1][0] + tvec.z*m0[2][0]);
	m0[3][1] = -(tvec.x*m0[0][1] + tvec.y*m0[1][1] + tvec.z*m0[2][1]);
	m0[3][2] = -(tvec.x*m0[0][2] + tvec.y*m0[1][2] + tvec.z*m0[2][2]);
	m0[0][3] = 0.0f;
	m0[1][3] = 0.0f;
	m0[2][3] = 0.0f;
	m0[3][3] = 1.0f;
#else
	QVEC zz = V4_zero();
	QVEC r0 = V4_load(m1[0]);
	QVEC r1 = V4_load(m1[1]);
	QVEC r2 = V4_load(m1[2]);
	QVEC r3 = V4_load(m1[3]);
	QVEC t0 = _mm_unpacklo_ps(r0, r1);
	QVEC t1 = _mm_unpackhi_ps(r0, r1);
	QVEC t2 = _mm_unpacklo_ps(r2, zz);
	QVEC t3 = _mm_unpackhi_ps(r2, zz);
	QIVEC itmp = D_M128I(r3);
	QIVEC itmp_x = _mm_shuffle_epi32(itmp, 0);
	QIVEC itmp_y = _mm_shuffle_epi32(itmp, 0x55);
	QIVEC itmp_z = _mm_shuffle_epi32(itmp, 0xAA);
	r0 = _mm_movelh_ps(t0, t2);
	r1 = _mm_movehl_ps(t2, t0);
	r2 = _mm_movelh_ps(t1, t3);
	r3 = V4_add(V4_add(V4_mul(D_M128(itmp_x), r0), V4_mul(D_M128(itmp_y), r1)), V4_mul(D_M128(itmp_z), r2));
	r3 = V4_sub(zz, r3);
	V4_store(m0[0], r0);
	V4_store(m0[1], r1);
	V4_store(m0[2], r2);
	V4_store(m0[3], V4_set_w1(r3));
#endif
}

D_FORCE_INLINE void MTX_mul(MTX m0, MTX m1, MTX m2) {
#if D_KISS
	QMTX m;

	m[0][0] = m1[0][0]*m2[0][0] + m1[0][1]*m2[1][0] + m1[0][2]*m2[2][0] + m1[0][3]*m2[3][0];
	m[0][1] = m1[0][0]*m2[0][1] + m1[0][1]*m2[1][1] + m1[0][2]*m2[2][1] + m1[0][3]*m2[3][1];
	m[0][2] = m1[0][0]*m2[0][2] + m1[0][1]*m2[1][2] + m1[0][2]*m2[2][2] + m1[0][3]*m2[3][2];
	m[0][3] = m1[0][0]*m2[0][3] + m1[0][1]*m2[1][3] + m1[0][2]*m2[2][3] + m1[0][3]*m2[3][3];

	m[1][0] = m1[1][0]*m2[0][0] + m1[1][1]*m2[1][0] + m1[1][2]*m2[2][0] + m1[1][3]*m2[3][0];
	m[1][1] = m1[1][0]*m2[0][1] + m1[1][1]*m2[1][1] + m1[1][2]*m2[2][1] + m1[1][3]*m2[3][1];
	m[1][2] = m1[1][0]*m2[0][2] + m1[1][1]*m2[1][2] + m1[1][2]*m2[2][2] + m1[1][3]*m2[3][2];
	m[1][3] = m1[1][0]*m2[0][3] + m1[1][1]*m2[1][3] + m1[1][2]*m2[2][3] + m1[1][3]*m2[3][3];

	m[2][0] = m1[2][0]*m2[0][0] + m1[2][1]*m2[1][0] + m1[2][2]*m2[2][0] + m1[2][3]*m2[3][0];
	m[2][1] = m1[2][0]*m2[0][1] + m1[2][1]*m2[1][1] + m1[2][2]*m2[2][1] + m1[2][3]*m2[3][1];
	m[2][2] = m1[2][0]*m2[0][2] + m1[2][1]*m2[1][2] + m1[2][2]*m2[2][2] + m1[2][3]*m2[3][2];
	m[2][3] = m1[2][0]*m2[0][3] + m1[2][1]*m2[1][3] + m1[2][2]*m2[2][3] + m1[2][3]*m2[3][3];

	m[3][0] = m1[3][0]*m2[0][0] + m1[3][1]*m2[1][0] + m1[3][2]*m2[2][0] + m1[3][3]*m2[3][0];
	m[3][1] = m1[3][0]*m2[0][1] + m1[3][1]*m2[1][1] + m1[3][2]*m2[2][1] + m1[3][3]*m2[3][1];
	m[3][2] = m1[3][0]*m2[0][2] + m1[3][1]*m2[1][2] + m1[3][2]*m2[2][2] + m1[3][3]*m2[3][2];
	m[3][3] = m1[3][0]*m2[0][3] + m1[3][1]*m2[1][3] + m1[3][2]*m2[2][3] + m1[3][3]*m2[3][3];

	MTX_cpy(m0, m);
#else
	__m128 __tmp, __tmp2;
	__m128i __itmp;
	__m128 __m2_0 = D_VEC128(m2[0]);
	__m128 __m2_1 = D_VEC128(m2[1]);
	__m128 __m2_2 = D_VEC128(m2[2]);
	__m128 __m2_3 = D_VEC128(m2[3]);
	__tmp = D_VEC128(m1[0]);
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0);
	__tmp2 = _mm_mul_ps(D_M128(__itmp), __m2_0);
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0x55);
	__tmp2 = _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_1));
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0xAA);
	__tmp2 = _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_2));
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0xFF);
	_mm_store_ps(m0[0], _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_3)));
	__tmp = D_VEC128(m1[1]);
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0);
	__tmp2 = _mm_mul_ps(D_M128(__itmp), __m2_0);
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0x55);
	__tmp2 = _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_1));
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0xAA);
	__tmp2 = _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_2));
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0xFF);
	_mm_store_ps(m0[1], _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_3)));
	__tmp = D_VEC128(m1[2]);
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0);
	__tmp2 = _mm_mul_ps(D_M128(__itmp), __m2_0);
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0x55);
	__tmp2 = _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_1));
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0xAA);
	__tmp2 = _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_2));
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0xFF);
	_mm_store_ps(m0[2], _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_3)));
	__tmp = D_VEC128(m1[3]);
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0);
	__tmp2 = _mm_mul_ps(D_M128(__itmp), __m2_0);
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0x55);
	__tmp2 = _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_1));
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0xAA);
	__tmp2 = _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_2));
	__itmp = _mm_shuffle_epi32(D_M128I(__tmp), 0xFF);
	_mm_store_ps(m0[3], _mm_add_ps(__tmp2, _mm_mul_ps(D_M128(__itmp), __m2_3)));
#endif
}

void MTX_rot_x(MTX m, float rad) {
	float s, c;
	float sc[2];
	SinCos(rad, sc);
	s = sc[0];
	c = sc[1];
	MTX_unit(m);
	m[1][1] = c;
	m[1][2] = s;
	m[2][1] = -s;
	m[2][2] = c;
}

void MTX_rot_y(MTX m, float rad) {
	float s, c;
	float sc[2];
	SinCos(rad, sc);
	s = sc[0];
	c = sc[1];
	MTX_unit(m);
	m[0][0] = c;
	m[0][2] = -s;
	m[2][0] = s;
	m[2][2] = c;
}

void MTX_rot_z(MTX m, float rad) {
	float s, c;
	float sc[2];
	SinCos(rad, sc);
	s = sc[0];
	c = sc[1];
	MTX_unit(m);
	m[0][0] = c;
	m[0][1] = s;
	m[1][0] = -s;
	m[1][1] = c;
}

void MTX_rot_xyz(MTX m, float rx, float ry, float rz) {
	QMTX mtmp;
	MTX_rot_x(m, rx);
	MTX_rot_y(mtmp, ry);
	MTX_mul(m, m, mtmp);
	MTX_rot_z(mtmp, rz);
	MTX_mul(m, m, mtmp);
}

void MTX_rot_axis(MTX m, QVEC axis, float rad) {
	UVEC unit_axis;
	UVEC axis2;
	float sc[2];
	float s, c, t;
	float x, y, z;
	float xy, xz, yz;

	SinCos(rad, sc);
	s = sc[0];
	c = sc[1];
	MTX_unit(m);
	unit_axis.qv = V4_normalize(axis);
	axis2.qv = V4_mul(unit_axis.qv, unit_axis.qv);
	t = 1.0f - c;
	x = unit_axis.x;
	y = unit_axis.y;
	z = unit_axis.z;
	xy = x*y;
	xz = x*z;
	yz = y*z;

	m[0][0] = t*axis2.x + c;
	m[0][1] = t*xy + s*z;
	m[0][2] = t*xz - s*y;

	m[1][0] = t*xy - s*z;
	m[1][1] = t*axis2.y + c;
	m[1][2] = t*yz + s*x;

	m[2][0] = t*xz + s*y;
	m[2][1] = t*yz - s*x;
	m[2][2] = t*axis2.z + c;
}

void MTX_make_view(MTX m, QVEC pos, QVEC tgt, QVEC upvec) {
	QVEC dir;
	QVEC right;
	QVEC up;
	dir = V4_normalize(V4_sub(tgt, pos));
	right = V4_normalize(V4_cross(upvec, dir));
	up = V4_cross(right, dir);
	V4_store(m[0], V4_sub(V4_zero(), right));
	V4_store(m[1], V4_sub(V4_zero(), up));
	V4_store(m[2], V4_sub(V4_zero(), dir));
	MTX_transpose_sr(m, m);
	V4_store(m[3], V4_sub(V4_zero(), pos));
	V4_store(m[3], V4_set_w1(MTX_calc_qvec(m, V4_load(m[3]))));
}

void MTX_make_proj(MTX m, float fovy, float aspect, float znear, float zfar) {
	float sc[2];
	float cot, q;
	SinCos(fovy*0.5f, sc);
	MTX_clear(m);
	cot = sc[1] / sc[0];
	m[2][3] = -1.0f;
	m[0][0] = cot / aspect;
	m[1][1] = cot;
	q = zfar / (zfar - znear);
	m[2][2] = -q;
	m[3][2] = -q * znear;
}

void MTX_calc_vec(VEC vdst, MTX m, VEC vsrc) {
#if D_KISS
	float x = vsrc[0];
	float y = vsrc[1];
	float z = vsrc[2];
	vdst[0] = x*m[0][0] + y*m[1][0] + z*m[2][0];
	vdst[1] = x*m[0][1] + y*m[1][1] + z*m[2][1];
	vdst[2] = x*m[0][2] + y*m[1][2] + z*m[2][2];
#else
	UVEC res;
	QVEC qx = _mm_set_ps1(vsrc[0]);
	QVEC qy = _mm_set_ps1(vsrc[1]);
	QVEC qz = _mm_set_ps1(vsrc[2]);
	res.qv = _mm_add_ps(_mm_add_ps(_mm_mul_ps(qx, D_VEC128(m[0])), _mm_mul_ps(qy, D_VEC128(m[1]))), _mm_mul_ps(qz, D_VEC128(m[2])));
	VEC_cpy(vdst, res.v);
#endif
}

void MTX_calc_pnt(VEC vdst, MTX m, VEC vsrc) {
#if D_KISS
	float x = vsrc[0];
	float y = vsrc[1];
	float z = vsrc[2];
	vdst[0] = x*m[0][0] + y*m[1][0] + z*m[2][0] + m[3][0];
	vdst[1] = x*m[0][1] + y*m[1][1] + z*m[2][1] + m[3][1];
	vdst[2] = x*m[0][2] + y*m[1][2] + z*m[2][2] + m[3][2];
#else
	UVEC res;
	QVEC qx = _mm_set_ps1(vsrc[0]);
	QVEC qy = _mm_set_ps1(vsrc[1]);
	QVEC qz = _mm_set_ps1(vsrc[2]);
	res.qv = _mm_add_ps(_mm_add_ps(_mm_mul_ps(qx, D_VEC128(m[0])), _mm_mul_ps(qy, D_VEC128(m[1]))),
	         _mm_add_ps(_mm_mul_ps(qz, D_VEC128(m[2])), D_VEC128(m[3])));
	VEC_cpy(vdst, res.v);
#endif
}

QVEC MTX_calc_qvec(MTX m, QVEC v) {
#if D_KISS
	UVEC v0;
	float x = V4_at(v, 0);
	float y = V4_at(v, 1);
	float z = V4_at(v, 2);
	v0.x = x*m[0][0] + y*m[1][0] + z*m[2][0];
	v0.y = x*m[0][1] + y*m[1][1] + z*m[2][1];
	v0.z = x*m[0][2] + y*m[1][2] + z*m[2][2];
	v0.w = x*m[0][3] + y*m[1][3] + z*m[2][3];
	return v0.qv;
#else
	QVEC qx = _mm_shuffle_ps(v, v, 0x00);
	QVEC qy = _mm_shuffle_ps(v, v, 0x55);
	QVEC qz = _mm_shuffle_ps(v, v, 0xAA);
	return _mm_add_ps(_mm_add_ps(_mm_mul_ps(qx, D_VEC128(m[0])), _mm_mul_ps(qy, D_VEC128(m[1]))), _mm_mul_ps(qz, D_VEC128(m[2])));
#endif
}

QVEC MTX_calc_qpnt(MTX m, QVEC v) {
#if D_KISS
	UVEC v0;
	float x = V4_at(v, 0);
	float y = V4_at(v, 1);
	float z = V4_at(v, 2);
	v0.x = x*m[0][0] + y*m[1][0] + z*m[2][0] + m[3][0];
	v0.y = x*m[0][1] + y*m[1][1] + z*m[2][1] + m[3][1];
	v0.z = x*m[0][2] + y*m[1][2] + z*m[2][2] + m[3][2];
	v0.w = x*m[0][3] + y*m[1][3] + z*m[2][3] + m[3][3];
	return v0.qv;
#else
	QVEC qx = _mm_shuffle_ps(v, v, 0x00);
	QVEC qy = _mm_shuffle_ps(v, v, 0x55);
	QVEC qz = _mm_shuffle_ps(v, v, 0xAA);
	return _mm_add_ps(_mm_add_ps(_mm_mul_ps(qx, D_VEC128(m[0])), _mm_mul_ps(qy, D_VEC128(m[1]))),
	       _mm_add_ps(_mm_mul_ps(qz, D_VEC128(m[2])), D_VEC128(m[3])));
#endif
}

QVEC MTX_apply(MTX m, QVEC v) {
#if D_KISS
	UVEC v0;
	float x = V4_at(v, 0);
	float y = V4_at(v, 1);
	float z = V4_at(v, 2);
	float w = V4_at(v, 3);
	v0.x = x*m[0][0] + y*m[1][0] + z*m[2][0] + w*m[3][0];
	v0.y = x*m[0][1] + y*m[1][1] + z*m[2][1] + w*m[3][1];
	v0.z = x*m[0][2] + y*m[1][2] + z*m[2][2] + w*m[3][2];
	v0.w = x*m[0][3] + y*m[1][3] + z*m[2][3] + w*m[3][3];
	return v0.qv;
#else
	__m128i itmp = D_M128I(v);
	__m128i ixxxx = _mm_shuffle_epi32(itmp, 0);
	__m128i iyyyy = _mm_shuffle_epi32(itmp, 0x55);
	__m128i izzzz = _mm_shuffle_epi32(itmp, 0xAA);
	__m128i iwwww = _mm_shuffle_epi32(itmp, 0xFF);
	return _mm_add_ps(
		_mm_add_ps( _mm_mul_ps(D_M128(ixxxx), D_VEC128(m[0])), _mm_mul_ps(D_M128(iyyyy), D_VEC128(m[1])) ),
		_mm_add_ps( _mm_mul_ps(D_M128(izzzz), D_VEC128(m[2])), _mm_mul_ps(D_M128(iwwww), D_VEC128(m[3])) )
	);
#endif
}


QVEC QUAT_from_axis_angle(QVEC axis, float ang) {
	float sc[2];
	SinCos(ang*0.5f, sc);
	return V4_mul(V4_set_w1(V4_normalize(axis)), V4_set(sc[0], sc[0], sc[0], sc[1]));
}

QVEC QUAT_from_mtx(MTX m) {
	float s, x, y, z, w;
	float trace = m[0][0] + m[1][1] + m[2][2];
	if (trace > 0.0f) {
		s = sqrtf(trace + 1.0f);
		w = s * 0.5f;
		s = 0.5f / s;
		x = (m[1][2] - m[2][1]) * s;
		y = (m[2][0] - m[0][2]) * s;
		z = (m[0][1] - m[1][0]) * s;
	} else {
		if (m[1][1] > m[0][0]) {
			if (m[2][2] > m[1][1]) {
				s = m[2][2] - m[1][1] - m[0][0];
				s = sqrtf(s + 1.0f);
				z = s * 0.5f;
				if (s != 0.0f) {
					s = 0.5f / s;
				}
				w = (m[0][1] - m[1][0]) * s;
				x = (m[2][0] + m[0][2]) * s;
				y = (m[2][1] + m[1][2]) * s;
			} else {
				s = m[1][1] - m[2][2] - m[0][0];
				s = sqrtf(s + 1.0f);
				y = s * 0.5f;
				if (s != 0.0f) {
					s = 0.5f / s;
				}
				w = (m[2][0] - m[0][2]) * s;
				z = (m[1][2] + m[2][1]) * s;
				x = (m[1][0] + m[0][1]) * s;
			}
		} else if (m[2][2] > m[0][0]) {
			s = m[2][2] - m[1][1] - m[0][0];
			s = sqrtf(s + 1.0f);
			z = s * 0.5f;
			if (s != 0.0f) {
				s = 0.5f / s;
			}
			w = (m[0][1] - m[1][0]) * s;
			x = (m[2][0] + m[0][2]) * s;
			y = (m[2][1] + m[1][2]) * s;
		} else {
			s = m[0][0] - m[1][1] - m[2][2];
			s = sqrtf(s + 1.0f);
			x = s * 0.5f;
			if (s != 0.0f) {
				s = 0.5f / s;
			}
			w = (m[1][2] - m[2][1]) * s;
			y = (m[0][1] + m[1][0]) * s;
			z = (m[0][2] + m[2][0]) * s;
		}
	}
	return V4_set(x, y, z, w);
}

QVEC QUAT_get_vec_x(QVEC q) {
	float x = V4_at(q, 0);
	float y = V4_at(q, 1);
	float z = V4_at(q, 2);
	float w = V4_at(q, 3);
	return V4_set(1.0f - (2.0f*y*y) - (2.0f*z*z), (2.0f*x*y) + (2.0f*w*z), (2.0f*x*z) - (2.0f*w*y), 0.0f);
}

QVEC QUAT_get_vec_y(QVEC q) {
	float x = V4_at(q, 0);
	float y = V4_at(q, 1);
	float z = V4_at(q, 2);
	float w = V4_at(q, 3);
	return V4_set((2.0f*x*y) - (2.0f*w*z), 1.0f - (2.0f*x*x) - (2.0f*z*z), (2.0f*y*z) + (2.0f*w*x), 0.0f);
}

QVEC QUAT_get_vec_z(QVEC q) {
	float x = V4_at(q, 0);
	float y = V4_at(q, 1);
	float z = V4_at(q, 2);
	float w = V4_at(q, 3);
	return V4_set((2.0f*x*z) + (2.0f*w*y), (2.0f*y*z) - (2.0f*w*x), 1.0f - (2.0f*x*x) - (2.0f*y*y), 0.0f);
}

void QUAT_get_mtx(QVEC q, MTX m) {
	V4_store(m[0], QUAT_get_vec_x(q));
	V4_store(m[1], QUAT_get_vec_y(q));
	V4_store(m[2], QUAT_get_vec_z(q));
	V4_store(m[3], V4_load(g_identity[3]));
}

QVEC QUAT_normalize(QVEC q) {
	return V4_scale(q, 1.0f / sqrtf(V4_dot4(q, q)));
}

QVEC QUAT_mul(QVEC a, QVEC b) {
#if D_KISS
	UVEC q;
	UVEC qa;
	UVEC qb;
	qa.qv = a;
	qb.qv = b;
	q.f[0] = qa.w*qb.x + qa.x*qb.w + qa.y*qb.z - qa.z*qb.y;
	q.f[1] = qa.w*qb.y + qa.y*qb.w + qa.z*qb.x - qa.x*qb.z;
	q.f[2] = qa.w*qb.z + qa.z*qb.w + qa.x*qb.y - qa.y*qb.x;
	q.f[3] = qa.w*qb.w - qa.x*qb.x - qa.y*qb.y - qa.z*qb.z;
	return q.qv;
#else
	QVEC t1, t2, t3;
	t1 = _mm_sub_ps(
		_mm_mul_ps(
			_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)),
			_mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2))),
		_mm_mul_ps(
			_mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 1, 0, 2)),
			_mm_shuffle_ps(b, b, _MM_SHUFFLE(0, 0, 2, 1)))
	);
	t2 = _mm_add_ps(
		_mm_mul_ps(
			_mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 2, 1, 0)),
			_mm_shuffle_ps(b, b, _MM_SHUFFLE(1, 3, 3, 3))),
		_mm_mul_ps(
			_mm_shuffle_ps(b, b, _MM_SHUFFLE(2, 2, 1, 0)),
			_mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 3, 3)))
	);
	t3 = _mm_unpackhi_ps(_mm_setzero_ps(), t2);
	t3 = _mm_shuffle_ps(t3, t3, _MM_SHUFFLE(3, 2, 2, 2));
	t2 = _mm_sub_ps(_mm_sub_ps(t2, t3), t3);
	return _mm_add_ps(t1, t2);
#endif
}

QVEC QUAT_apply(QVEC q, QVEC v) {
	float w = V4_at(q, 3);
	float ww = D_SQ(w);
	float d = V4_dot4(q, V4_set_w0(v));
	QVEC tv = V4_sub(
		V4_add(V4_scale(v, ww), V4_scale(q, d)),
		V4_scale(V4_sub(
			V4_mul(D_V4_SHUFFLE(v, 1, 2, 0, 0), D_V4_SHUFFLE(q, 2, 0, 1, 0)),
			V4_mul(D_V4_SHUFFLE(v, 2, 0, 1, 0), D_V4_SHUFFLE(q, 1, 2, 0, 0))), w));
	tv = V4_sub(V4_scale(tv, 2.0f), v);
	return V4_set_w(tv, V4_at(v, 3));
}

QVEC QUAT_lerp(QVEC a, QVEC b, float bias) {
	return QUAT_normalize(V4_combine(a, (1.0f - bias), b, bias));
}

QVEC QUAT_slerp(QVEC a, QVEC b, float bias) {
	float theta, s, oos, af, bf, c;

	bf = 1.0f;
	c = V4_dot4(a, b);
	if (c < 0.0f) {
		c = -c;
		bf = -bf;
	}
	if (c <= (1.0f - 1e-5f)) {
		theta = acosf(c);
		s = sinf(theta);
		oos = 1.0f / s;
		af = sinf((1.0f-bias)*theta) * oos;
		bf *= sinf(bias*theta) * oos;
	} else {
		af = 1.0f - bias;
		bf *= bias;
	}
	return V4_combine(a, af, b, bf);
}


sys_ui32 CLR_f2i(QVEC cv) {
	UVEC tv;
	tv.qv = V4_scale(V4_max(V4_min(cv, V4_fill(1.0f)), V4_zero()), 255.0f);
	return (sys_ui32)(((((int)tv.a) & 0xFF)<<24) | ((((int)tv.r) & 0xFF)<<16) | ((((int)tv.g) & 0xFF)<<8) | (((int)tv.b) & 0xFF));
}

QVEC CLR_i2f(sys_ui32 ci) {
	UVEC cv;
	cv.r = (float)((ci>>16) & 0xFF);
	cv.g = (float)((ci>>8) & 0xFF);
	cv.b = (float)(ci & 0xFF);
	cv.a = (float)((ci>>24) & 0xFF);
	return V4_scale(cv.qv, 1.0f/255);
}

QVEC CLR_RGB_to_HSV(QVEC qrgb) {
	UVEC rgb;
	float h, s, v, cmin, cmax, diff;

	rgb.qv = qrgb;
	cmin = F_min(F_min(rgb.r, rgb.g), rgb.b);
	cmax = F_max(F_max(rgb.r, rgb.g), rgb.b);
	diff = cmax - cmin;
	h = 0.0f;
	s = cmax > 0.0f ? diff / cmax : 0.0f;
	v = cmax;
	if (diff > 0.0f) {
#if D_KISS
		if (cmax == rgb.r) {
			h = (rgb.g - rgb.b) / diff;
		} else if (cmax == rgb.g) {
			h = ((rgb.b - rgb.r) / diff) + 2.0f;
		} else {
			h = ((rgb.r - rgb.g) / diff) + 4.0f;
		}
#else
		UVEC hv;
		int msk;
		static QVEC hv_add = {0.0f, 2.0f, 4.0f, 0.0f};
		hv.qv = V4_add(V4_scale(V4_sub(D_V4_SHUFFLE(qrgb, 1, 2, 0, 0), D_V4_SHUFFLE(qrgb, 2, 0, 1, 0)), 1.0f/diff), hv_add);
		msk = V4_eq(qrgb, V4_fill(cmax)) & 7;
		h = hv.f[(0x1210>>(msk<<1)) & 3];
#endif
	}
	h /= 6.0f;
	if (h < 0.0f) {
		h += 1.0f;
	}
	return V4_set(h, s, v, 0.0f);
}

QVEC CLR_HSV_to_RGB(QVEC qhsv) {
	UVEC hsv;
	QVEC tv;
	int ih;
	float fh;
	float h, s, v;

	hsv.qv = qhsv;
	h = hsv.x;
	s = hsv.y;
	v = hsv.z;
	if (s) {
		if (h >= 1.0f) {
			h = h - (int)h;
		}
		h *= 6.0f;
		ih = (int)h;
		fh = h - ih;
		tv = V4_mul(V4_fill(v), V4_sub(V4_fill(1.0f), V4_set(s, s*fh, s*(1.0f-fh), 0.0f)));
		switch (ih) {
			case 0:
				tv = D_V4_SHUFFLE(tv, 3, 2, 0, 3);
				break;
			case 1:
				tv = D_V4_SHUFFLE(tv, 1, 3, 0, 3);
				break;
			case 2:
				tv = D_V4_SHUFFLE(tv, 0, 3, 2, 3);
				break;
			case 3:
				tv = D_V4_SHUFFLE(tv, 0, 1, 3, 3);
				break;
			case 4:
				tv = D_V4_SHUFFLE(tv, 2, 0, 3, 3);
				break;
			case 5:
				tv = D_V4_SHUFFLE(tv, 3, 0, 1, 3);
				break;
			default:
				tv = V4_zero();
				break;
		}
	} else {
		tv = V4_fill(v);
	}
	return V4_set_w1(tv);
}

QVEC CLR_RGB_to_YCbCr(QVEC qrgb) {
	static QVEC vb = {-0.169f, -0.331f, 0.5f, 0.0f};
	static QVEC vr = {0.5f, -0.419f, -0.081f, 0.0f};
	return V4_set_vec(CLR_get_luma(qrgb), V4_dot4(qrgb, vb), V4_dot4(qrgb, vr));
}

QVEC CLR_YCbCr_to_RGB(QVEC qybr) {
	UVEC c;
	float Y, Cb, Cr, r, g, b;
	c.qv = qybr;
	Y = c.x;
	Cb = c.y;
	Cr = c.z;
	r = Y + Cr*1.402f;
	g = Y - Cr*0.714f - Cb*0.344f;
	b = Y + Cb*1.772f;
	return V4_set_pnt(r, g, b);
}

QVEC CLR_RGB_to_XYZ(QVEC qrgb, MTX* pMtx) {
	static QMTX mtx709 = {
		{0.412453f, 0.212671f, 0.019334f, 0.0f},
		{0.357580f, 0.715160f, 0.119193f, 0.0f},
		{0.180423f, 0.072169f, 0.950227f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f}
	};
	if (!pMtx) pMtx = &mtx709;
	return MTX_calc_qvec(*pMtx, qrgb);
}

QVEC CLR_XYZ_to_RGB(QVEC qxyz, MTX* pMtx) {
	static QMTX mtx709 = {
		{ 3.240479f, -0.969256f,  0.055648f, 0.0f},
		{-1.537150f,  1.875992f, -0.204043f, 0.0f},
		{-0.498535f,  0.041556f,  1.057311f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f}
	};
	if (!pMtx) pMtx = &mtx709;
	return MTX_calc_qvec(*pMtx, qxyz);
}

static float XYZ2Lab_sub(float x) {
	return x <= 0.008856f ? 7.787f*x + (16.0f/116) : powf(x, 1.0f/3);
}

QVEC CLR_XYZ_to_Lab(QVEC qxyz, MTX* pMtx) {
	UVEC xyz;
	float lx, ly, lz;
	QVEC white = V4_set_w1(CLR_RGB_to_XYZ(V4_fill(1.0f), pMtx));
	xyz.qv = V4_mul(qxyz, V4_inv(white));
	lx = XYZ2Lab_sub(xyz.x);
	ly = XYZ2Lab_sub(xyz.y);
	lz = XYZ2Lab_sub(xyz.z);
	return V4_set_vec(116.0f*ly - 16.0f, 500.0f*(lx-ly), 200.0f*(ly-lz));
}

static float Lab2XYZ_sub(float x) {
	return x <= 0.206893f ? (x - (16.0f/116))/7.787f: D_CB(x);
}

QVEC CLR_Lab_to_XYZ(QVEC qlab, MTX* pMtx) {
	UVEC Lab;
	QVEC white;
	float L, a, b, ly;

	Lab.qv = qlab;
	white = CLR_RGB_to_XYZ(V4_fill(1.0f), pMtx);
	L = Lab.x;
	a = Lab.y;
	b = Lab.z;
	ly = (L + 16.0f) / 116.0f;
	return V4_mul(V4_set_vec(Lab2XYZ_sub(a/500.0f + ly), Lab2XYZ_sub(ly), Lab2XYZ_sub(-b/200.0f + ly)), white);
}

QVEC CLR_RGB_to_Lab(QVEC qrgb, MTX* pMtx) {
	return CLR_XYZ_to_Lab(CLR_RGB_to_XYZ(qrgb, pMtx), pMtx);
}

QVEC CLR_Lab_to_RGB(QVEC qlab, MTX* pRGB2XYZ, MTX* pXYZ2RGB) {
	return CLR_XYZ_to_RGB(CLR_Lab_to_XYZ(qlab, pRGB2XYZ), pXYZ2RGB);
}

float CLR_get_luma(QVEC qrgb) {
	static QVEC luma601 = {0.299f, 0.587f, 0.114f, 0.0f};
	return V4_dot4(qrgb, luma601);
}

float CLR_get_luminance(QVEC qrgb, MTX* pMtx) {
	static QVEC Y709 = {0.212671f, 0.71516f, 0.072169f, 0.0f};
	QVEC Yvec;
	if (pMtx) {
		Yvec = V4_set_vec((*pMtx)[0][1], (*pMtx)[1][1], (*pMtx)[2][1]);
	} else {
		Yvec = Y709;
	}
	return V4_dot4(qrgb, Yvec);
}

void CLR_calc_XYZ_transform(MTX* pRGB2XYZ, MTX* pXYZ2RGB, QVEC* pPrim, QVEC* pWhite) {
	QMTX im;
	QMTX tm;
	QMTX cm;
	QVEC wvec;
	QVEC jvec;
	static QVEC prim709[3] = {
		{0.640f, 0.330f, 0.030f, 0.0f}, /* Rxyz */
		{0.300f, 0.600f, 0.100f, 0.0f}, /* Gxyz */
		{0.150f, 0.060f, 0.790f, 0.0f}  /* Bxyz */
	};
	static QVEC D65 = {0.3127f, 0.3290f, 0.3582f, 0.0f};

	if (!pPrim) pPrim = prim709;
	if (!pWhite) pWhite = &D65;
	wvec = V4_scale(*pWhite, 1.0f/V4_at(*pWhite, 1));
	V4_store(cm[0], pPrim[0]);
	V4_store(cm[1], pPrim[1]);
	V4_store(cm[2], pPrim[2]);
	V4_store(cm[3], V4_load(g_identity[3]));
	MTX_invert(im, cm);
	jvec = MTX_calc_qvec(im, wvec);
	MTX_unit(tm);
	tm[0][0] = V4_at(jvec, 0);
	tm[1][1] = V4_at(jvec, 1);
	tm[2][2] = V4_at(jvec, 2);
	MTX_mul(tm, tm, cm);
	if (pRGB2XYZ) {
		MTX_cpy(*pRGB2XYZ, tm);
	}
	if (pXYZ2RGB) {
		MTX_invert(*pXYZ2RGB, tm);
	}
}


#define D_SHC1 0.282095f
#define D_SHC2 0.488603f
#define D_SHC3 1.095255f
#define D_SHC4 0.315392f
#define D_SHC5 0.546274f

#define D_SH_DIR_NFACTOR 2.956793f

#define D_SHCC0 0.282095f
#define D_SHCC1 0.325735f
#define D_SHCC2 0.273137f
#define D_SHCC3 0.078848f
#define D_SHCC4 0.136569f

void SH_clear_ch(SH_CHANNEL* pChan) {
	pChan->v[0].qv = V4_zero();
	pChan->v[1].qv = V4_zero();
	pChan->f = 0.0f;
}

void SH_clear(SH_COEF* pCoef) {
	SH_clear_ch(&pCoef->r);
	SH_clear_ch(&pCoef->g);
	SH_clear_ch(&pCoef->b);
}

void SH_add_ch(SH_CHANNEL* pCh0, SH_CHANNEL* pCh1, SH_CHANNEL* pCh2) {
	pCh0->v[0].qv = V4_add(pCh1->v[0].qv, pCh2->v[0].qv);
	pCh0->v[1].qv = V4_add(pCh1->v[1].qv, pCh2->v[1].qv);
	pCh0->f = pCh1->f + pCh2->f;
}

void SH_add(SH_COEF* pCoef0, SH_COEF* pCoef1, SH_COEF* pCoef2) {
	SH_add_ch(&pCoef0->r, &pCoef1->r, &pCoef2->r);
	SH_add_ch(&pCoef0->g, &pCoef1->g, &pCoef2->g);
	SH_add_ch(&pCoef0->b, &pCoef1->b, &pCoef2->b);
}

void SH_scale_ch(SH_CHANNEL* pCh0, SH_CHANNEL* pCh1, float s) {
	pCh0->v[0].qv = V4_scale(pCh1->v[0].qv, s);
	pCh0->v[1].qv = V4_scale(pCh1->v[1].qv, s);
	pCh0->f = pCh1->f * s;
}

void SH_scale(SH_COEF* pCoef0, SH_COEF* pCoef1, float s) {
	SH_scale_ch(&pCoef0->r, &pCoef1->r, s);
	SH_scale_ch(&pCoef0->g, &pCoef1->g, s);
	SH_scale_ch(&pCoef0->b, &pCoef1->b, s);
}

void SH_calc_dir_ch(SH_CHANNEL* pCh, float c, UVEC* pDir) {
	float cc, dx, dy, dz;
	QVEC cvec;
	static QVEC c1 = {D_SHC1, -D_SHC2, D_SHC2, -D_SHC2};
	static QVEC c2 = {D_SHC3, -D_SHC3, D_SHC4, -D_SHC3};
	dx = pDir->x;
	dy = pDir->y;
	dz = pDir->z;
	cc = c * D_SH_DIR_NFACTOR;
	cvec = V4_fill(cc);
	pCh->v[0].qv = V4_mul(V4_mul(V4_set(1.0f, dy, dz, dx), cvec), c1);
	pCh->v[1].qv = V4_mul(V4_mul(V4_set(dx*dy, dy*dz, 3.0f*D_SQ(dz) - 1.0f, dx*dz), cvec), c2);
	pCh->f = (D_SQ(dx) - D_SQ(dy)) * D_SHC5 * cc;
}

void SH_calc_dir(SH_COEF* pCoef, UVEC* pColor, UVEC* pDir) {
	UVEC dir;
	dir.qv = V4_sub(V4_zero(), pDir->qv);
	SH_calc_dir_ch(&pCoef->r, pColor->r, &dir);
	SH_calc_dir_ch(&pCoef->g, pColor->g, &dir);
	SH_calc_dir_ch(&pCoef->b, pColor->b, &dir);
}

void SH_calc_param_ch(SH_PARAM* pParam, SH_CHANNEL* pChan, int ch_idx) {
	QVEC v;
	float* pCh = (float*)pChan;
	static QVEC mul_a = {-D_SHCC1, -D_SHCC1, D_SHCC1, D_SHCC0};
	static QVEC mul_b = {D_SHCC2, -D_SHCC2, 3.0f*D_SHCC3, -D_SHCC2};
	v = V4_set(pCh[D_SH_IDX(1, 1)], pCh[D_SH_IDX(1, -1)], pCh[D_SH_IDX(1, 0)], pCh[D_SH_IDX(0, 0)]);
	pParam->sh[ch_idx].qv = V4_mul(v, mul_a);
	pParam->sh[ch_idx].w -= D_SHCC3 * pCh[D_SH_IDX(2, 0)];
	v = V4_set(pCh[D_SH_IDX(2, -2)], pCh[D_SH_IDX(2, -1)], pCh[D_SH_IDX(2, 0)], pCh[D_SH_IDX(2, 1)]);
	pParam->sh[ch_idx+3].qv = V4_mul(v, mul_b);
	pParam->sh[6].f[ch_idx] = D_SHCC4 * pCh[D_SH_IDX(2, 2)];
}

void SH_calc_param(SH_PARAM* pParam, SH_COEF* pCoef) {
	SH_calc_param_ch(pParam, &pCoef->r, 0);
	SH_calc_param_ch(pParam, &pCoef->g, 1);
	SH_calc_param_ch(pParam, &pCoef->b, 2);
	pParam->sh[6].qv = V4_set_w1(pParam->sh[6].qv);
}


float SPL_hermite(float p0, float m0, float p1, float m1, float t) {
	float tt = t*t;
	float ttt = tt*t;
	float tt3 = tt*3.0f;
	float tt2 = tt + tt;
	float ttt2 = tt2 * t;
	float h00 = ttt2 - tt3 + 1.0f;
	float h10 = ttt - tt2 + t;
	float h01 = -ttt2 + tt3;
	float h11 = ttt - tt;
	return (h00*p0 + h10*m0 + h01*p1 + h11*m1);
}

float SPL_overhauser(QVEC pvec, float t) {
	float tt = t*t;
	float ttt = tt*t;
	static QMTX mtx = {
		{-1.0f/2.0f, 3.0f/2.0f, -3.0f/2.0f, 1.0f/2.0f},
		{1.0f, -5.0f/2.0f, 2.0f, -1.0f/2.0f},
		{-1.0f/2.0f, 0.0f, 1.0f/2.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f}
	};
	QVEC tvec = MTX_calc_qpnt(mtx, V4_set(ttt, tt, t, 1.0f));
	return V4_dot4(pvec, tvec);
}

QVEC _GEOM_get_plane_static(QVEC pos, QVEC nrm) {
	return GEOM_get_plane(pos, nrm);
}

D_FORCE_INLINE QVEC GEOM_get_plane(QVEC pos, QVEC nrm) {
	QVEC n = V4_set_w0(nrm);
	float d = V4_dot4(pos, nrm);
	return V4_set(V4_at(n, 0), V4_at(n, 1), V4_at(n, 2), d);
}

int GEOM_aabb_overlap(GEOM_AABB* pBox0, GEOM_AABB* pBox1) {
#if D_KISS
	if (pBox0->min.x > pBox1->max.x || pBox0->max.x < pBox1->min.x) return 0;
	if (pBox0->min.y > pBox1->max.y || pBox0->max.y < pBox1->min.y) return 0;
	if (pBox0->min.z > pBox1->max.z || pBox0->max.z < pBox1->min.z) return 0;
	return 1;
#else
	int m = _mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(pBox0->min.qv, pBox1->max.qv), _mm_cmpnlt_ps(pBox0->max.qv, pBox1->min.qv)));
	return ((m & 7) == 7);
#endif
}

int GEOM_pnt_inside_aabb(QVEC pos, GEOM_AABB* pBox) {
#if D_KISS
	UVEC tp;
	tp.qv = pos;
	if (tp.x > pBox->max.x || tp.x < pBox->min.x) return 0;
	if (tp.y > pBox->max.y || tp.y < pBox->min.y) return 0;
	if (tp.z > pBox->max.z || tp.z < pBox->min.z) return 0;
	return 1;
#else
	int m = _mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(pos, pBox->max.qv), _mm_cmpnlt_ps(pos, pBox->min.qv)));
	return ((m & 7) == 7);
#endif
}

int GEOM_seg_quad_intersect(QVEC p0, QVEC p1, QVEC* pVtx, QVEC* pHit_pos, QVEC* pHit_nml) {
	QVEC n;
	QVEC dir;
	QVEC edge[4];
	QVEC vec[4];
	float d, d0, d1, t;

	edge[0] = V4_sub(pVtx[1], pVtx[0]);
	n = V4_normalize(V4_cross(edge[0], V4_sub(pVtx[2], pVtx[0])));
	vec[0] = V4_sub(p0, pVtx[0]);
	d0 = V4_dot(vec[0], n);
	d1 = V4_dot(V4_sub(p1, pVtx[0]), n);
	if (d0*d1 > 0.0f || (d0 == 0.0f && d1 == 0.0f)) return 0;
	edge[1] = V4_sub(pVtx[2], pVtx[1]);
	edge[2] = V4_sub(pVtx[3], pVtx[2]);
	edge[3] = V4_sub(pVtx[0], pVtx[3]);
	vec[1] = V4_sub(p0, pVtx[1]);
	vec[2] = V4_sub(p0, pVtx[2]);
	vec[3] = V4_sub(p0, pVtx[3]);
	dir = V4_sub(p1, p0);
	if (V4_triple(edge[0], dir, vec[0]) < 0.0f) return 0;
	if (V4_triple(edge[1], dir, vec[1]) < 0.0f) return 0;
	if (V4_triple(edge[2], dir, vec[2]) < 0.0f) return 0;
	if (V4_triple(edge[3], dir, vec[3]) < 0.0f) return 0;
	d = V4_dot(dir, n);
	if (d == 0.0f || d0 == 0.0f) {
		t = 0.0f;
	} else {
		t = -d0 / d;
	}
	if (t > 1.0f || t < 0.0f) return 0;
	if (pHit_pos) {
		*pHit_pos = V4_set_w1(V4_add(p0, V4_scale(dir, t)));
	}
	if (pHit_nml) {
		*pHit_nml = n; 
	}
	return 1;
}

int GEOM_seg_polyhedron_intersect(QVEC p0, QVEC p1, GEOM_PLANE* pPln, int n, QVEC* pRes) {
	QVEC d;
	int i, res = 0;
	float first = 0.0f;
	float last = 1.0f;

	d = V4_set_w0(V4_sub(p1, p0));
	for (i = 0; i < n; ++i) {
		float dnm = V4_dot4(pPln->qv, d);
		float dist = pPln->d - V4_dot(pPln->qv, p0);
		if (dnm == 0.0f) {
			if (dist > 0.0f) goto _exit;
		} else {
			float t = dist / dnm;
			if (dnm < 0.0f) {
				if (t > first) first = t;
			} else {
				if (t < last) last = t;
			}
			if (first > last) goto _exit;
		}
		++pPln;
	}
	res = 1;
_exit:
	if (pRes) {
		*pRes = V4_set(first, last, 0.0f, 0.0f);
	}
	return res;
}

int GEOM_barycentric(QVEC pos, QVEC* pVtx, QVEC* pCoord) {
	QVEC dv0;
	QVEC dv1;
	QVEC dv2;
	float u, v, w;
	float d, ood;
	float d01, d02, d11, d12, d22;
	int res;

	dv0 = V4_set_w0(V4_sub(pos, pVtx[0]));
	dv1 = V4_set_w0(V4_sub(pVtx[1], pVtx[0]));
	dv2 = V4_set_w0(V4_sub(pVtx[2], pVtx[0]));
	d01 = V4_dot4(dv0, dv1);
	d02 = V4_dot4(dv0, dv2);
	d12 = V4_dot4(dv1, dv2);
	d11 = V4_dot4(dv1, dv1);
	d22 = V4_dot4(dv2, dv2);
	d = d11*d22 - D_SQ(d12);
	ood = 1.0f / d;
	v = (d01*d22 - d02*d12) * ood;
	w = (d02*d11 - d01*d12) * ood;
	u = (1.0f - v - w);
	res = 0;
	if (u < 0.0f) res += 4;
	if (v < 0.0f) res += 2;
	if (w < 0.0f) res += 1;
	if (pCoord) {
		*pCoord = V4_set(u, v, w, 0.0f);
	}
	return res;
}
