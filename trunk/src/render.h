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

#define D_RDR_ARGB32(a,r,g,b) (sys_ui32)( (((a)&0xFF)<<24)|(((r)&0xFF)<<16)|(((g)&0xFF)<<8)|(((b)&0xFF)<<0) )
#define D_RDR_RGB32(r,g,b) D_DRW_ARGB32(0xFF,(r),(g),(b))

typedef enum _E_RDR_LAYER {
	E_RDR_LAYER_ZBUF,
	E_RDR_LAYER_CAST,
	E_RDR_LAYER_MTL0,
	E_RDR_LAYER_MTL1,
	E_RDR_LAYER_RECV,
	E_RDR_LAYER_MTL2,
	E_RDR_LAYER_PTCL,
	E_RDR_LAYER_MAX
} E_RDR_LAYER;

typedef enum _E_RDR_PARAMTYPE {
	E_RDR_PARAMTYPE_FVEC  = 0,
	E_RDR_PARAMTYPE_IVEC  = 1,
	E_RDR_PARAMTYPE_FLOAT = 2,
	E_RDR_PARAMTYPE_INT   = 3,
	E_RDR_PARAMTYPE_SMP   = 4,
	E_RDR_PARAMTYPE_BOOL  = 5
} E_RDR_PARAMTYPE;

typedef enum _E_RDR_VTXTYPE {
	E_RDR_VTXTYPE_GENERAL = 0,
	E_RDR_VTXTYPE_SOLID   = 1,
	E_RDR_VTXTYPE_SKIN    = 2,
	E_RDR_VTXTYPE_SCREEN  = 3
} E_RDR_VTXTYPE;

typedef enum _E_RDR_IDXTYPE {
	E_RDR_IDXTYPE_16BIT = 0,
	E_RDR_IDXTYPE_32BIT = 1
} E_RDR_IDXTYPE;

typedef enum _E_RDR_TEXTYPE {
	E_RDR_TEXTYPE_2D,
	E_RDR_TEXTYPE_CUBE,
	E_RDR_TEXTYPE_VOLUME
} E_RDR_TEXTYPE;

typedef enum _E_RDR_RSRCATTR {
	E_RDR_RSRCATTR_DYNAMIC = 1
} E_RDR_RSRCATTR;

typedef enum _E_RDR_TEXADDR {
	E_RDR_TEXADDR_WRAP     = 0,
	E_RDR_TEXADDR_MIRROR   = 1,
	E_RDR_TEXADDR_CLAMP    = 2,
	E_RDR_TEXADDR_MIRCLAMP = 3,
	E_RDR_TEXADDR_BORDER   = 4
} E_RDR_TEXADDR;

typedef enum _E_RDR_TEXFILTER {
	E_RDR_TEXFILTER_POINT       = 0,
	E_RDR_TEXFILTER_LINEAR      = 1,
	E_RDR_TEXFILTER_ANISOTROPIC = 2
} E_RDR_TEXFILTER;

typedef enum _E_RDR_BLENDOP {
	E_RDR_BLENDOP_ADD   = 0,
	E_RDR_BLENDOP_SUB   = 1,
	E_RDR_BLENDOP_MIN   = 2,
	E_RDR_BLENDOP_MAX   = 3,
	E_RDR_BLENDOP_RSUB  = 4,
	E_RDR_BLENDOP_MASK  = 7
} E_RDR_BLENDOP;

typedef enum _E_RDR_BLENDMODE {
	E_RDR_BLENDMODE_ZERO     = 0,
	E_RDR_BLENDMODE_ONE      = 1,
	E_RDR_BLENDMODE_SRC      = 2,
	E_RDR_BLENDMODE_INVSRC   = 3,
	E_RDR_BLENDMODE_DST      = 4,
	E_RDR_BLENDMODE_INVDST   = 5,
	E_RDR_BLENDMODE_SRCA     = 6,
	E_RDR_BLENDMODE_INVSRCA  = 7,
	E_RDR_BLENDMODE_SRCASAT  = 8,
	E_RDR_BLENDMODE_SRCA2    = 9,
	E_RDR_BLENDMODE_INVSRCA2 = 10,
	E_RDR_BLENDMODE_DSTA     = 11,
	E_RDR_BLENDMODE_INVDSTA  = 12,
	E_RDR_BLENDMODE_CONST    = 13,
	E_RDR_BLENDMODE_INVCONST = 14,

	E_RDR_BLENDMODE_MASK = 0xF
} E_RDR_BLENDMODE;

