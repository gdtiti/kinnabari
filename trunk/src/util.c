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

#include <stdlib.h>
#include <string.h>

#include "system.h"
#include "calc.h"
#include "util.h"

#define D_SYMPOOL_DEF_SIZE (1024)
#define D_DICT_MAX_INT32 ((sys_i32)(((sys_ui32)-1) >> 1))
#define D_DICT_CHAIN_MARKER (~D_DICT_MAX_INT32)
#define D_DICT_HSTEP(_hash, _size) ((((sys_uint)((_hash) >> 5) + 1) % ((_size) - 1)) + 1);
static const char* s_dict_removed = "X";
#define D_DICT_REMOVED_MARKER ((char*)s_dict_removed)

static sys_int s_dict_prime_tbl[] = {
	11, 19, 37, 73, 109, 163, 251, 367, 557, 823,
	1237, 1861, 2777, 4177, 6247, 9371, 14057, 21089,
	31627, 47431, 71143, 106721, 160073, 240101, 360163,
	540217, 810343, 1215497, 1823231, 2734867, 4102283,
	6153409, 9230113, 13845163
};

static struct _CFG_WK {
	SYM_POOL* pSym;
	DICT*     pDict;
} s_cfg_wk = {NULL, NULL};


static int Dict_prime_ck(sys_int x) {
	sys_int n;
	if (x & 1) {
		int lim = (int)sqrt(x);
		for (n = 3; n < lim; n += 2) {
			if ((x % n) == 0) return 0;
		}
		return 1;
	}
	return !!(x == 2);
}

static sys_int Dict_calc_prime(sys_int x) {
	sys_int i;
	for (i = (x & (~1))-1; i < D_DICT_MAX_INT32; i += 2) {
		if (Dict_prime_ck(i)) return i;
	}
	return x;
}

static sys_int Dict_get_prime(sys_int x) {
	sys_int i;
	for (i = 0; i < D_ARRAY_LENGTH(s_dict_prime_tbl); ++i) {
		if (x <= s_dict_prime_tbl[i]) return s_dict_prime_tbl[i];
	}
	return Dict_calc_prime(x);
}

static sys_int Dict_calc_hash(const char* s) {
	sys_int h = 0;
	char c;
	if (s) {
		while (0 != (c=*s++)) {
			h += c << 5;
			h -= c;
		}
	}
	return h;
}


SYM_POOL* DICT_pool_create() {
	SYM_POOL* pSelf = (SYM_POOL*)SYS_malloc(sizeof(SYM_POOL) + D_SYMPOOL_DEF_SIZE);
	pSelf->size = D_SYMPOOL_DEF_SIZE;
	pSelf->offs = 0;
	pSelf->pNext = NULL;
	return pSelf;
}



void DICT_pool_destroy(SYM_POOL* pSelf) {
	SYM_POOL* p = pSelf;
	SYM_POOL* pNext;
	while (p) {
		pNext = p->pNext;
		SYS_free(p);
		p = pNext;
	}
}

sys_int DICT_pool_get_free(SYM_POOL* pSelf) {
	return (pSelf->size - pSelf->offs);
}

const char* DICT_pool_add(SYM_POOL* pSelf, const char* sym) {
	char* res = NULL;
	SYM_POOL* pPool = pSelf;
	sys_int len, size;
	if (pSelf && sym) {
		len = (sys_int)strlen(sym) + 1;
		while (!res) {
			if (DICT_pool_get_free(pPool) >= len) {
				res = (char*)(pPool + 1) + pPool->offs;
				memcpy(res, sym, len);
				pPool->offs += len;
			} else {
				if (!pPool->pNext) {
					size = len > D_SYMPOOL_DEF_SIZE ? len : D_SYMPOOL_DEF_SIZE;
					pPool->pNext = (SYM_POOL*)SYS_malloc(sizeof(SYM_POOL)+size);
					pPool->pNext->offs = 0;
					pPool->pNext->size = size;
					pPool->pNext->pNext = NULL;
				}
				pPool = pPool->pNext;
			}
		}
	}
	return res;
}

