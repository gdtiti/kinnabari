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
#include "anim.h"
#include "render.h"
#include "material.h"
#include "camera.h"
#include "model.h"

MDL_SYS g_mdl_sys;

void MDL_sys_init() {
	MDL_SYS* pSys = &g_mdl_sys;

	pSys->pName_pool = DICT_pool_create();
}

void MDL_sys_reset() {
	MDL_SYS* pSys = &g_mdl_sys;

	if (pSys->pName_pool) {
		DICT_pool_destroy(pSys->pName_pool);
		pSys->pName_pool = NULL;
	}
}

static void Set_jnt_info(OMD* pOmd, JNT_INFO* pJnt_info, OMD_JNT_INFO* pOmd_jnt, OMD_HEAD* pHead, sys_ui32* pName_offs) {
	MDL_SYS* pSys = &g_mdl_sys;
	const char* pName = (const char*)D_INCR_PTR(pHead, pName_offs[pOmd_jnt->id]);
	pJnt_info->offs_len.qv = V4_set(pOmd_jnt->offs.x, pOmd_jnt->offs.y, pOmd_jnt->offs.z, VEC_mag(pOmd_jnt->offs.v));
	pJnt_info->id = pOmd_jnt->id;
	pJnt_info->parent_id = pOmd_jnt->parent_id;
	pJnt_info->pName = DICT_pool_add(pSys->pName_pool, pName);
	DICT_put_p(pOmd->pName_dict, pJnt_info->pName, pJnt_info);
}

static void Vtx_cpy(RDR_VTX_GENERAL* pDst, OMD_VERTEX* pSrc) {
	int i;
	pDst->pos[0] = pSrc->pos.x;
	pDst->pos[1] = pSrc->pos.y;
	pDst->pos[2] = pSrc->pos.z;
	pDst->pos[3] = 1.0f;
	pDst->nrm[0] = pSrc->nrm.x;
	pDst->nrm[1] = pSrc->nrm.y;
	pDst->nrm[2] = pSrc->nrm.z;
	pDst->nrm[3] = 0.0f;
	pDst->clr[0] = 1.0f;
	pDst->clr[1] = 1.0f;
	pDst->clr[2] = 1.0f;
	pDst->clr[3] = 1.0f;
	pDst->uv0[0] = pSrc->tex.u;
	pDst->uv0[1] = pSrc->tex.v;
	pDst->uv0[2] = 0.0f;
	pDst->uv0[3] = 0.0f;
	for (i = 0; i < 4; ++i) {
		pDst->idx[i] = (float)pSrc->idx[i];
		pDst->wgt[i] = pSrc->wgt[i];
	}
}

