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


typedef enum _E_GMTATTRTYPE {
	E_GMTATTRTYPE_INT    = 0,
	E_GMTATTRTYPE_FLOAT  = 1,
	E_GMTATTRTYPE_STRING = 2
} E_GMTATTRTYPE;

typedef struct _GMT_HEAD {
	sys_ui32  magic;
	sys_ui32  nb_glb_attr;
	sys_ui32  nb_pnt_attr;
	sys_ui32  nb_pol_attr;
	sys_ui32  nb_pnt;
	sys_ui32  nb_pol;
	sys_ui32  offs_glb_attr;
	sys_ui32  offs_pnt_attr;
	sys_ui32  offs_pol_attr;
	sys_ui32  offs_pnt;
	sys_ui32  offs_pol;
	sys_ui32  offs_str;
	GEOM_AABB bbox;
} GMT_HEAD;

typedef struct _GMT_ATTR_INFO {
	sys_ui32 name;
	sys_ui32 offs;
	sys_ui16 type;
	sys_ui16 size;
} GMT_ATTR_INFO;

typedef struct _GMT_POLY {
	sys_ui32 nb_vtx;
	sys_ui32 idx[1];
} GMT_POLY;

typedef struct _UTL_GEOMETRY {
	GMT_HEAD* pData;
} UTL_GEOMETRY;


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

D_EXTERN_FUNC void CFG_init(const char* fname);
D_EXTERN_FUNC const char* CFG_get(const char* pName);

D_EXTERN_FUNC UTL_GEOMETRY* GMT_load(const char* fname);
D_EXTERN_FUNC void GMT_free(UTL_GEOMETRY* pGeo);
D_EXTERN_FUNC UVEC* GMT_get_pnt(UTL_GEOMETRY* pGeo, int i);
D_EXTERN_FUNC GMT_POLY* GMT_get_pol(UTL_GEOMETRY* pGeo, int i);
D_EXTERN_FUNC const char* GMT_get_str(UTL_GEOMETRY* pGeo, int offs);
D_EXTERN_FUNC GMT_ATTR_INFO* GMT_get_attr_info_glb(UTL_GEOMETRY* pGeo);
D_EXTERN_FUNC GMT_ATTR_INFO* GMT_get_attr_info_pnt(UTL_GEOMETRY* pGeo);
D_EXTERN_FUNC GMT_ATTR_INFO* GMT_get_attr_info_pol(UTL_GEOMETRY* pGeo);
D_EXTERN_FUNC GMT_ATTR_INFO* GMT_find_attr_glb(UTL_GEOMETRY* pGeo, const char* name);
D_EXTERN_FUNC GMT_ATTR_INFO* GMT_find_attr_pnt(UTL_GEOMETRY* pGeo, const char* name);
D_EXTERN_FUNC GMT_ATTR_INFO* GMT_find_attr_pol(UTL_GEOMETRY* pGeo, const char* name);
D_EXTERN_FUNC void* GMT_get_attr_val_glb(UTL_GEOMETRY* pGeo, GMT_ATTR_INFO* pInfo);
D_EXTERN_FUNC void* GMT_get_attr_val_pnt(UTL_GEOMETRY* pGeo, GMT_ATTR_INFO* pInfo, int pnt_id);
D_EXTERN_FUNC void* GMT_get_attr_val_pol(UTL_GEOMETRY* pGeo, GMT_ATTR_INFO* pInfo, int pol_id);

D_EXTERN_FUNC float UTL_frand01(void);
D_EXTERN_FUNC float UTL_frand_11(void);