static void Dict_adj_threshold(DICT* pDict) {
	pDict->threshold = (sys_int)(pDict->size * pDict->load_factor);
	if (pDict->threshold >= pDict->size) pDict->threshold = pDict->size-1;
}

static DICT_SLOT* Dict_alloc_tbl(sys_uint size) {
	sys_uint len = sizeof(DICT_SLOT) * size;
	DICT_SLOT* pTbl = (DICT_SLOT*)SYS_malloc(len);
	if (pTbl) memset(pTbl, 0, len);
	return pTbl;
}

static void Dict_set_tbl(DICT* pDict, DICT_SLOT* pTbl) {
	if (pDict->pTbl) SYS_free(pDict->pTbl);
	pDict->pTbl = pTbl;
	Dict_adj_threshold(pDict);
}

DICT* DICT_create(sys_int capacity, float load_factor) {
	DICT* pDict;
	sys_uint mem_size = (sys_uint)D_ALIGN(sizeof(DICT), 16);
	pDict = (DICT*)SYS_malloc(mem_size);
	if (capacity <= 0) capacity = 1;
	pDict->load_factor = 0.75f*load_factor;
	pDict->size = Dict_get_prime((sys_int)(capacity / pDict->load_factor));
	pDict->in_use = 0;
	pDict->pTbl = NULL;
	Dict_set_tbl(pDict, Dict_alloc_tbl(pDict->size));
	pDict->attr.copy_keys = 0;
	return pDict;
}

DICT* DICT_new() {
	return DICT_create(0, 1.0f);
}

void DICT_destroy(DICT* pDict) {
	sys_int i;
	if (pDict) {
		if (pDict->pTbl) {
			if (pDict->attr.copy_keys) {
				for (i = 0; i < pDict->size; ++i) {
					if (pDict->pTbl[i].pKey && pDict->pTbl[i].pKey != D_DICT_REMOVED_MARKER) {
						SYS_free(pDict->pTbl[i].pKey);
					}
				}
			}
			SYS_free(pDict->pTbl);
		}
		SYS_free(pDict);
	}
}

void DICT_set_copy_keys(DICT* pDict, int on_off) {
	pDict->attr.copy_keys = !!on_off;
}

static void Dict_rehash(DICT* pDict) {
	sys_int i, h, n;
	sys_uint j, spot, step;
	sys_uint old_size = pDict->size;
	sys_uint new_size = (sys_uint)Dict_get_prime((old_size << 1) | 1);
	DICT_SLOT* pNew_tbl = Dict_alloc_tbl(new_size);
	n = pDict->size;
	for (i = 0; i < n; ++i) {
		if (pDict->pTbl[i].pKey) {
			h = pDict->pTbl[i].hash_mix & D_DICT_MAX_INT32;
			spot = (sys_uint)h;
			step = D_DICT_HSTEP(h, new_size);
			for (j = spot % new_size; ;spot += step, j = spot % new_size) {
				if (pNew_tbl[j].pKey) {
					pNew_tbl[j].hash_mix |= D_DICT_CHAIN_MARKER;
				} else {
					pNew_tbl[j].pKey = pDict->pTbl[i].pKey;
					pNew_tbl[j].val = pDict->pTbl[i].val;
					pNew_tbl[j].hash_mix |= h;
					break;
				}
			}
		}
	}
	pDict->size = new_size;
	Dict_set_tbl(pDict, pNew_tbl);
}

static char* Dict_mk_key(DICT* pDict, const char* pKey) {
	char* p = NULL;
	if (pDict->attr.copy_keys) {
		sys_int len = (sys_int)strlen(pKey)+1;
		p = (char*)SYS_malloc(len);
		memcpy(p, pKey, len);
	} else {
		p = (char*)pKey;
	}
	return p;
}