OMD* OMD_load(const char* name) {
	int i, n, mem_size, cull_offs;
	OMD* pOmd = NULL;
	OMD_HEAD* pHead;
	MTX* pJnt_inv;
	JNT_INFO* pJnt_info;
	MTL_INFO* pMtl_info;
	sys_ui32* pMtl_name_offs;
	sys_ui32* pJnt_name_offs;
	OMD_JNT_INFO* pOmd_jnt;
	OMD_GROUP* pGrp_src;
	OMD_VERTEX* pVtx_src;
	OMD_CULL_HEAD* pCull;
	PRIM_GROUP* pGrp;
	sys_ui16* pIdx_src;

	pHead = (OMD_HEAD*)SYS_load(name);
	if (pHead) {
		mem_size = (int)(D_ALIGN(sizeof(OMD), 16)
	                     + D_ALIGN(pHead->nb_jnt*sizeof(JNT_INFO), 16)
	                     + pHead->nb_jnt*sizeof(MTX)
	                     + D_ALIGN(pHead->nb_grp*sizeof(PRIM_GROUP), 16));
		pCull = NULL;
		if (pHead->offs_cull) {
			cull_offs = mem_size;
			pCull = (OMD_CULL_HEAD*)D_INCR_PTR(pHead, pHead->offs_cull);
			mem_size += pCull->data_size;
		}
		pOmd = (OMD*)SYS_malloc(mem_size);
		memset(pOmd, 0, mem_size);
		pOmd->pName_dict = DICT_new();
		pJnt_info = (JNT_INFO*)D_INCR_PTR(pOmd, D_ALIGN(sizeof(OMD), 16));
		pJnt_inv = (MTX*)D_INCR_PTR(pJnt_info, D_ALIGN(pHead->nb_jnt*sizeof(JNT_INFO), 16));
		pGrp = (PRIM_GROUP*)(pJnt_inv + pHead->nb_jnt);
		pOmd->pJnt_info = pJnt_info;
		pOmd->pJnt_inv = pJnt_inv;
		pOmd->pGrp = pGrp;
		pOmd->nb_jnt = pHead->nb_jnt;
		pOmd->nb_grp = pHead->nb_grp;
		pMtl_info = (MTL_INFO*)(pHead + 1);
		pOmd_jnt = (OMD_JNT_INFO*)(pMtl_info + pHead->nb_mtl);
		pMtl_name_offs = (sys_ui32*)(pOmd_jnt + pHead->nb_jnt);
		pJnt_name_offs = pMtl_name_offs + pHead->nb_mtl;
		pOmd->pMtl_lst = MTL_lst_create(pMtl_info, pMtl_name_offs, pHead, pHead->nb_mtl);
		if (pCull) {
			pOmd->pCull = (OMD_CULL_HEAD*)D_INCR_PTR(pOmd, cull_offs);
			memcpy(pOmd->pCull, pCull, pCull->data_size);
		}

		n = pOmd->nb_jnt;
		for (i = 0; i < n; ++i) {
			Set_jnt_info(pOmd, pJnt_info, pOmd_jnt, pHead, pJnt_name_offs);
			++pJnt_info;
			++pOmd_jnt;
		}
		pJnt_info = pOmd->pJnt_info;
		for (i = 0; i < n; ++i) {
			if (pJnt_info->parent_id >= 0) {
				pJnt_info->pParent_info = &pOmd->pJnt_info[pJnt_info->parent_id];
			} else {
				pJnt_info->pParent_info = NULL;
			}
			++pJnt_info;
		}
		pJnt_info = pOmd->pJnt_info;
		for (i = 0; i < n; ++i) {
			MTX_unit(*pJnt_inv);
			V4_store((*pJnt_inv)[3], V4_set_w1(V4_scale(pJnt_info->offs_len.qv, -1.0f)));
			++pJnt_info;
			++pJnt_inv;
		}
		pJnt_info = pOmd->pJnt_info;
		pJnt_inv = pOmd->pJnt_inv;
		for (i = 0; i < n; ++i) {
			if (pJnt_info->parent_id >= 0) {
				MTX_mul(*pJnt_inv, *pJnt_inv, pOmd->pJnt_inv[pJnt_info->parent_id]);
			}
			++pJnt_info;
			++pJnt_inv;
		}

		pGrp_src = (OMD_GROUP*)D_INCR_PTR(pHead, pHead->offs_grp);
		pVtx_src = (OMD_VERTEX*)D_INCR_PTR(pHead, pHead->offs_vtx);
		pIdx_src = (sys_ui16*)D_INCR_PTR(pHead, pHead->offs_idx);
		n = pOmd->nb_grp;
		for (i = 0; i < n; ++i) {
			pGrp->base_color.qv = V4_set(pGrp_src->clr.r, pGrp_src->clr.g, pGrp_src->clr.b, 1.0f);
			pGrp->mtl_id = pGrp_src->mtl_id;
			pGrp->start = pGrp_src->start;
			pGrp->count = pGrp_src->count;
			++pGrp;
			++pGrp_src;
		}

		pOmd->pVtx = RDR_vtx_create(E_RDR_VTXTYPE_GENERAL, pHead->nb_vtx);
		if (pOmd->pVtx) {
			RDR_vtx_lock(pOmd->pVtx);
			if (pOmd->pVtx->locked.pData) {
				RDR_VTX_GENERAL* pVtx_dst = pOmd->pVtx->locked.pGen;
				n = pHead->nb_vtx;
				for (i = 0; i < n; ++i) {
					Vtx_cpy(pVtx_dst, pVtx_src);
					++pVtx_src;
					++pVtx_dst;
				}
				RDR_vtx_unlock(pOmd->pVtx);
			}
		}

		pOmd->pIdx = RDR_idx_create(E_RDR_IDXTYPE_16BIT, pHead->nb_idx);
		if (pOmd->pIdx) {
			RDR_idx_lock(pOmd->pIdx);
			if (pOmd->pIdx->pData) {
				memcpy(pOmd->pIdx->pData, pIdx_src, pHead->nb_idx*sizeof(sys_ui16));
				RDR_idx_unlock(pOmd->pIdx);
			}
		}

		SYS_free(pHead);
	}

	return pOmd;
}