typedef enum _E_RDR_CMPFUNC {
	E_RDR_CMPFUNC_NEVER        = 0,
	E_RDR_CMPFUNC_GREATEREQUAL = 1,
	E_RDR_CMPFUNC_EQUAL        = 2,
	E_RDR_CMPFUNC_GREATER      = 3,
	E_RDR_CMPFUNC_LESSEQUAL    = 4,
	E_RDR_CMPFUNC_NOTEQUAL     = 5,
	E_RDR_CMPFUNC_LESS         = 6,
	E_RDR_CMPFUNC_ALWAYS       = 7,

	E_RDR_CMPFUNC_MASK = 7
} E_RDR_CMPFUNC;

typedef enum _E_RDR_CULL {
	E_RDR_CULL_NONE = 0,
	E_RDR_CULL_CCW  = 1,
	E_RDR_CULL_CW   = 2,

	E_RDR_CULL_MASK = 3
} E_RDR_CULL;

typedef enum _E_RDR_PRIMTYPE {
	E_RDR_PRIMTYPE_TRILIST   = 0,
	E_RDR_PRIMTYPE_TRISTRIP  = 1,
	E_RDR_PRIMTYPE_LINELIST  = 2,
	E_RDR_PRIMTYPE_LINESTRIP = 3,
} E_RDR_PRIMTYPE;

typedef enum _E_RDR_CCMODE {
	E_RDR_CCMODE_NONE = 0,
	E_RDR_CCMODE_LEVELS,
	E_RDR_CCMODE_LINEAR,
	E_RDR_CCMODE_LUM
} E_RDR_CCMODE;


typedef union _RDR_COLOR32 {
	sys_ui32 argb32;
	struct {sys_byte b, g, r, a;};
	sys_byte c[4];
} RDR_COLOR32;

typedef struct _RDR_VTX_GENERAL {
/* +00 */	float pos[4];
/* +10 */	float nrm[4];
/* +20 */	float tng[4];
/* +30 */	float clr[4];
/* +40 */	float uv0[4];
/* +50 */	float uv1[4];
/* +60 */	float uv2[4];
/* +70 */	float uv3[4];
/* +80 */	float idx[4];
/* +90 */	float wgt[4];
/* +A0 */	float bnm[4];
} RDR_VTX_GENERAL;

typedef struct _RDR_VTX_SOLID {
	float       pos[3];
	sys_byte    nrm[4];
	sys_byte    tng[4];
	RDR_COLOR32 clr;
	sys_i16     tex[4];
} RDR_VTX_SOLID;

typedef struct _RDR_VTX_SKIN {
	sys_i16  pos[4];
	sys_byte nrm[4];
	sys_byte tng[4];
	sys_i16  tex[4];
	sys_byte idx[4];
	sys_byte wgt[4];
} RDR_VTX_SKIN;

typedef struct _RDR_VTX_SCREEN {
	float       pos[3];
	RDR_COLOR32 clr;
	float       tex[4];
} RDR_VTX_SCREEN;

typedef void* RDR_HANDLE;

typedef union _RDR_LOCKED_VTX {
	void*            pData;
	RDR_VTX_GENERAL* pGen;
	RDR_VTX_SOLID*   pSolid;
	RDR_VTX_SKIN*    pSkin;
	RDR_VTX_SCREEN*  pScreen;
} RDR_LOCKED_VTX;