static int Dict_key_eq(const char* k1, const char* k2) {
	return (k1 == k2 || 0 == strcmp(k1, k2));
}

static char* Dict_put_impl(DICT* pDict, const char* pKey, DICT_VAL val, int overwrite) {
	sys_int h, free_idx, idx;
	sys_uint i, size, spot, step;
	DICT_SLOT* pEntry;

	size = (sys_uint)pDict->size;
	if (pDict->in_use >= pDict->threshold) {
		Dict_rehash(pDict);
		size = (sys_uint)pDict->size;
	}

	h = Dict_calc_hash(pKey) & D_DICT_MAX_INT32;
	spot = (sys_uint)h;
	step = D_DICT_HSTEP(h, size);

	free_idx = -1;
	for (i = 0; i < size; ++i) {
		idx = (sys_int)(spot % size);
		pEntry = &pDict->pTbl[idx];
		if (free_idx == -1
		    && pEntry->pKey == D_DICT_REMOVED_MARKER
			&& (pEntry->hash_mix & D_DICT_CHAIN_MARKER) != 0) free_idx = idx;

		if (!pEntry->pKey ||
			(pEntry->pKey == D_DICT_REMOVED_MARKER &&
			(pEntry->hash_mix & D_DICT_CHAIN_MARKER) == 0)) {
					if (free_idx == -1) free_idx = idx;
					break;
		}

		if ((pEntry->hash_mix & D_DICT_MAX_INT32) == h && Dict_key_eq(pKey, pEntry->pKey)) {
			if (overwrite) {
				pDict->pTbl[idx].val = val;
				return pEntry->pKey;
			} else {
				return (char*)NULL;
			}
		}

		if (free_idx == -1) {
			pDict->pTbl[idx].hash_mix |= D_DICT_CHAIN_MARKER;
		}

		spot += step;
	}

	if (free_idx != -1) {
		pDict->pTbl[free_idx].pKey = Dict_mk_key(pDict, pKey);;
		pDict->pTbl[free_idx].val = val;
		pDict->pTbl[free_idx].hash_mix |= h;
		++pDict->in_use;
	}

	return pDict->pTbl[free_idx].pKey;
}

static sys_int Dict_find(DICT* pDict, const char* pKey) {
	sys_int h, idx;
	sys_uint i, size, spot, step;
	DICT_SLOT* pEntry;

	size = (sys_uint)pDict->size;
	h = Dict_calc_hash(pKey) & D_DICT_MAX_INT32;
	spot = (sys_uint)h;
	step = D_DICT_HSTEP(h, size);

	for (i = 0; i < size; ++i) {
		idx = (sys_int)(spot % size);
		pEntry = &pDict->pTbl[idx];
		if (!pEntry->pKey) return -1;
		if ((pEntry->hash_mix & D_DICT_MAX_INT32) == h && Dict_key_eq(pKey, pEntry->pKey)) {
			return idx;
		}
		if (0 == (pEntry->hash_mix & D_DICT_CHAIN_MARKER)) return -1;
		spot += step;
	}

	return -1;
}

DICT_VAL DICT_get(DICT* pDict, const char* pKey) {
	DICT_VAL val;
	sys_int i = Dict_find(pDict, pKey);
	val.iVal = 0;
	if (i >= 0) val = pDict->pTbl[i].val;
	return val;
}

sys_int DICT_get_i(DICT* pDict, const char* pKey) {
	return DICT_get(pDict, pKey).iVal;
}

void* DICT_get_p(DICT* pDict, const char* pKey) {
	return DICT_get(pDict, pKey).pVal;
}

void DICT_remove(DICT* pDict, const char* pKey) {
	sys_int h;
	sys_int i = Dict_find(pDict, pKey);
	if (i >= 0) {
		h = pDict->pTbl[i].hash_mix;
		h &= D_DICT_CHAIN_MARKER;
		pDict->pTbl[i].hash_mix = h;
		pDict->pTbl[i].pKey = h ? D_DICT_REMOVED_MARKER : (char*)NULL;
		pDict->pTbl[i].val.pVal = NULL;
		--pDict->in_use;
	}
}

