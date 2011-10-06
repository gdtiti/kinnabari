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


typedef struct _OMD_HEAD {
/* +00 */	sys_ui32 magic;
/* +04 */	sys_ui32 nb_mtl;
/* +08 */	sys_ui32 nb_grp;
/* +0C */	sys_ui32 nb_vtx;
/* +10 */	sys_ui32 nb_idx;
/* +14 */	sys_ui32 offs_grp;
/* +18 */	sys_ui32 offs_vtx;
/* +1C */	sys_ui32 offs_idx;
/* +20 */	sys_ui32 nb_jnt;
/* +24 */	sys_ui32 offs_cull;
/* +28 */	sys_ui32 reserved_0;
/* +2C */	sys_ui32 reserved_1;
} OMD_HEAD;

typedef struct _OMD_JNT_INFO {
	UVEC3   offs;
	sys_i16 id;
	sys_i16 parent_id;
} OMD_JNT_INFO;

typedef struct _OMD_VERTEX {
	UVEC3 pos;
	UVEC3 nrm;
	struct {float u, v;} tex;
	sys_byte idx[4];
	float wgt[4];
} OMD_VERTEX;

typedef struct _OMD_GROUP {
	UVEC3    clr;
	sys_i32  mtl_id;
	sys_ui32 start;
	sys_ui32 count;
	sys_ui32 reserved[2];
} OMD_GROUP;

typedef struct _OMD_CULL_HEAD {
	sys_ui32 magic;
	sys_ui32 data_size;
	struct {
		sys_ui32 offs;
		sys_ui32 nb_jnt;
	} list[1];
} OMD_CULL_HEAD;

typedef struct _JNT_INFO JNT_INFO;

struct _JNT_INFO {
	UVEC offs_len;
	JNT_INFO* pParent_info;
	const char* pName;
	sys_i16 id;
	sys_i16 parent_id;
};

typedef struct _PRIM_GROUP {
	UVEC     base_color;
	sys_i32  mtl_id;
	sys_ui32 start;
	sys_ui32 count;
	sys_ui32 reserved;
} PRIM_GROUP;

typedef struct _OMD {
	JNT_INFO* pJnt_info;
	MTX* pJnt_inv;
	MTL_LIST* pMtl_lst;
	DICT* pName_dict;
	PRIM_GROUP* pGrp;
	OMD_CULL_HEAD* pCull;
	RDR_VTX_BUFFER* pVtx;
	RDR_IDX_BUFFER* pIdx;
	int nb_jnt;
	int nb_grp;
} OMD;

typedef struct _JOINT {
	D_MTX_POS(mtx);
	UVEC3 rot;
	MTX* pParent_mtx;
	JNT_INFO* pInfo;
} JOINT;

typedef struct _MODEL {
	D_MTX_POS(root_mtx);
	UVEC prev_pos;
	UVEC3 rot;
	OMD* pOmd;
	JOINT* pJnt;
	MTX* pJnt_wmtx;
	sys_ui32* pCull;
	sys_ui32* pHide;
} MODEL;

typedef struct _MDL_SYS {
	SYM_POOL* pName_pool;
} MDL_SYS;

D_EXTERN_DATA MDL_SYS g_mdl_sys;

D_EXTERN_FUNC void MDL_sys_init(void);
D_EXTERN_FUNC void MDL_sys_reset(void);

D_EXTERN_FUNC OMD* OMD_load(const char* name);
D_EXTERN_FUNC void OMD_free(OMD* pOmd);
D_EXTERN_FUNC JNT_INFO* OMD_get_jnt_info(OMD* pOmd, const char* name);
D_EXTERN_FUNC int OMD_get_jnt_idx(OMD* pOmd, const char* name);

D_EXTERN_FUNC MODEL* MDL_create(OMD* pOmd);
D_EXTERN_FUNC void MDL_destroy(MODEL* pMdl);
D_EXTERN_FUNC void MDL_jnt_reset(MODEL* pMdl);
D_EXTERN_FUNC void MDL_calc_local(MODEL* pMdl);
D_EXTERN_FUNC void MDL_calc_world(MODEL* pMdl);
D_EXTERN_FUNC JOINT* MDL_get_jnt(MODEL* pMdl, const char* name);
D_EXTERN_FUNC void MDL_cull(MODEL* pMdl, CAMERA* pCam);
D_EXTERN_FUNC void MDL_disp(MODEL* pMdl);