typedef struct _RDR_VTX_BUFFER {
	sys_byte       type; /* E_RDR_VTXTYPE */
	sys_byte       attr; /* E_RDR_RSRCATTR */
	sys_ui16       vtx_size;
	sys_ui32       nb_vtx;
	RDR_HANDLE     handle;
	RDR_LOCKED_VTX locked;
} RDR_VTX_BUFFER;

typedef struct _RDR_IDX_BUFFER {
	sys_byte   type; /* E_RDR_IDXTYPE */
	sys_byte   attr; /* E_RDR_RSRCATTR */
	sys_ui16   reserved;
	sys_ui32   nb_idx;
	RDR_HANDLE handle;
	void*     pData;
} RDR_IDX_BUFFER;

typedef struct _RDR_TARGET {
	RDR_HANDLE hTgt;
	RDR_HANDLE hDepth;
	RDR_HANDLE hTgt_surf;
	RDR_HANDLE hDepth_surf;
	sys_ui16   w;
	sys_ui16   h;
} RDR_TARGET;

typedef struct _RDR_TEXTURE {
	RDR_HANDLE handle;
	sys_ui16   w;
	sys_ui16   h;
	sys_ui16   d;
	sys_byte   nb_lvl;
	sys_byte   type; /* E_RDR_TEXTYPE */
} RDR_TEXTURE;

typedef struct _RDR_SAMPLER {
	RDR_HANDLE  hTex;
	RDR_COLOR32 border;
	float       mip_bias;
/*  bits  */
/* 00..02 */	sys_ui32 addr_u     : 3; /* E_RDR_TEXADDR */
/* 03..05 */	sys_ui32 addr_v     : 3; /* E_RDR_TEXADDR */
/* 06..08 */	sys_ui32 addr_w     : 3; /* E_RDR_TEXADDR */
/* 09..0A */	sys_ui32 min        : 2; /* E_RDR_TEXFILTER */
/* 0B..0C */	sys_ui32 mag        : 2; /* E_RDR_TEXFILTER */
/* 0D..0E */	sys_ui32 mip        : 2; /* E_RDR_TEXFILTER */
/* 0F..12 */	sys_ui32 anisotropy : 4;
/* 13..16 */	sys_ui32 max_mip    : 4;
} RDR_SAMPLER;

typedef struct _RDR_GPARAM_ID {
	sys_ui16 offs : 13; /* D_RDR_GP_* */
	sys_ui16 type : 3; /* E_RDR_PARAMTYPE */
} RDR_GPARAM_ID;

typedef struct _RDR_GPARAM_INFO {
	RDR_GPARAM_ID id;
	sys_ui16 reg;
	sys_ui16 len;
	sys_ui16 name;
} RDR_GPARAM_INFO;

typedef struct _RDR_BATCH_PARAM {
	union {
		void*     pVal;
		UVEC*     pVec;
		float*    pFloat;
		sys_i32*  pInt;
		sys_byte* pBool;
	};
	RDR_GPARAM_ID id;
	sys_ui16      count;
} RDR_BATCH_PARAM;

typedef struct _RDR_BLEND_STATE {
	sys_ui32 op    : 3; /* E_RDR_BLENDOP */
	sys_ui32 src   : 4; /* E_RDR_BLENDMODE */
	sys_ui32 dst   : 4; /* E_RDR_BLENDMODE */
	sys_ui32 op_a  : 3; /* E_RDR_BLENDOP */
	sys_ui32 src_a : 4; /* E_RDR_BLENDMODE */
	sys_ui32 dst_a : 4; /* E_RDR_BLENDMODE */
	sys_ui32 on    : 1;
} RDR_BLEND_STATE;

typedef struct _RDR_DRAW_STATE {
	sys_ui32 color_mask : 4;
	sys_ui32 cull       : 2; /* E_RDR_CULL */
	sys_ui32 ztest      : 1;
	sys_ui32 zwrite     : 1;
	sys_ui32 zfunc      : 3; /* E_RDR_CMPFUNC */
	sys_ui32 atest      : 1;
	sys_ui32 afunc      : 3; /* E_RDR_CMPFUNC */
	sys_ui32 msaa       : 1;
} RDR_DRAW_STATE;

