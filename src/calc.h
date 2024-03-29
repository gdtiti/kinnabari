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

#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>
#include <memory.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <intrin.h>
#if defined(__INTEL_COMPILER)
#	include <smmintrin.h>
#endif

//#define D_KISS 1

#ifndef D_KISS
#	define D_KISS 0
#endif

#ifdef M_PI
#	define D_PI ((float)M_PI)
#else
#	define D_PI 3.141592f
#endif

#ifdef FLT_MAX
#	define D_MAX_FLOAT FLT_MAX
#else
#	define D_MAX_FLOAT ((float)(3.402823466e38))
#endif

#ifdef FLT_MIN
#	define D_MIN_FLOAT FLT_MIN
#else
#	define D_MIN_FLOAT ((float)(1.175494351e-38))
#endif

#define D_SQRT2 (1.4142135623730950488016887242097f)
#define D_SQRT3 (1.7320508075688772935274463415059f)

#define D_SQ(_x) ((_x) * (_x))
#define D_CB(_x) ((_x) * (_x) * (_x))
#define D_CLAMP(_x, _min, _max) ( (_x) < (_min) ? (_min) : (_x) > (_max) ? (_max) : (_x) )
#define D_MIN(_x, _y) ( (_x) < (_y) ? (_x) : (_y) )
#define D_MIN3(_x, _y, _z) D_MIN((_x), D_MIN((_y), (_z)))
#define D_MIN4(_x, _y, _z, _w) D_MIN((_x), D_MIN3((_y), (_z), (_w)))
#define D_MAX(_x, _y) ( (_x) > (_y) ? (_x) : (_y) )
#define D_MAX3(_x, _y, _z) D_MAX((_x), D_MAX((_y), (_z)))
#define D_MAX4(_x, _y, _z, _w) D_MAX((_x), D_MAX3((_y), (_z), (_w)))

#define D_CLAMP_F(_x, _min, _max) F_max(F_min(_x, _max), _min)

#define D_DEG2RAD(_deg) ( (_deg) * (D_PI / 180.0f) )
#define D_RAD2DEG(_rad) ( (_rad) * (180.0f / D_PI) )

#define D_RAD2INT(_rad, _nbits) ( (int)((_rad) * ((1<<(_nbits)) / (D_PI*2.0f))) & ((1<<(_nbits))-1) )
#define D_INT2RAD(_val, _nbits) ( ((_val) & ((1<<(_nbits))-1)) * ((D_PI*2.0f) / ((float)(1<<(_nbits)))) )

#define D_SH_IDX(l, m) ((l)*((l)+1) + (m))

#define D_VEC128(_v) _mm_load_ps(_v)

#if defined(__INTEL_COMPILER) || defined(__GNUC__) || (defined(_MSC_VER) && (_MSC_VER >= 1500))
#	define D_M128I(_m) _mm_castps_si128(_m)
#	define D_M128(_mi) _mm_castsi128_ps(_mi)
#	define D_VEC_128I(_v) D_M128I(D_VEC128(_v))
#else
/* No cast* intrinsics in VC2005 - use type-punning. */
#	define D_M128I(_m) (*((__m128i*)&(_m)))
#	define D_M128(_mi) (*((__m128*)&(_mi)))
#	define D_VEC_128I(_v) D_M128I(_v)
#endif

#if D_KISS
#	define D_V4_SHUFFLE(_v, _ix, _iy, _iz, _iw) V4_set(V4_at(_v, _ix), V4_at(_v, _iy), V4_at(_v, _iz), V4_at(_v, _iw))
#	define D_V4_MIX(_v0, _v1, _ix0, _iy0, _iz1, _iw1) V4_set(V4_at(_v0, _ix0), V4_at(_v0, _iy0), V4_at(_v1, _iz1), V4_at(_v1, _iw1))
#else
#	define D_V4_SHUFFLE(_v, _ix, _iy, _iz, _iw) _mm_shuffle_ps(_v, _v, (_ix)|((_iy)<<2)|((_iz)<<4)|((_iw)<<6))
#	define D_V4_MIX(_v0, _v1, _ix0, _iy0, _iz1, _iw1) _mm_shuffle_ps(_v0, _v1, (_ix0)|((_iy0)<<2)|((_iz1)<<4)|((_iw1)<<6))
#endif

