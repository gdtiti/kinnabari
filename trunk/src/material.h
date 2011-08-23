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

typedef struct _MTL_INFO {
	UVEC sun_dir;
	UVEC sun_color;
	union {
		UVEC sun_param;
		struct {
			float sun_scale;
			float sun_bias;
			float sun_power;
			float sun_blend;
		};
	};
	UVEC rim_color;
	union {
		UVEC rim_param;
		struct {
			float rim_scale;
			float rim_bias;
			float rim_power;
		};
	};
} MTL_INFO;

typedef struct _MATERIAL {
	MTL_INFO* pInfo;
	const char* pName;
} MATERIAL;

typedef struct _MTL_LIST {
	DICT*     pName_dict;
	MTL_INFO* pInfo;
	MATERIAL* pMtl;
	sys_ui32  nb_mtl;
} MTL_LIST;

typedef struct _MTL_SYS {
	SYM_POOL* pName_pool;
} MTL_SYS;

D_EXTERN_DATA MTL_SYS g_mtl_sys;

D_EXTERN_FUNC void MTL_sys_init(void);
D_EXTERN_FUNC void MTL_sys_reset(void);

D_EXTERN_FUNC MTL_LIST* MTL_lst_create(MTL_INFO* pInfo, sys_ui32* pName_offs, void* pData_top, int n);

D_EXTERN_FUNC RDR_BATCH_PARAM* MTL_apply(MATERIAL* pMtl, RDR_BATCH_PARAM* pParam);