char* DICT_put(DICT* pDict, const char* pKey, DICT_VAL val) {
	return Dict_put_impl(pDict, pKey, val, 1);
}

char* DICT_put_i(DICT* pDict, const char* pKey, sys_int iVal) {
	DICT_VAL val;
	val.iVal = iVal;
	return DICT_put(pDict, pKey, val);
}

char* DICT_put_p(DICT* pDict, const char* pKey, void* pVal) {
	DICT_VAL val;
	val.pVal = pVal;
	return DICT_put(pDict, pKey, val);
}

char* DICT_add(DICT* pDict, const char* pKey, DICT_VAL val) {
	return Dict_put_impl(pDict, pKey, val, 0);
}

char* DICT_add_i(DICT* pDict, const char* pKey, sys_int iVal) {
	DICT_VAL val;
	val.iVal = iVal;
	return DICT_add(pDict, pKey, val);
}

char* DICT_add_p(DICT* pDict, const char* pKey, void* pVal) {
	DICT_VAL val;
	val.pVal = pVal;
	return DICT_add(pDict, pKey, val);
}

void DICT_foreach(DICT* pDict, DICT_FUNC func, void* pData) {
	sys_int i, n;
	if (!pDict || !func) return;
	n = pDict->size;
	for (i = 0; i < n; ++i) {
		if (pDict->pTbl[i].pKey && pDict->pTbl[i].pKey != D_DICT_REMOVED_MARKER) {
			func(pDict, (const char*)pDict->pTbl[i].pKey, pDict->pTbl[i].val, pData);
		}
	}
}


void CFG_init(const char* fname) {
	FILE* f;
	char buff[512];
	char name[256];
	char val[256];
	const char* pName;
	const char* pVal;

	fopen_s(&f, fname, "r");
	if (!f) return;
	s_cfg_wk.pSym = DICT_pool_create();
	s_cfg_wk.pDict = DICT_new();
	while (1) {
		if (NULL == fgets(buff, sizeof(buff), f)) break;
		if (buff[0] == '#') continue;
		sscanf_s(buff, "%[_.a-z0-9]=%[-_./a-z0-9]", name, sizeof(name), val, sizeof(val));
		pName = DICT_pool_add(s_cfg_wk.pSym, (const char*)name);
		pVal = DICT_pool_add(s_cfg_wk.pSym, (const char*)val);
		DICT_put_p(s_cfg_wk.pDict, pName, (void*)pVal);
		SYS_log("%s:%s\n", pName, pVal);
	}
	fclose(f);
}

const char* CFG_get(const char* pName) {
	const char* pVal = NULL;
	if (s_cfg_wk.pDict) {
		pVal = (const char*)DICT_get_p(s_cfg_wk.pDict, pName);
	}
	return pVal;
}


UTL_GEOMETRY* GMT_load(const char* fname) {
	GMT_HEAD* pHead;
	UTL_GEOMETRY* pGeo = NULL;

	pHead = SYS_load(fname);
	if (pHead && pHead->magic == D_FOURCC('G','M','T','\0')) {
		pGeo = (UTL_GEOMETRY*)SYS_malloc(sizeof(UTL_GEOMETRY));
		pGeo->pData = pHead;
	}
	return pGeo;
}

void GMT_free(UTL_GEOMETRY* pGeo) {
	if (pGeo) {
		SYS_free(pGeo->pData);
		SYS_free(pGeo);
	}
}

UVEC* GMT_get_pnt(UTL_GEOMETRY* pGeo, int i) {
	if (!pGeo || !pGeo->pData) return NULL;
	if ((sys_uint)i >= pGeo->pData->nb_pnt) return NULL;
	return (UVEC*)D_INCR_PTR(pGeo->pData, pGeo->pData->offs_pnt) + i;
}

