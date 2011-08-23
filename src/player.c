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
#include "anim.h"
#include "render.h"
#include "material.h"
#include "camera.h"
#include "obstacle.h"
#include "room.h"
#include "model.h"
#include "player.h"

typedef void (*PLR_FUNC)(PLAYER*);

PLAYER g_pl;

void PLR_init() {
	KFR_HEAD* pKfr;
	PLAYER* pPl = &g_pl;

	memset(pPl, 0, sizeof(PLAYER));

	pPl->pOmd = OMD_load("char/char.omd");
	pPl->pMdl = MDL_create(pPl->pOmd);
	pPl->pAnm = ANM_create(pPl->pMdl);

	pKfr = KFR_load("char/idle.kfr");
	pPl->pAnm_data[0] = ANM_data_create(pPl->pAnm, pKfr);
	pKfr = KFR_load("char/walk.kfr");
	pPl->pAnm_data[1] = ANM_data_create(pPl->pAnm, pKfr);

	ANM_set(pPl->pAnm, pPl->pAnm_data[0], 0);
	MDL_calc_local(pPl->pMdl);
	ANM_calc_ik(pPl->pAnm);
	ANM_blend_init(pPl->pAnm, 0);
	MDL_calc_world(pPl->pMdl);
	pPl->ctrl.var[0] = E_PLSTATE_IDLE;
	pPl->ctrl.var[1] = 1;
}

static void Plr_ctrl_idle(PLAYER* pPl) {
	if (pPl->inp_on & E_KEY_UP) {
		pPl->ctrl.var[0] = E_PLSTATE_WALK;
		pPl->ctrl.var[1] = 0;
	}
}

static void Plr_exec_idle(PLAYER* pPl) {
	switch (pPl->ctrl.var[1]) {
		case 0:
			ANM_blend_init(pPl->pAnm, 10);
			ANM_set(pPl->pAnm, pPl->pAnm_data[0], 0);
			++pPl->ctrl.var[1];
			/* fall thru */
		case 1:
			break;
	}
}

static void Plr_ctrl_walk(PLAYER* pPl) {
	if (!(pPl->inp_on & E_KEY_UP)) {
		pPl->ctrl.var[0] = E_PLSTATE_IDLE;
		pPl->ctrl.var[1] = 0;
	}
}

static void Plr_exec_walk(PLAYER* pPl) {
	float rot_add = 0.0f;
	const int ang_count = 10;
	switch (pPl->ctrl.var[1]) {
		case 0:
			ANM_blend_init(pPl->pAnm, 10);
			ANM_set(pPl->pAnm, pPl->pAnm_data[1], 0);
			++pPl->ctrl.var[1];
			/* fall thru */
		case 1:
			if (!(pPl->inp_on & (E_KEY_LEFT | E_KEY_RIGHT))) {
				pPl->ctrl.var[2] = 0;
			}
			rot_add = D_DEG2RAD(1.0f);
			if (1) {
				float t = (float)pPl->ctrl.var[2] / ang_count;
				float s = (1.0f - cosf(t*D_PI)) / 2;
				rot_add = D_DEG2RAD(0.2f) + (s / 35.0f);
			}
			if (pPl->inp_on & E_KEY_LEFT) {
				pPl->pAnm->move.heading += rot_add;
			} else if (pPl->inp_on & E_KEY_RIGHT) {
				pPl->pAnm->move.heading -= rot_add;
			}
			if (pPl->ctrl.var[2] < ang_count) {
				++pPl->ctrl.var[2];
			}
			break;
	}
}

static void Plr_ctrl(PLAYER* pPl) {
	static PLR_FUNC ctrl_tbl[] = {
		Plr_ctrl_idle,
		Plr_ctrl_walk,
		NULL
	};
	static PLR_FUNC exec_tbl[] = {
		Plr_exec_idle,
		Plr_exec_walk,
		NULL
	};
	ctrl_tbl[pPl->ctrl.var[0]](pPl);
	exec_tbl[pPl->ctrl.var[0]](pPl);
}

static void Plr_check_floor(PLAYER* pPl) {
	OBST_QUERY qry;
	MODEL* pMdl = pPl->pMdl;

	qry.p0.qv = pMdl->pos.qv;
	qry.p0.y += 1.0f;
	qry.p1.qv = pMdl->pos.qv;
	qry.p1.y -= 10.0f;
	qry.mask = E_OBST_POLYATTR_FLOOR;
	if (OBST_check(&g_room.obst, &qry)) {
		pMdl->pos.y = qry.hit_pos.y;
	}
}

static void Plr_wall_collide(PLAYER* pPl) {
	UVEC cur_pos;
	UVEC prev_pos;
	UVEC new_pos;
	MODEL* pMdl = pPl->pMdl;
	ANIMATION* pAnm = pPl->pAnm;
	cur_pos.qv = pMdl->pos.qv;
	cur_pos.y += 1.0f;
	prev_pos.qv = pMdl->prev_pos.qv;
	prev_pos.y += 1.0f;
	if (OBST_collide(&g_room.obst, cur_pos.qv, prev_pos.qv, 0.8f, E_OBST_POLYATTR_WALL, &new_pos.qv)) {
		pMdl->pos.x = new_pos.x;
		pMdl->pos.z = new_pos.z;
		pAnm->move.pos.x = pMdl->pos.x;
		pAnm->move.pos.z = pMdl->pos.z;
	}
}

void PLR_ctrl() {
	PLAYER* pPl = &g_pl;

	pPl->pMdl->prev_pos.qv = pPl->pMdl->pos.qv;
	pPl->inp_on = g_input.state;
	pPl->inp_trg = g_input.state_trg;
	Plr_ctrl(pPl);
	ANM_play(pPl->pAnm);
	ANM_move(pPl->pAnm);
	Plr_wall_collide(pPl);
	Plr_check_floor(pPl);
}

void PLR_calc(CAMERA* pCam) {
	PLAYER* pPl = &g_pl;

	MDL_calc_local(pPl->pMdl);
	ANM_calc_ik(pPl->pAnm);
	ANM_blend_calc(pPl->pAnm);
	MDL_calc_world(pPl->pMdl);
	MDL_cull(pPl->pMdl, pCam);
	MDL_disp(pPl->pMdl);
}

