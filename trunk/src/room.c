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
#include "util.h"
#include "keyframe.h"
#include "render.h"
#include "camera.h"
#include "material.h"
#include "obstacle.h"
#include "room.h"

ROOM g_room;

static void Vtx_cpy(RDR_VTX_GENERAL* pDst, RMD_VERTEX* pSrc) {
	pDst->pos[0] = pSrc->pos.x;
	pDst->pos[1] = pSrc->pos.y;
	pDst->pos[2] = pSrc->pos.z;
	pDst->pos[3] = 1.0f;
	pDst->nrm[0] = pSrc->nrm.x;
	pDst->nrm[1] = pSrc->nrm.y;
	pDst->nrm[2] = pSrc->nrm.z;
	pDst->nrm[3] = 0.0f;
	pDst->clr[0] = pSrc->clr.r;
	pDst->clr[1] = pSrc->clr.g;
	pDst->clr[2] = pSrc->clr.b;
	pDst->clr[3] = 1.0f;
	pDst->uv0[0] = pSrc->tex.u;
	pDst->uv0[1] = pSrc->tex.v;
	pDst->uv0[2] = 0.0f;
	pDst->uv0[3] = 0.0f;
}

ROOM_MODEL* RMD_load(const char* name) {
	int i, n, mem_size;
	ROOM_MODEL* pRmd = NULL;
	RMD_HEAD* pHead;
	MTL_INFO* pMtl_info;
	sys_ui32* pName_offs;
	RMD_GROUP* pGrp_src;
	RMD_VERTEX* pVtx_src;
	sys_ui16* pIdx_src;
	void* pMem;

	pHead = (RMD_HEAD*)SYS_load(name);
	if (pHead && pHead->magic == D_FOURCC('R','M','D','\0')) {
		n = pHead->nb_grp;
		mem_size = D_ALIGN(sizeof(ROOM_MODEL), 16) + n*sizeof(GEOM_AABB) + n*sizeof(RM_PRIM_GRP) + 2*D_BIT_ARY_SIZE32(n)*sizeof(sys_ui32);
		pMem = SYS_malloc(mem_size);
		memset(pMem, 0, mem_size);
		pRmd = (ROOM_MODEL*)pMem;
		memcpy(&pRmd->bbox, &pHead->bbox, sizeof(GEOM_AABB));
		pRmd->base_color.qv = V4_fill(1.0f);
		pRmd->nb_grp = pHead->nb_grp;
		pRmd->disp_attr = 0;
		pRmd->pGrp_bbox = (GEOM_AABB*)D_INCR_PTR(pRmd, D_ALIGN(sizeof(ROOM_MODEL), 16));
		pRmd->pGrp = (RM_PRIM_GRP*)(pRmd->pGrp_bbox + n);
		pRmd->pCull = (sys_ui32*)(pRmd->pGrp + n);
		pRmd->pHide = pRmd->pCull + D_BIT_ARY_SIZE32(n);
		pMtl_info = (MTL_INFO*)(pHead + 1);
		pName_offs = (sys_ui32*)(pMtl_info + pHead->nb_mtl);
		pRmd->pMtl_lst = MTL_lst_create(pMtl_info, pName_offs, pHead, pHead->nb_mtl);
		pGrp_src = (RMD_GROUP*)D_INCR_PTR(pHead, pHead->offs_grp);
		pVtx_src = (RMD_VERTEX*)D_INCR_PTR(pHead, pHead->offs_vtx);
		pIdx_src = (sys_ui16*)D_INCR_PTR(pHead, pHead->offs_idx);
		for (i = 0; i < n; ++i) {
			memcpy(&pRmd->pGrp_bbox[i], &pGrp_src->bbox, sizeof(GEOM_AABB));
			pRmd->pGrp[i].mtl_id = pGrp_src->mtl_id;
			pRmd->pGrp[i].start = pGrp_src->start;
			pRmd->pGrp[i].count = pGrp_src->count;
			++pGrp_src;
		}
		pRmd->pVtx = RDR_vtx_create(E_RDR_VTXTYPE_GENERAL, pHead->nb_vtx);
		if (pRmd->pVtx) {
			RDR_vtx_lock(pRmd->pVtx);
			if (pRmd->pVtx->locked.pData) {
				RDR_VTX_GENERAL* pVtx_dst = pRmd->pVtx->locked.pGen;
				n = pHead->nb_vtx;
				for (i = 0; i < n; ++i) {
					Vtx_cpy(pVtx_dst, pVtx_src);
					++pVtx_src;
					++pVtx_dst;
				}
				RDR_vtx_unlock(pRmd->pVtx);
			}
		}
		pRmd->pIdx = RDR_idx_create(E_RDR_IDXTYPE_16BIT, pHead->nb_idx);
		if (pRmd->pIdx) {
			RDR_idx_lock(pRmd->pIdx);
			if (pRmd->pIdx->pData) {
				memcpy(pRmd->pIdx->pData, pIdx_src, pHead->nb_idx*sizeof(sys_ui16));
				RDR_idx_unlock(pRmd->pIdx);
			}
		}
		SYS_free(pHead);
	}
	return pRmd;
}

