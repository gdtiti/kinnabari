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
#include "render.h"
#include "material.h"

MTL_SYS g_mtl_sys;

void MTL_sys_init() {
	MTL_SYS* pSys = &g_mtl_sys;

	pSys->pName_pool = DICT_pool_create();
}

void MTL_sys_reset() {
	MTL_SYS* pSys = &g_mtl_sys;

	if (pSys->pName_pool) {
		DICT_pool_destroy(pSys->pName_pool);
		pSys->pName_pool = NULL;
	}
}

MTL_LIST* MTL_lst_create(MTL_INFO* pInfo, sys_ui32* pName_offs, void* pData_top, int n) {
	int i;
	MTL_LIST* pLst = NULL;
	MTL_SYS* pSys = &g_mtl_sys;

	pLst = SYS_malloc(D_ALIGN(sizeof(MTL_LIST), 16) + n*sizeof(MTL_INFO) + n*sizeof(MATERIAL));
	if (pLst) {
		pLst->nb_mtl = n;
		pLst->pName_dict = DICT_new();
		pLst->pInfo = (MTL_INFO*)D_INCR_PTR(pLst, D_ALIGN(sizeof(MTL_LIST), 16));
		pLst->pMtl = (MATERIAL*)&pLst->pInfo[n];
		memcpy(pLst->pInfo, pInfo, n*sizeof(MTL_INFO));
		for (i = 0; i < n; ++i) {
			pLst->pMtl[i].pInfo = &pLst->pInfo[i];
		}
		for (i = 0; i < n; ++i) {
			const char* pName = (const char*)D_INCR_PTR(pData_top, pName_offs[i]);
			pLst->pMtl[i].pName = DICT_pool_add(pSys->pName_pool, pName);
			DICT_put_p(pLst->pName_dict, pLst->pMtl[i].pName, &pLst->pMtl[i]);
		}
	}
	return pLst;
}


RDR_BATCH_PARAM* MTL_apply(MATERIAL* pMtl, RDR_BATCH_PARAM* pParam) {
	int n = 0;
	MTL_INFO* pInfo = pMtl->pInfo;

	pParam->count = 1;
	pParam->id.type = E_RDR_PARAMTYPE_FVEC;
	pParam->id.offs = D_RDR_GP_toon_sun_dir;
	if (pInfo->sun_dir.w == 0.0f) {
		pParam->pVec = RDR_get_val_v(1);
		pParam->pVec->qv = V4_normalize(pInfo->sun_dir.qv);
	} else {
		pParam->pVec = &RDR_get_view()->dir;
	}
	++pParam;

	pParam->count = 1;
	pParam->id.type = E_RDR_PARAMTYPE_FVEC;
	pParam->id.offs = D_RDR_GP_toon_sun_color;
	pParam->pVec = RDR_get_val_v(1);
	pParam->pVec->qv = V4_set_w1(V4_scale(pInfo->sun_color.qv, pInfo->sun_color.a));
	++pParam;

	pParam->count = 1;
	pParam->id.type = E_RDR_PARAMTYPE_FVEC;
	pParam->id.offs = D_RDR_GP_toon_sun_param;
	pParam->pVec = &pInfo->sun_param;
	++pParam;

	pParam->count = 1;
	pParam->id.type = E_RDR_PARAMTYPE_FVEC;
	pParam->id.offs = D_RDR_GP_toon_rim_color;
	pParam->pVec = &pInfo->rim_color;
	++pParam;

	pParam->count = 1;
	pParam->id.type = E_RDR_PARAMTYPE_FVEC;
	pParam->id.offs = D_RDR_GP_toon_rim_param;
	pParam->pVec = &pInfo->rim_param;
	++pParam;

	return pParam;
}