#define D_V4_FILL_ELEM(_v, _idx) D_V4_SHUFFLE(_v, _idx, _idx, _idx, _idx)

#define D_MTX_POS(_name) union {QMTX _name; struct {float sr_mtx[3][4]; UVEC pos;};}

typedef float VEC[3];
typedef __m128 QVEC;
typedef __m128i QIVEC;
typedef float MTX[4][4];
typedef float D_STRUCT_ALIGN16_PREFIX QMTX[4][4] D_STRUCT_ALIGN16_POSTFIX;

typedef union _UVEC {
	QVEC    qv;
	QIVEC   iv;
	VEC     v;
	float   f[4];
	sys_i32 i[4];
	struct {float x, y, z, w;};
	struct {float r, g, b, a;};
} UVEC;

typedef union _UVEC3 {
	VEC v;
	struct {float x, y, z;};
	struct {float r, g, b;};
} UVEC3;

typedef struct _SH_CHANNEL {
	UVEC     v[2];
	float    f;
	sys_ui32 pad[3];
} SH_CHANNEL;

typedef struct _SH_COEF {
	SH_CHANNEL r;
	SH_CHANNEL g;
	SH_CHANNEL b;
} SH_COEF;

typedef struct _SH_PARAM {
	UVEC sh[7];
} SH_PARAM;

typedef struct _SPL_BEZ01 {
	float p1;
	float p2;
} SPL_BEZ01;

typedef struct _GEOM_LINE {
	UVEC pos0;
	UVEC pos1;
} GEOM_LINE;

typedef struct _GEOM_AABB {
	UVEC min;
	UVEC max;
} GEOM_AABB;

typedef struct _GEOM_OBB {
	D_MTX_POS(mtx);
	UVEC rad;
} GEOM_OBB;

typedef union _GEOM_SPHERE {
	QVEC qv;
	struct {float x, y, z, r;};
} GEOM_SPHERE;

typedef struct _GEOM_CAPSULE {
	GEOM_SPHERE pos0r;
	UVEC        pos1;
} GEOM_CAPSULE;

typedef union _GEOM_PLANE {
	QVEC qv;
	struct {float a, b, c, d;};
} GEOM_PLANE;

typedef struct _GEOM_FRUSTUM {
	UVEC pnt[8];
	UVEC nrm[6];
	GEOM_PLANE pln[6];
} GEOM_FRUSTUM;

D_EXTERN_DATA QMTX g_identity;

D_EXTERN_DATA QVEC g_axis_qdop8[];
D_EXTERN_DATA QVEC g_axis_qdop16[];
D_EXTERN_DATA QVEC g_axis_qdop24[];
D_EXTERN_DATA QVEC g_axis_qdop32[];

