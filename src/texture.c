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

TEX_PACKAGE* TEX_load(const char* name) {
	TEX_PACKAGE* pPkg = NULL;
	TPK_HEAD* pHead;
	pHead = (TPK_HEAD*)SYS_load(name);
	if (pHead && pHead->magic == D_FOURCC('T','P','K',0)) {
		int i;
		int n = pHead->nb_tex;
		int size = sizeof(TEX_PACKAGE) + n*sizeof(TEX_INFO);
		pPkg = (TEX_PACKAGE*)SYS_malloc(size);
		pPkg->pData = pHead;
		pPkg->pList = (TEX_INFO*)(pPkg+1);
		for (i = 0; i < n; ++i) {
			pPkg->pList[i].pTex = NULL;
			pPkg->pList[i].pPkg = pPkg;
			pPkg->pList[i].idx = i;
		}
	}
	return pPkg;
}

void TEX_free(TEX_PACKAGE* pPkg) {
	TEX_release(pPkg);
	if (pPkg) {
		SYS_free(pPkg->pData);
		SYS_free(pPkg->pList);
		SYS_free(pPkg);
	}
}

void TEX_init(TEX_PACKAGE* pPkg) {
	int i, n;
	if (!pPkg) return;
	n = pPkg->pData->nb_tex;
	for (i = 0; i < n; ++i) {
		sys_ui32 fmt = 0;
		TEX_HEAD* pHead = (TEX_HEAD*)(pPkg->pData + 1) + i;
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
		pPkg->pList[i].pTex = RDR_tex_create(pPkg->pData, pHead->width, pHead->height, pHead->depth, pHead->nb_lvl, fmt, (sys_ui32*)D_INCR_PTR(pPkg->pData, pHead->list_offs));
	}
}

void TEX_release(TEX_PACKAGE* pPkg) {
	int i, n;
	if (!pPkg) return;
	n = pPkg->pData->nb_tex;
	for (i = 0; i < n; ++i) {
		RDR_tex_release(pPkg->pList[i].pTex);
	}
}

