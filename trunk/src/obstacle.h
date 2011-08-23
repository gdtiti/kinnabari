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

typedef enum _E_OBST_POLYATTR {
	E_OBST_POLYATTR_FLOOR = 1<<0,
	E_OBST_POLYATTR_CEIL  = 1<<1,
	E_OBST_POLYATTR_WALL  = 1<<2
} E_OBST_POLYATTR;

typedef struct _OBST_HEAD {
	sys_ui32 magic;
	sys_ui32 nb_pnt;
	sys_ui32 nb_pol;
} OBST_HEAD;

typedef struct _OBST_POLY {
	sys_ui16 idx[4];
	sys_ui32 attr; /* E_OBST_POLYATTR */
} OBST_POLY;

typedef struct _OBSTACLE {
	OBST_HEAD* pData;
	UVEC3* pPnt;
	OBST_POLY* pPol;
} OBSTACLE;

typedef struct _OBST_QUERY {
	UVEC p0;
	UVEC p1;
	UVEC hit_pos;
	UVEC hit_nml;
	sys_ui32 mask;
	float dist2;
	int res;
	int pol_no;
} OBST_QUERY;

D_EXTERN_FUNC void OBST_load(OBSTACLE* pObst, const char* name);
D_EXTERN_FUNC int OBST_check(OBSTACLE* pObst, OBST_QUERY* pQry);
D_EXTERN_FUNC int OBST_collide(OBSTACLE* pObst, QVEC cur_pos, QVEC prev_pos, float r, sys_ui32 mask, QVEC* pNew_pos);
