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

#define D_MAX_ROOM_MDL (16)

typedef enum _E_RMD_DISPATTR {
	E_RMD_DISPATTR_HIDE = 1,
	E_RMD_DISPATTR_CULL = 2
} E_RMD_DISPATTR;

typedef struct _RMD_HEAD {
	sys_ui32  magic;
	sys_ui32  nb_mtl;
	sys_ui32  nb_grp;
	sys_ui32  nb_vtx;
	sys_ui32  nb_idx;
	sys_ui32  offs_grp;
	sys_ui32  offs_vtx;
	sys_ui32  offs_idx;
	GEOM_AABB bbox;
} RMD_HEAD;

typedef struct _RMD_GROUP {
	GEOM_AABB bbox;
	sys_i32   mtl_id;
	sys_ui32  start;
	sys_ui32  count;
	sys_ui32  reserved;
} RMD_GROUP;

typedef struct _RMD_VERTEX {
	UVEC3 pos;
	UVEC3 nrm;
	UVEC3 clr;
	struct {float u, v;} tex;
} RMD_VERTEX;

typedef struct _RM_PRIM_GRP {
	sys_i32  mtl_id;
	sys_ui32 start;
	sys_ui32 count;
} RM_PRIM_GRP;

typedef struct _ROOM_MODEL {
	GEOM_AABB  bbox;
	UVEC       base_color;
	GEOM_AABB* pGrp_bbox;
	MTL_LIST*  pMtl_lst;
	RM_PRIM_GRP* pGrp;
	RDR_VTX_BUFFER* pVtx;
	RDR_IDX_BUFFER* pIdx;
	sys_ui32* pCull;
	sys_ui32* pHide;
	int nb_grp;
	sys_ui32 disp_attr;
} ROOM_MODEL;

typedef struct _ROOM {
	OBSTACLE obst;
	ROOM_MODEL* pRmd[D_MAX_ROOM_MDL];
} ROOM;

D_EXTERN_DATA ROOM g_room;

D_EXTERN_FUNC void ROOM_init(int id);
D_EXTERN_FUNC void ROOM_free(void);
D_EXTERN_FUNC void ROOM_cull(CAMERA* pCam);
D_EXTERN_FUNC void ROOM_disp(void);