void OMD_free(OMD* pOmd) {
	if (pOmd) {
		RDR_idx_release(pOmd->pIdx);
		RDR_vtx_release(pOmd->pVtx);
		MTL_lst_destroy(pOmd->pMtl_lst);
		DICT_destroy(pOmd->pName_dict);
		SYS_free(pOmd);
	}
}

JNT_INFO* OMD_get_jnt_info(OMD* pOmd, const char* name) {
	return (JNT_INFO*)DICT_get_p(pOmd->pName_dict, name);
}

int OMD_get_jnt_idx(OMD* pOmd, const char* name) {
	int idx = -1;
	JNT_INFO* pInfo = OMD_get_jnt_info(pOmd, name);
	if (pInfo) {
		idx = (int)(pInfo - pOmd->pJnt_info);
	}
	return idx;
}

MODEL* MDL_create(OMD* pOmd) {
	int i;
	int nb_jnt, nb_grp;
	int mem_size;
	JOINT* pJnt;
	MODEL* pMdl = NULL;

	nb_jnt = pOmd->nb_jnt;
	nb_grp = pOmd->nb_grp;
	mem_size = (int)(D_ALIGN(sizeof(MODEL), 16) + D_ALIGN(nb_jnt*sizeof(JOINT), 16) + nb_jnt*sizeof(MTX) + 2*D_BIT_ARY_SIZE32(nb_grp)*sizeof(sys_ui32));
	pMdl = (MODEL*)SYS_malloc(mem_size);
	memset(pMdl, 0, mem_size);
	MTX_unit(pMdl->root_mtx);
	pMdl->pOmd = pOmd;
	pMdl->pJnt = (JOINT*)D_INCR_PTR(pMdl, D_ALIGN(sizeof(MODEL), 16));
	pMdl->pJnt_wmtx = (MTX*)D_INCR_PTR(pMdl->pJnt, D_ALIGN(nb_jnt*sizeof(JOINT), 16));
	pMdl->pCull = (sys_ui32*)(pMdl->pJnt_wmtx + nb_jnt);
	pMdl->pHide = pMdl->pCull + D_BIT_ARY_SIZE32(nb_grp);
	pJnt = pMdl->pJnt;
	for (i = 0; i < nb_jnt; ++i) {
		pJnt->pInfo = &pOmd->pJnt_info[i];
		++pJnt;
	}
	MDL_jnt_reset(pMdl);
	MDL_calc_local(pMdl);
	MDL_calc_world(pMdl);
	return pMdl;
}

void MDL_destroy(MODEL* pMdl) {
	SYS_free(pMdl);
}

void MDL_jnt_reset(MODEL* pMdl) {
	int i, n;
	JOINT* pJnt;
	JNT_INFO* pInfo;

	n = pMdl->pOmd->nb_jnt;
	pJnt = pMdl->pJnt;
	for (i = 0; i < n; ++i) {
		pInfo = pJnt->pInfo;
		MTX_unit(pJnt->mtx);
		pJnt->pos.qv = V4_set_w1(pInfo->offs_len.qv);
		if (pInfo->parent_id < 0) {
			pJnt->pParent_mtx = &pMdl->root_mtx;
		} else {
			pJnt->pParent_mtx = &pMdl->pJnt_wmtx[pInfo->parent_id];
		}
		++pJnt;
	}
}

static void Calc_local(MTX* pMtx, UVEC* pPos, UVEC3* pRot) {
	QMTX m;
	MTX_rot_xyz(m, pRot->x, pRot->y, pRot->z);
	V4_store(m[3], V4_set_w1(pPos->qv));
	MTX_cpy(*pMtx, m);
}

void MDL_calc_local(MODEL* pMdl) {
	int i, n;
	JOINT* pJnt;

	Calc_local(&pMdl->root_mtx, &pMdl->pos, &pMdl->rot);
	n = pMdl->pOmd->nb_jnt;
	pJnt = pMdl->pJnt;
	for (i = 0; i < n; ++i) {
		Calc_local(&pJnt->mtx, &pJnt->pos, &pJnt->rot);
		++pJnt;
	}
}