typedef struct _RDR_BATCH {
	RDR_VTX_BUFFER* pVtx;
	RDR_IDX_BUFFER* pIdx;
	RDR_BATCH_PARAM* pParam;
	sys_ui32 count : 29;
	sys_ui32 type  : 3; /* E_RDR_PRIMTYPE */
	sys_ui32 start;
	sys_ui32 base;
	RDR_BLEND_STATE blend_state;
	RDR_DRAW_STATE draw_state;
	sys_ui16 vtx_prog;
	sys_ui16 pix_prog;
	sys_ui16 nb_param;
} RDR_BATCH;

typedef struct _RDR_PROG_INFO {
	sys_ui16 name;
	sys_ui16 nb_param;
	sys_ui32 param_offs;
	sys_ui32 code_offs;
	sys_ui32 code_len;
} RDR_PROG_INFO;

typedef struct _RDR_PROG {
	RDR_HANDLE     handle;
	RDR_PROG_INFO* pInfo;
} RDR_PROG;

typedef struct _RDR_VIEW {
	QMTX view;
	QMTX proj;
	QMTX iview;
	QMTX iproj;
	QMTX view_proj;
	UVEC pos;
	UVEC dir;
} RDR_VIEW;


#include "gen/gparam.h"

D_EXTERN_DATA RDR_GPARAM g_rdr_param;

D_EXTERN_FUNC void RDR_init(void* hWnd, int width, int height, int fullscreen);
D_EXTERN_FUNC void RDR_reset(void);
D_EXTERN_FUNC void RDR_init_thread_FPU(void);
D_EXTERN_FUNC void RDR_begin(void);
D_EXTERN_FUNC void RDR_exec(void);

D_EXTERN_FUNC RDR_VIEW* RDR_get_view(void);

D_EXTERN_FUNC UVEC* RDR_get_val_v(int n);
D_EXTERN_FUNC float* RDR_get_val_f(int n);
D_EXTERN_FUNC sys_i32* RDR_get_val_i(int n);
D_EXTERN_FUNC sys_byte* RDR_get_val_b(int n);
D_EXTERN_FUNC RDR_BATCH_PARAM* RDR_get_param(int n);
D_EXTERN_FUNC RDR_BATCH* RDR_get_batch(void);
D_EXTERN_FUNC void RDR_put_batch(RDR_BATCH* pBatch, sys_ui32 key, sys_uint lyr_id);

D_EXTERN_FUNC RDR_VTX_BUFFER* RDR_vtx_create(E_RDR_VTXTYPE type, int n);
D_EXTERN_FUNC RDR_VTX_BUFFER* RDR_vtx_create_dyn(E_RDR_VTXTYPE type, int n);
D_EXTERN_FUNC void RDR_vtx_release(RDR_VTX_BUFFER* pVB);
D_EXTERN_FUNC void RDR_vtx_lock(RDR_VTX_BUFFER* pVB);
D_EXTERN_FUNC void RDR_vtx_unlock(RDR_VTX_BUFFER* pVB);

D_EXTERN_FUNC RDR_IDX_BUFFER* RDR_idx_create(E_RDR_IDXTYPE type, int n);
D_EXTERN_FUNC RDR_IDX_BUFFER* RDR_idx_create_dyn(E_RDR_IDXTYPE type, int n);
D_EXTERN_FUNC void RDR_idx_release(RDR_IDX_BUFFER* pIB);
D_EXTERN_FUNC void RDR_idx_lock(RDR_IDX_BUFFER* pIB);
D_EXTERN_FUNC void RDR_idx_unlock(RDR_IDX_BUFFER* pIB);

D_EXTERN_FUNC RDR_TEXTURE* RDR_tex_create(void* pTop, int w, int h, int d, int nb_lvl, sys_ui32 fmt, sys_i32* pOffs);
D_EXTERN_FUNC void RDR_tex_release(RDR_TEXTURE* pTex);