#ifdef __cplusplus
extern "C" {
#endif

float F_min(float x, float y);
float F_max(float x, float y);
sys_ui32 F_get_bits(float x);
float F_set_bits(sys_ui32 bits);
sys_ui16 F_encode_half(float x);
float F_decode_half(sys_ui16 h);
float F_hypot(float x, float y);
float F_limit_pi(float rad);
float F_sin_from_cos(float c);

void VEC_cpy(VEC vdst, VEC vsrc);
void VEC_add(VEC v0, VEC v1, VEC v2);
void VEC_sub(VEC v0, VEC v1, VEC v2);
void VEC_mul(VEC v0, VEC v1, VEC v2);
void VEC_div(VEC v0, VEC v1, VEC v2);
void VEC_cross(VEC v0, VEC v1, VEC v2);
float VEC_dot(VEC v0, VEC v1);
float VEC_mag2(VEC v);
float VEC_mag(VEC v);

void V4_store(float* p, QVEC v);
void V4_store_vec3(VEC v3, QVEC qv);
float V4_at(QVEC v, int idx);
QVEC V4_set(float x, float y, float z, float w);
QVEC V4_set_vec(float x, float y, float z);
QVEC V4_set_pnt(float x, float y, float z);
QVEC V4_fill(float x);
QVEC V4_load(float* p);
QVEC V4_load_vec3(VEC v);
QVEC V4_load_pnt3(VEC v);
QVEC V4_zero(void);
QVEC V4_set_w0(QVEC v);
QVEC V4_set_w1(QVEC v);
QVEC V4_set_w(QVEC v, float w);
QVEC V4_scale(QVEC v, float s);
QVEC V4_normalize(QVEC v);
QVEC V4_add(QVEC a, QVEC b);
QVEC V4_sub(QVEC a, QVEC b);
QVEC V4_mul(QVEC a, QVEC b);
QVEC V4_div(QVEC a, QVEC b);
QVEC V4_combine(QVEC a, float sa, QVEC b, float sb);
QVEC V4_lerp(QVEC a, QVEC b, float bias);
QVEC V4_cross(QVEC a, QVEC b);
QVEC V4_vdot(QVEC a, QVEC b);
float V4_dot(QVEC a, QVEC b);
float V4_dot4(QVEC a, QVEC b);
float V4_triple(QVEC v0, QVEC v1, QVEC v2);
float V4_mag2(QVEC v);
float V4_mag(QVEC v);
float V4_dist(QVEC v0, QVEC v1);
float V4_dist2(QVEC v0, QVEC v1);
QVEC V4_clamp(QVEC v, QVEC min, QVEC max);
QVEC V4_saturate(QVEC v);
QVEC V4_min(QVEC a, QVEC b);
QVEC V4_max(QVEC a, QVEC b);
QVEC V4_abs(QVEC v);
QVEC V4_neg(QVEC v);
QVEC V4_inv(QVEC v);
QVEC V4_rcp(QVEC v);
QVEC V4_sqrt(QVEC v);
QVEC V4_neg_xyz(QVEC v);
int V4_same(QVEC a, QVEC b);
int V4_same_xyz(QVEC a, QVEC b);
int V4_eq(QVEC a, QVEC b);
int V4_ne(QVEC a, QVEC b);
int V4_lt(QVEC a, QVEC b);
int V4_le(QVEC a, QVEC b);
int V4_gt(QVEC a, QVEC b);
int V4_ge(QVEC a, QVEC b);
void V4_print(QVEC v);

void MTX_cpy(MTX mdst, MTX msrc);
void MTX_clear(MTX m);
void MTX_unit(MTX m);
void MTX_load(MTX m, float* p);
void MTX_load64(MTX m, double* p);
void MTX_store(MTX m, float* p);
void MTX_transpose(MTX m0, MTX m1);
void MTX_transpose_sr(MTX m0, MTX m1);
void MTX_invert(MTX m0, MTX m1);
void MTX_invert_fast(MTX m0, MTX m1);
void MTX_mul(MTX m0, MTX m1, MTX m2);
void MTX_rot_x(MTX m, float rad);
void MTX_rot_y(MTX m, float rad);
void MTX_rot_z(MTX m, float rad);
void MTX_rot_xyz(MTX m, float rx, float ry, float rz);
void MTX_rot_axis(MTX m, QVEC axis, float rad);
void MTX_make_view(MTX m, QVEC pos, QVEC tgt, QVEC upvec);
void MTX_make_proj(MTX m, float fovy, float aspect, float znear, float zfar);
void MTX_make_frame(MTX m, QVEC dir, int hou_flg);
void MTX_calc_vec(VEC vdst, MTX m, VEC vsrc);
void MTX_calc_pnt(VEC vdst, MTX m, VEC vsrc);
QVEC MTX_calc_qvec(MTX m, QVEC v);
QVEC MTX_calc_qpnt(MTX m, QVEC v);
QVEC MTX_apply(MTX m, QVEC v);
QVEC MTX_get_rot_xyz(MTX m);
QVEC MTX_get_row(MTX m, int idx);
void MTX_set_row(MTX m, int idx, QVEC v);

QVEC QUAT_unit(void);
QVEC QUAT_from_axis_angle(QVEC axis, float ang);
QVEC QUAT_from_mtx(MTX m);
QVEC QUAT_get_vec_x(QVEC q);
QVEC QUAT_get_vec_y(QVEC q);
QVEC QUAT_get_vec_z(QVEC q);
void QUAT_get_mtx(QVEC q, MTX m);
QVEC QUAT_normalize(QVEC q);
QVEC QUAT_mul(QVEC q, QVEC b);
QVEC QUAT_apply(QVEC q, QVEC v);
QVEC QUAT_lerp(QVEC a, QVEC b, float bias);
QVEC QUAT_slerp(QVEC a, QVEC b, float bias);
QVEC QUAT_conjugate(QVEC q);
QVEC QUAT_invert(QVEC q);

sys_ui32 CLR_f2i(QVEC cv);
QVEC CLR_i2f(sys_ui32 ci);
QVEC CLR_HDR_encode(QVEC qrgb);
QVEC CLR_HDR_decode(QVEC qhdr);
QVEC CLR_RGB_to_HSV(QVEC qrgb);
QVEC CLR_HSV_to_RGB(QVEC qhsv);
QVEC CLR_RGB_to_YCbCr(QVEC qrgb);
QVEC CLR_YCbCr_to_RGB(QVEC qybr);
QVEC CLR_RGB_to_XYZ(QVEC qrgb, MTX* pMtx);
QVEC CLR_XYZ_to_RGB(QVEC qxyz, MTX* pMtx);
QVEC CLR_XYZ_to_Lab(QVEC qxyz, MTX* pMtx);
QVEC CLR_Lab_to_XYZ(QVEC qlab, MTX* pMtx);
QVEC CLR_RGB_to_Lab(QVEC qxyz, MTX* pMtx);
QVEC CLR_Lab_to_RGB(QVEC qlab, MTX* pRGB2XYZ, MTX* pXYZ2RGB);
QVEC CLR_Lab_to_LCH(QVEC qlab);
QVEC CLR_LCH_to_Lab(QVEC qlch);
float CLR_get_luma(QVEC qrgb);
float CLR_get_luminance(QVEC qrgb, MTX* pMtx);
void CLR_calc_XYZ_transform(MTX* pRGB2XYZ, MTX* pXYZ2RGB, QVEC* pPrim, QVEC* pWhite);

void SH_clear_ch(SH_CHANNEL* pChan);
void SH_clear(SH_COEF* pCoef);
void SH_add_ch(SH_CHANNEL* pCh0, SH_CHANNEL* pCh1, SH_CHANNEL* pCh2);
void SH_add(SH_COEF* pCoef0, SH_COEF* pCoef1, SH_COEF* pCoef2);
void SH_scale_ch(SH_CHANNEL* pCh0, SH_CHANNEL* pCh1, float s);
void SH_scale(SH_COEF* pCoef0, SH_COEF* pCoef1, float s);
void SH_calc_dir_ch(SH_CHANNEL* pCh, float c, UVEC* pDir);
void SH_calc_dir(SH_COEF* pCoef, UVEC* pColor, UVEC* pDir);
void SH_calc_param_ch(SH_PARAM* pParam, SH_CHANNEL* pChan, int ch_idx);
void SH_calc_param(SH_PARAM* pParam, SH_COEF* pCoef);

float SPL_hermite(float p0, float m0, float p1, float m1, float t);
float SPL_overhauser(QVEC pvec, float t);
void SPL_bezier01_reset(SPL_BEZ01* pBez);
void SPL_bezier01_set(SPL_BEZ01* pBez, float p1, float p2);
float SPL_bezier01(SPL_BEZ01* pBez, float t);

QVEC GEOM_get_plane(QVEC pos, QVEC nrm);
QVEC GEOM_intersect_3_planes(QVEC pln0, QVEC pln1, QVEC pln2);
QVEC GEOM_tri_norm_cw(QVEC v0, QVEC v1, QVEC v2);
QVEC GEOM_tri_norm_ccw(QVEC v0, QVEC v1, QVEC v2);
QVEC GEOM_poly_norm_cw(QVEC* pVtx, int nvtx);
QVEC GEOM_poly_norm_ccw(QVEC* pVtx, int nvtx);
float GEOM_line_closest(QVEC pos, QVEC p0, QVEC p1, QVEC* pPnt, QVEC* pDir);
QVEC GEOM_seg_closest(QVEC pos, QVEC p0, QVEC p1);
float GEOM_seg_dist2(GEOM_LINE* pSeg0, GEOM_LINE* pSeg1, GEOM_LINE* pBridge);
void GEOM_sph_from_pts(GEOM_SPHERE* pSph, QVEC* pPnt, int npnt);
int GEOM_sph_overlap(QVEC sph0, QVEC sph1);
int GEOM_sph_cap_check(QVEC sph, QVEC pos0r, QVEC pos1);
int GEOM_sph_aabb_check(QVEC sph, QVEC min, QVEC max);
int GEOM_sph_obb_check(QVEC sph, GEOM_OBB* pBox);
int GEOM_cap_overlap(GEOM_CAPSULE* pCap0, GEOM_CAPSULE* pCap1);
int GEOM_cap_obb_check(GEOM_CAPSULE* pCap, GEOM_OBB* pBox);
void GEOM_aabb_init(GEOM_AABB* pBox);
QVEC GEOM_aabb_center(GEOM_AABB* pBox);
QVEC GEOM_aabb_size(GEOM_AABB* pBox);
QVEC GEOM_aabb_extents(GEOM_AABB* pBox);
float GEOM_aabb_bounding_radius(GEOM_AABB* pBox);
void GEOM_aabb_transform(GEOM_AABB* pNew, MTX m, GEOM_AABB* pOld);
void GEOM_aabb_from_frustum(GEOM_AABB* pBox, GEOM_FRUSTUM* pFst);
int GEOM_aabb_overlap(GEOM_AABB* pBox0, GEOM_AABB* pBox1);
int GEOM_pnt_inside_aabb(QVEC pos, GEOM_AABB* pBox);
void GEOM_obb_from_mtx(GEOM_OBB* pBox, MTX m);
void GEOM_obb_from_frustum(GEOM_OBB* pBox, GEOM_FRUSTUM* pFst);
int GEOM_obb_overlap(GEOM_OBB* pBox0, GEOM_OBB* pBox1);
int GEOM_qslab_overlap(QVEC* pMin0, QVEC* pMax0, QVEC* pMin1, QVEC* pMax1);

/* QVEC-aligned kDOPs, k = 8, 16, 24, 32, 40 */
void GEOM_qdop_init(QVEC* pMin, QVEC* pMax, int k);
void GEOM_qdop_add_pnt(QVEC* pMin, QVEC* pMax, QVEC* pAxis, int k, QVEC pos);
int GEOM_qdop_overlap(QVEC* pMin0, QVEC* pMax0, QVEC* pMin1, QVEC* pMax1, int k);

float GEOM_tri_dist2(QVEC pos, QVEC* pVtx);
float GEOM_quad_dist2(QVEC pos, QVEC* pVtx);
int GEOM_seg_quad_intersect(QVEC p0, QVEC p1, QVEC* pVtx, QVEC* pHit_pos, QVEC* pHit_nml);
int GEOM_seg_polyhedron_intersect(QVEC p0, QVEC p1, GEOM_PLANE* pPln, int n, QVEC* pRes);
int GEOM_seg_aabb_check(QVEC p0, QVEC p1, GEOM_AABB* pBox);
int GEOM_seg_obb_check(QVEC p0, QVEC p1, GEOM_OBB* pBox);
int GEOM_barycentric(QVEC pos, QVEC* pVtx, QVEC* pCoord);
void GEOM_frustum_init(GEOM_FRUSTUM* pFst, MTX m, float fovy, float aspect, float znear, float zfar);
int GEOM_frustum_aabb_check(GEOM_FRUSTUM* pFst, GEOM_AABB* pBox);
int GEOM_frustum_aabb_cull(GEOM_FRUSTUM* pFst, GEOM_AABB* pBox);
int GEOM_frustum_obb_check(GEOM_FRUSTUM* pFst, GEOM_OBB* pBox);
int GEOM_frustum_obb_cull(GEOM_FRUSTUM* pFst, GEOM_OBB* pBox);
int GEOM_frustum_sph_cull(GEOM_FRUSTUM* pFst, GEOM_SPHERE* pSph);

#ifdef __cplusplus
}
#endif