GMT_POLY* GMT_get_pol(UTL_GEOMETRY* pGeo, int i) {
	sys_ui32* pOffs;
	if (!pGeo || !pGeo->pData) return NULL;
	if ((sys_uint)i >= pGeo->pData->nb_pol) return NULL;
	pOffs = (sys_ui32*)D_INCR_PTR(pGeo->pData, pGeo->pData->offs_pol);
	return (GMT_POLY*)D_INCR_PTR(pOffs, pOffs[i]);
}

const char* GMT_get_str(UTL_GEOMETRY* pGeo, int offs) {
	if (!pGeo || !pGeo->pData) return NULL;
	return (const char*)D_INCR_PTR(pGeo->pData, pGeo->pData->offs_str + offs);
}

GMT_ATTR_INFO* GMT_get_attr_info_glb(UTL_GEOMETRY* pGeo) {
	if (!pGeo || !pGeo->pData) return NULL;
	return (GMT_ATTR_INFO*)D_INCR_PTR(pGeo->pData, pGeo->pData->offs_glb_attr);
}

GMT_ATTR_INFO* GMT_get_attr_info_pnt(UTL_GEOMETRY* pGeo) {
	if (!pGeo || !pGeo->pData) return NULL;
	return (GMT_ATTR_INFO*)D_INCR_PTR(pGeo->pData, pGeo->pData->offs_pnt_attr);
}

GMT_ATTR_INFO* GMT_get_attr_info_pol(UTL_GEOMETRY* pGeo) {
	if (!pGeo || !pGeo->pData) return NULL;
	return (GMT_ATTR_INFO*)D_INCR_PTR(pGeo->pData, pGeo->pData->offs_pol_attr);
}

void* GMT_get_attr_val_glb(UTL_GEOMETRY* pGeo, GMT_ATTR_INFO* pInfo) {
	void* pVal = NULL;
	GMT_ATTR_INFO* pTop = GMT_get_attr_info_glb(pGeo);
	if (pInfo && pTop) {
		int n = pGeo->pData->nb_glb_attr;
		int size = pTop[n-1].offs + pTop[n-1].size*4;
		pVal = D_INCR_PTR(&pTop[n], pInfo->offs);
	}
	return pVal;
}

void* GMT_get_attr_val_pnt(UTL_GEOMETRY* pGeo, GMT_ATTR_INFO* pInfo, int pnt_id) {
	void* pVal = NULL;
	GMT_ATTR_INFO* pTop = GMT_get_attr_info_pnt(pGeo);
	if (pInfo && pTop) {
		int n = pGeo->pData->nb_pnt_attr;
		int size = pTop[n-1].offs + pTop[n-1].size*4;
		pVal = D_INCR_PTR(&pTop[n], size*pnt_id + pInfo->offs);
	}
	return pVal;
}

void* GMT_get_attr_val_pol(UTL_GEOMETRY* pGeo, GMT_ATTR_INFO* pInfo, int pol_id) {
	void* pVal = NULL;
	GMT_ATTR_INFO* pTop = GMT_get_attr_info_pol(pGeo);
	if (pInfo && pTop) {
		int n = pGeo->pData->nb_pol_attr;
		int size = pTop[n-1].offs + pTop[n-1].size*4;
		pVal = D_INCR_PTR(&pTop[n], size*pol_id + pInfo->offs);
	}
	return pVal;
}


float UTL_frand01() {
	union {sys_ui32 u32; float f32;} d32;
	sys_uint r0 = rand() & 0xFF;
	sys_uint r1 = rand() & 0xFF;
	sys_uint r2 = rand() & 0xFF;
	d32.f32 = 1.0f;
	d32.u32 += (r0 | (r1 << 8) | ((r2 & 0x7F) << 16));
	d32.f32 -= 1.0f;
	return d32.f32;
}

float UTL_frand_11() {
	float r = UTL_frand01() - 0.5f;
	return (r + r);
}

