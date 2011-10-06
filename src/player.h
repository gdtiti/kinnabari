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

#define D_PLR_MAX_ANIM (16)

typedef enum _E_PLSTATE {
	E_PLSTATE_IDLE,
	E_PLSTATE_WALK
} E_PLSTATE;

typedef struct _PLAYER_CTRL {
	sys_byte var[8];
} PLAYER_CTRL;

typedef struct _PLAYER {
	OMD* pOmd;
	MODEL* pMdl;
	ANIMATION* pAnm;
	ANM_DATA* pAnm_data[D_PLR_MAX_ANIM];
	PLAYER_CTRL ctrl;
	sys_ui32 inp_on;
	sys_ui32 inp_trg;
} PLAYER;

D_EXTERN_DATA PLAYER g_pl;

D_EXTERN_FUNC void PLR_init(void);
D_EXTERN_FUNC void PLR_free(void);
D_EXTERN_FUNC void PLR_ctrl(void);
D_EXTERN_FUNC void PLR_calc(CAMERA* pCam);