void MDL_calc_world(MODEL* pMdl) {
	int i, n;
	JOINT* pJnt;
	MTX* pWmtx;

	n = pMdl->pOmd->nb_jnt;
	pJnt = pMdl->pJnt;
	pWmtx = pMdl->pJnt_wmtx;
	for (i = 0; i < n; ++i) {
		MTX_mul(*pWmtx, pJnt->mtx, *pJnt->pParent_mtx);
		++pJnt;
		++pWmtx;
	}
}

JOINT* MDL_get_jnt(MODEL* pMdl, const char* name) {
	JOINT* pJnt = NULL;
	int idx = OMD_get_jnt_idx(pMdl->pOmd, name);
	if (idx >= 0) {
		pJnt = &pMdl->pJnt[idx];
	}
	return pJnt;
}

static int Omd_cull_box(GEOM_AABB* pBox, CAMERA* pCam) {
	int flg = CAM_cull_box(pCam, pBox);
	if (!flg) {
		flg = CAM_cull_box_ex(pCam, pBox);
	}
	return flg;
}

static void Grp_cull(MODEL* pMdl, CAMERA* pCam, int grp_no, GEOM_SPHERE* pSph, int nb_jnt) {
	QVEC pos;
	QVEC rvec;
	GEOM_AABB box;
	int i, jnt_id;
	sys_byte* pJnt_id;

	pJnt_id = (sys_byte*)(pSph + nb_jnt);
	GEOM_aabb_init(&box);
	for (i = 0; i < nb_jnt; ++i) {
		jnt_id = *pJnt_id;
		pos = MTX_calc_qpnt(pMdl->pJnt_wmtx[jnt_id], pSph->qv);
		rvec = V4_fill(pSph->r);
		box.min.qv = V4_min(box.min.qv, V4_sub(pos, rvec));
		box.max.qv = V4_max(box.max.qv, V4_add(pos, rvec));
		++pSph;
		++pJnt_id;
	}
	box.min.qv = V4_set_w1(box.min.qv);
	box.max.qv = V4_set_w1(box.max.qv);
	if (Omd_cull_box(&box, pCam)) {
		D_BIT_ST(pMdl->pCull, grp_no);
	} else {
		D_BIT_CL(pMdl->pCull, grp_no);
	}
}

void MDL_cull(MODEL* pMdl, CAMERA* pCam) {
	int i, n;
	OMD_CULL_HEAD* pCull = pMdl->pOmd->pCull;
	if (!pCull) return;
	n = pMdl->pOmd->nb_grp;
	for (i = 0; i < n; ++i) {
		GEOM_SPHERE* pSph = (GEOM_SPHERE*)D_INCR_PTR(pCull, pCull->list[i].offs);
		int nb_jnt = pCull->list[i].nb_jnt;
		Grp_cull(pMdl, pCam, i, pSph, nb_jnt);
	}
}

