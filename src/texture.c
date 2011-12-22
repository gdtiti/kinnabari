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
#include "texture.h"

static TEX_EXT_INFO* Tpk_get_ext_info(TPK_HEAD* pTpk) {
	TEX_EXT_INFO* pInfo = NULL;
	if (pTpk && pTpk->ext_info_offs) {
		pInfo = (TEX_EXT_INFO*)D_INCR_PTR(pTpk, pTpk->ext_info_offs);
	}
	return pInfo;
}

static sys_ui32* Tpk_get_name_list(TPK_HEAD* pTpk) {
	sys_ui32* pList = NULL;
	TEX_EXT_INFO* pInfo = Tpk_get_ext_info(pTpk);
	if (pInfo && pInfo->ext_head_size && pInfo->tex_name_offs) {
		pList = (sys_ui32*)D_INCR_PTR(pTpk, pInfo->tex_name_offs);
	}
	return pList;
}

static int Tpk_get_name_data_size(TPK_HEAD* pTpk) {
	int size = 0;
	sys_ui32* pList = Tpk_get_name_list(pTpk);
	if (pList) {
		int i;
		int n = pTpk->nb_tex;
		size = n * sizeof(sys_ui32);
		for (i = 0; i < n; ++i) {
			char* pName = (char*)D_INCR_PTR(pTpk, pList[i]);
			size += (int)strlen(pName) + 1;
		}
	}
	return size;
}

static void Tex_init(TEX_PACKAGE* pPkg, TPK_HEAD* pTpk) {
	int i, n;
	if (!pTpk) return;
	n = pTpk->nb_tex;
	for (i = 0; i < n; ++i) {
		sys_ui32 fmt = 0;
		TEX_HEAD* pHead = (TEX_HEAD*)(pTpk + 1) + i;
		switch (pHead->fmt) {
			case 1:
				fmt = D_FOURCC('D','X','T','1');
				break;
			case 2:
				fmt = D_FOURCC('D','X','T','3');
				break;
			case 3:
				fmt = D_FOURCC('D','X','T','5');
				break;
			default:
				break;
		}
		pPkg->pInfo[i].pTex = RDR_tex_create(pTpk, pHead->width, pHead->height, pHead->depth, pHead->nb_lvl, fmt, (sys_ui32*)D_INCR_PTR(pTpk, pHead->list_offs));
	}
}

TEX_PACKAGE* TEX_load(const char* name) {
	TEX_PACKAGE* pPkg = NULL;
	TPK_HEAD* pTpk;
	pTpk = (TPK_HEAD*)SYS_load(name);
	if (pTpk && pTpk->magic == D_FOURCC('T','P','K',0)) {
		int i;
		int n = pTpk->nb_tex;
		int size = sizeof(TEX_PACKAGE) + n*sizeof(TEX_INFO);
		int name_size = Tpk_get_name_data_size(pTpk);
		size += name_size;
		pPkg = (TEX_PACKAGE*)SYS_malloc(size);
		pPkg->nb_tex = pTpk->nb_tex;
		pPkg->pInfo = (TEX_INFO*)(pPkg+1);
		pPkg->pName = name_size ? (sys_ui32*)&pPkg->pInfo[n] : NULL;
		for (i = 0; i < n; ++i) {
			pPkg->pInfo[i].pTex = NULL;
			pPkg->pInfo[i].pPkg = pPkg;
			pPkg->pInfo[i].idx = i;
		}
		if (pPkg->pName) {
			sys_ui32* pList = Tpk_get_name_list(pTpk);
			char* pName_dst = (char*)&pPkg->pName[n];
			for (i = 0; i < n; ++i) {
				char* pName_src = (char*)D_INCR_PTR(pTpk, pList[i]);
				int len = (int)strlen(pName_src) + 1;
				memcpy(pName_dst, pName_src, len);
				pPkg->pName[i] = (sys_ui32)(pName_dst - (char*)pPkg->pName);
				pName_dst += len;
			}
		}
		Tex_init(pPkg, pTpk);
		SYS_free(pTpk);
	}
	return pPkg;
}

const char* TEX_get_name(TEX_PACKAGE* pPkg, int tex_no) {
	const char* pName = NULL;
	sys_ui32* pList = pPkg->pName;
	if (pList && (sys_uint)tex_no < pPkg->nb_tex && pList[tex_no]) {
		pName = (const char*)D_INCR_PTR(pList, pList[tex_no]);
	}
	return pName;
}

int TEX_get_idx(TEX_PACKAGE* pPkg, const char* name) {
	int idx = -1;
	sys_ui32* pList = pPkg->pName;
	if (pList) {
		int i;
		int n = pPkg->nb_tex;
		for (i = 0; i < n; ++i) {
			if (*pList) {
				char* pName = (char*)D_INCR_PTR(pPkg->pName, *pList);
				if (0 == strcmp(pName, name)) {
					idx = i;
					break;
				}
			}
			++pList;
		}
	}
	return idx;
}

static void Tex_release(TEX_PACKAGE* pPkg) {
	int i, n;
	n = pPkg->nb_tex;
	for (i = 0; i < n; ++i) {
		RDR_tex_release(pPkg->pInfo[i].pTex);
	}
}

void TEX_free(TEX_PACKAGE* pPkg) {
	if (pPkg) {
		Tex_release(pPkg);
		SYS_free(pPkg);
	}
}


