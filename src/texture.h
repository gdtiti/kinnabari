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

/* size = 0x10 */
typedef struct _TPK_HEAD {
/* +00 */	sys_ui32 magic;
/* +04 */	sys_ui32 nb_tex;
/* +08 */	sys_ui32 size;
/* +0C */	sys_ui32 ext_info_offs;
} TPK_HEAD;

typedef struct _TEX_HEAD {
/* +00 */	sys_byte fmt;
/* +01 */	sys_byte nb_lvl;
/* +02 */	sys_ui16 width;
/* +04 */	sys_ui16 height;
/* +06 */	sys_i16  depth;
/* +08 */	sys_ui32 top_lvl_size;
/* +0C */	sys_ui32 list_offs;
} TEX_HEAD;

typedef struct _TEX_EXT_INFO {
	sys_ui32 ext_head_size;
	sys_ui32 tex_name_offs;
} TEX_EXT_INFO;

typedef struct _TEX_PACKAGE TEX_PACKAGE;

typedef struct _TEX_INFO {
	RDR_TEXTURE* pTex;
	TEX_PACKAGE* pPkg;
	sys_ui16     idx;
	sys_ui16     reserved;
} TEX_INFO;

struct _TEX_PACKAGE {
	TPK_HEAD* pData;
	TEX_INFO* pList;
};

D_EXTERN_FUNC TEX_PACKAGE* TEX_load(const char* name);
D_EXTERN_FUNC void TEX_free(TEX_PACKAGE* pPkg);
D_EXTERN_FUNC void TEX_init(TEX_PACKAGE* pPkg);
D_EXTERN_FUNC void TEX_release(TEX_PACKAGE* pPkg);