void MDL_disp(MODEL* pMdl) {
	QMTX tm;
	UVEC* pSkin_mtx;
	OMD* pOmd;
	MTX* pJnt_wmtx;
	MTX* pJnt_inv;
	PRIM_GROUP* pGrp;
	RDR_BATCH* pBatch;
	RDR_BATCH_PARAM* pParam;
	int i, n, nb_jnt;

	pOmd = pMdl->pOmd;
	pJnt_wmtx = pMdl->pJnt_wmtx;
	pJnt_inv = pOmd->pJnt_inv;
	nb_jnt = pOmd->nb_jnt;
	pSkin_mtx = RDR_get_val_v(nb_jnt*3);
	for (i = 0; i < nb_jnt; ++i) {
		MTX_mul(tm, *pJnt_inv, *pJnt_wmtx);
		MTX_transpose(tm, tm);
		pSkin_mtx[i*3 + 0].qv = V4_load(tm[0]);
		pSkin_mtx[i*3 + 1].qv = V4_load(tm[1]);
		pSkin_mtx[i*3 + 2].qv = V4_load(tm[2]);
		++pJnt_inv;
		++pJnt_wmtx;
	}

	n = pOmd->nb_grp;

	pGrp = pOmd->pGrp;
	for (i = 0; i < n; ++i) {
		if (!D_BIT_CK(pMdl->pCull, i) && !D_BIT_CK(pMdl->pHide, i)) {
			pBatch = RDR_get_batch();
			pBatch->vtx_prog = D_RDRPROG_vtx_skin_zbuf;
			pBatch->pix_prog = D_RDRPROG_pix_zbuf;
			pBatch->type = E_RDR_PRIMTYPE_TRILIST;
			pBatch->pVtx = pOmd->pVtx;
			pBatch->pIdx = pOmd->pIdx;
			pBatch->start = pGrp->start;
			pBatch->count = pGrp->count;

			pBatch->blend_state.on = 0;
			pBatch->draw_state.cull = E_RDR_CULL_CCW;
			pBatch->draw_state.msaa = 0;
			pBatch->draw_state.zwrite = 1;

			pBatch->nb_param = 1;
			pParam = RDR_get_param(pBatch->nb_param);
			pBatch->pParam = pParam;

			pParam->count = nb_jnt*3;
			pParam->id.type = E_RDR_PARAMTYPE_FVEC;
			pParam->id.offs = D_RDR_GP_skin;
			pParam->pVec = pSkin_mtx;
			++pParam;

			RDR_put_batch(pBatch, 0, E_RDR_LAYER_ZBUF);
		}
		++pGrp;
	}

	pGrp = pOmd->pGrp;
	for (i = 0; i < n; ++i) {
		if (!D_BIT_CK(pMdl->pHide, i)) {
			pBatch = RDR_get_batch();
			pBatch->vtx_prog = D_RDRPROG_vtx_skin_cast;
			pBatch->pix_prog = D_RDRPROG_pix_cast;
			pBatch->type = E_RDR_PRIMTYPE_TRILIST;
			pBatch->pVtx = pOmd->pVtx;
			pBatch->pIdx = pOmd->pIdx;
			pBatch->start = pGrp->start;
			pBatch->count = pGrp->count;

			pBatch->blend_state.on = 0;
			pBatch->draw_state.cull = E_RDR_CULL_CCW;
			pBatch->draw_state.msaa = 0;

			pBatch->nb_param = 1;
			pParam = RDR_get_param(pBatch->nb_param);
			pBatch->pParam = pParam;

			pParam->count = nb_jnt*3;
			pParam->id.type = E_RDR_PARAMTYPE_FVEC;
			pParam->id.offs = D_RDR_GP_skin;
			pParam->pVec = pSkin_mtx;
			++pParam;

			RDR_put_batch(pBatch, 0, E_RDR_LAYER_CAST);
		}
		++pGrp;
	}

	pGrp = pOmd->pGrp;
	for (i = 0; i < n; ++i) {
		if (!D_BIT_CK(pMdl->pCull, i) && !D_BIT_CK(pMdl->pHide, i)) {
			pBatch = RDR_get_batch();
			pBatch->vtx_prog = D_RDRPROG_vtx_skin;
			pBatch->pix_prog = D_RDRPROG_pix_toon;
			pBatch->type = E_RDR_PRIMTYPE_TRILIST;
			pBatch->pVtx = pOmd->pVtx;
			pBatch->pIdx = pOmd->pIdx;
			pBatch->start = pGrp->start;
			pBatch->count = pGrp->count;

			pBatch->draw_state.cull = E_RDR_CULL_CCW;
			pBatch->draw_state.zwrite = 0;

			pBatch->nb_param = 7;
			pParam = RDR_get_param(pBatch->nb_param);
			pBatch->pParam = pParam;

			pParam->count = nb_jnt*3;
			pParam->id.type = E_RDR_PARAMTYPE_FVEC;
			pParam->id.offs = D_RDR_GP_skin;
			pParam->pVec = pSkin_mtx;
			++pParam;

			pParam->count = 1;
			pParam->id.type = E_RDR_PARAMTYPE_FVEC;
			pParam->id.offs = D_RDR_GP_base_color;
			pParam->pVec = &pGrp->base_color;
			++pParam;

			pParam = MTL_apply(&pOmd->pMtl_lst->pMtl[pGrp->mtl_id], pParam);

			RDR_put_batch(pBatch, 0, E_RDR_LAYER_MTL0);
		}
		++pGrp;
	}
}