void RMD_free(ROOM_MODEL* pRmd) {
	if (pRmd) {
		RDR_idx_release(pRmd->pIdx);
		RDR_vtx_release(pRmd->pVtx);
		MTL_lst_destroy(pRmd->pMtl_lst);
	}
}

static int Rmd_cull_box(GEOM_AABB* pBox, CAMERA* pCam) {
	int flg = CAM_cull_box(pCam, pBox);
	if (!flg) {
		flg = CAM_cull_box_ex(pCam, pBox);
	}
	return flg;
}

void RMD_cull(ROOM_MODEL* pRmd, CAMERA* pCam) {
	pRmd->disp_attr &= ~E_RMD_DISPATTR_CULL;
	if (Rmd_cull_box(&pRmd->bbox, pCam)) {
		pRmd->disp_attr |= E_RMD_DISPATTR_CULL;
	} else {
		int i, n;
		GEOM_AABB* pBox = pRmd->pGrp_bbox;
		n = pRmd->nb_grp;
		for (i = 0; i < n; ++i) {
			if (Rmd_cull_box(pBox, pCam)) {
				D_BIT_ST(pRmd->pCull, i);
			} else {
				D_BIT_CL(pRmd->pCull, i);
			}
			++pBox;
		}
	}
}

void RMD_disp(ROOM_MODEL* pRmd) {
	int i, n;
	RM_PRIM_GRP* pGrp;
	RDR_BATCH* pBatch;
	RDR_BATCH_PARAM* pParam;

	if (pRmd->disp_attr & (E_RMD_DISPATTR_HIDE | E_RMD_DISPATTR_CULL)) {
		return;
	}

	n = pRmd->nb_grp;

	pGrp = pRmd->pGrp;
	for (i = 0; i < n; ++i) {
		if (!D_BIT_CK(pRmd->pCull, i) && !D_BIT_CK(pRmd->pHide, i)) {
			pBatch = RDR_get_batch();
			pBatch->vtx_prog = D_RDRPROG_vtx_solid_zbuf;
			pBatch->pix_prog = D_RDRPROG_pix_zbuf;
			pBatch->type = E_RDR_PRIMTYPE_TRILIST;
			pBatch->pVtx = pRmd->pVtx;
			pBatch->pIdx = pRmd->pIdx;
			pBatch->start = pGrp->start;
			pBatch->count = pGrp->count;

			pBatch->nb_param = 1;
			pParam = RDR_get_param(pBatch->nb_param);
			pBatch->pParam = pParam;

			pParam->count = 3;
			pParam->id.type = E_RDR_PARAMTYPE_FVEC;
			pParam->id.offs = D_RDR_GP_world;
			pParam->pVec = (UVEC*)&g_identity;
			++pParam;

			RDR_put_batch(pBatch, 0, E_RDR_LAYER_ZBUF);
		}
		++pGrp;
	}

	pGrp = pRmd->pGrp;
	for (i = 0; i < n; ++i) {
		if (!D_BIT_CK(pRmd->pCull, i) && !D_BIT_CK(pRmd->pHide, i)) {
			pBatch = RDR_get_batch();
			pBatch->vtx_prog = D_RDRPROG_vtx_solid;
			pBatch->pix_prog = D_RDRPROG_pix_toon;
			pBatch->type = E_RDR_PRIMTYPE_TRILIST;
			pBatch->pVtx = pRmd->pVtx;
			pBatch->pIdx = pRmd->pIdx;
			pBatch->start = pGrp->start;
			pBatch->count = pGrp->count;

			pBatch->nb_param = 7;
			pParam = RDR_get_param(pBatch->nb_param);
			pBatch->pParam = pParam;

			pParam->count = 3;
			pParam->id.type = E_RDR_PARAMTYPE_FVEC;
			pParam->id.offs = D_RDR_GP_world;
			pParam->pVec = (UVEC*)&g_identity;
			++pParam;

			pParam->count = 1;
			pParam->id.type = E_RDR_PARAMTYPE_FVEC;
			pParam->id.offs = D_RDR_GP_base_color;
			pParam->pVec = &pRmd->base_color;
			++pParam;

			pParam = MTL_apply(&pRmd->pMtl_lst->pMtl[pGrp->mtl_id], pParam);

			RDR_put_batch(pBatch, 0, E_RDR_LAYER_MTL0);
		}
		++pGrp;
	}

	pGrp = pRmd->pGrp;
	for (i = 0; i < n; ++i) {
		if (!D_BIT_CK(pRmd->pCull, i) && !D_BIT_CK(pRmd->pHide, i)) {
			pBatch = RDR_get_batch();
			pBatch->vtx_prog = D_RDRPROG_vtx_solid_recv;
			pBatch->pix_prog = D_RDRPROG_pix_recv;
			pBatch->type = E_RDR_PRIMTYPE_TRILIST;
			pBatch->pVtx = pRmd->pVtx;
			pBatch->pIdx = pRmd->pIdx;
			pBatch->start = pGrp->start;
			pBatch->count = pGrp->count;

			pBatch->blend_state.on = 1;
			pBatch->blend_state.src = E_RDR_BLENDMODE_SRCA;
			pBatch->blend_state.dst = E_RDR_BLENDMODE_INVSRCA;

			pBatch->draw_state.zwrite = 0;

			pBatch->nb_param = 1;
			pParam = RDR_get_param(pBatch->nb_param);
			pBatch->pParam = pParam;

			pParam->count = 3;
			pParam->id.type = E_RDR_PARAMTYPE_FVEC;
			pParam->id.offs = D_RDR_GP_world;
			pParam->pVec = (UVEC*)&g_identity;
			++pParam;

			RDR_put_batch(pBatch, 0, E_RDR_LAYER_RECV);
		}
		++pGrp;
	}
}

void ROOM_init(int id) {
	ROOM* pRoom = &g_room;
	memset(pRoom, 0, sizeof(ROOM));
	OBST_load(&pRoom->obst, "room/room.obs", "room/room.bvh");
	pRoom->pRmd[0] = RMD_load("room/room.rmd");
	CAM_load_data(&g_cam, "room/room.kfr", "room/room.lan");
}

void ROOM_free() {
	int i;
	ROOM* pRoom = &g_room;
	OBST_free(&pRoom->obst);
	for (i = 0; i < D_MAX_ROOM_MDL; ++i) {
		RMD_free(pRoom->pRmd[i]);
	}
	CAM_free_data(&g_cam);
}

void ROOM_cull(CAMERA* pCam) {
	int i;
	ROOM* pRoom = &g_room;
	for (i = 0; i < D_MAX_ROOM_MDL; ++i) {
		if (pRoom->pRmd[i]) {
			RMD_cull(pRoom->pRmd[i], pCam);
		}
	}
}

void ROOM_disp() {
	int i;
	ROOM* pRoom = &g_room;
	for (i = 0; i < D_MAX_ROOM_MDL; ++i) {
		if (pRoom->pRmd[i]) {
			RMD_disp(pRoom->pRmd[i]);
		}
	}
}

