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

#define D_BIT_ARY_SIZE32(_n) ((((_n)-1)/(sizeof(sys_ui32)*8))+1)
#define D_BIT_ARY(_name, _n) sys_ui32 _name[D_BIT_ARY_SIZE32(_n)]
#define D_BIT_IDX(_no) ((_no)/(sizeof(sys_ui32)*8))
#define D_BIT_MASK(_no) (1 << ((_no)&((sizeof(sys_ui32)*8)-1)))
#define D_BIT_ST(_ary, _no) (_ary)[D_BIT_IDX(_no)] |= D_BIT_MASK(_no)
#define D_BIT_CL(_ary, _no) (_ary)[D_BIT_IDX(_no)] &= ~D_BIT_MASK(_no)
#define D_BIT_CK(_ary, _no) !!((_ary)[D_BIT_IDX(_no)] & D_BIT_MASK(_no))
#define D_BIT_SW(_ary, _no) (_ary)[D_BIT_IDX(_no)] ^= D_BIT_MASK(_no)


typedef struct _SYM_POOL SYM_POOL;

struct _SYM_POOL {
	sys_int   size;
	sys_int   offs;
	SYM_POOL* pNext;
};

typedef union _DICT_VAL {
	void*   pVal;
	sys_int iVal;
} DICT_VAL;

typedef struct _DICT_SLOT {
	char*    pKey;
	DICT_VAL val;
	sys_i32  hash_mix;
} DICT_SLOT;

typedef struct _DICT {
	DICT_SLOT* pTbl;
	sys_int    in_use;
	float      load_factor;
	sys_int    threshold;
	sys_int    size;
	struct _DICT_ATTR {
		sys_ui32 copy_keys : 1; /* off by default */
	} attr;
} DICT;

typedef void (*DICT_FUNC)(DICT* pDict, const char* pKey, DICT_VAL val, void* pData);

D_EXTERN_FUNC SYM_POOL* DICT_pool_create(void);
D_EXTERN_FUNC void DICT_pool_destroy(SYM_POOL* pSelf);
D_EXTERN_FUNC sys_int DICT_pool_get_free(SYM_POOL* pSelf);
D_EXTERN_FUNC const char* DICT_pool_add(SYM_POOL* pSelf, const char* sym);

D_EXTERN_FUNC DICT* DICT_create(sys_int capacity, float load_factor);
D_EXTERN_FUNC DICT* DICT_new(void);
D_EXTERN_FUNC void DICT_destroy(DICT* pDict);
D_EXTERN_FUNC void DICT_set_copy_keys(DICT* pDict, int on_off);
D_EXTERN_FUNC DICT_VAL DICT_get(DICT* pDict, const char* pKey);
D_EXTERN_FUNC sys_int DICT_get_i(DICT* pDict, const char* pKey);
D_EXTERN_FUNC void* DICT_get_p(DICT* pDict, const char* pKey);
D_EXTERN_FUNC void DICT_remove(DICT* pDict, const char* pKey);
D_EXTERN_FUNC char* DICT_put(DICT* pDict, const char* pKey, DICT_VAL val);
D_EXTERN_FUNC char* DICT_put_i(DICT* pDict, const char* pKey, sys_int iVal);
D_EXTERN_FUNC char* DICT_put_p(DICT* pDict, const char* pKey, void* pVal);
D_EXTERN_FUNC char* DICT_add(DICT* pDict, const char* pKey, DICT_VAL val);
D_EXTERN_FUNC char* DICT_add_i(DICT* pDict, const char* pKey, sys_int iVal);
D_EXTERN_FUNC char* DICT_add_p(DICT* pDict, const char* pKey, void* pVal);
D_EXTERN_FUNC void DICT_foreach(DICT* pDict, DICT_FUNC func, void* pData);

D_EXTERN_FUNC void CFG_init(void);
D_EXTERN_FUNC const char* CFG_get(const char* pName);
