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
#include "keyframe.h"
#include "anim.h"
#include "render.h"
#include "material.h"
#include "camera.h"
#include "model.h"

IK_FLOOR_FUNC g_ik_floor_func = NULL;

static void KC_init(ANIMATION* pAnm, KIN_CHAIN* pKC, JOINT* pJnt_top, JOINT* pJnt_rot, JOINT* pJnt_end) {
	memset(pKC, 0, sizeof(KIN_CHAIN));
	pKC->pJnt_top = pJnt_top;
	pKC->pJnt_rot = pJnt_rot;
	pKC->pJnt_end = pJnt_end;
	pKC->end_pos.qv = V4_set_w1(V4_scale(V4_load(pAnm->pMdl->pOmd->pJnt_inv[pJnt_end->pInfo->id][3]), -1.0f));
}

ANIMATION* ANM_create(MODEL* pMdl) {
	ANIMATION* pAnm = NULL;
	if (pMdl) {
		int nb_jnt = pMdl->pOmd->nb_jnt;
		int mem_size = D_ALIGN(sizeof(ANIMATION), 16) + D_ALIGN(sizeof(ANM_BLEND), 16) + nb_jnt*sizeof(JNT_BLEND_POSE);
		pAnm = (ANIMATION*)SYS_malloc(mem_size);
		memset(pAnm, 0, mem_size);
		pAnm->pMdl = pMdl;
		pAnm->pBlend = (ANM_BLEND*)D_INCR_PTR(pAnm, D_ALIGN(sizeof(ANIMATION), 16));
		pAnm->pBlend->pPose = (JNT_BLEND_POSE*)D_INCR_PTR(pAnm->pBlend, D_ALIGN(sizeof(ANM_BLEND), 16));
		pAnm->pData = NULL;
		pAnm->ankle_height = 0.1f;
		pAnm->frame = 0.0f;
		pAnm->frame_step = 1.0f;
		KC_init(pAnm, &pAnm->kc_leg_l, MDL_get_jnt(pMdl, "jnt_uprLeg_L"), MDL_get_jnt(pMdl, "jnt_lwrLeg_L"), MDL_get_jnt(pMdl, "jnt_ankle_L"));
		KC_init(pAnm, &pAnm->kc_leg_r, MDL_get_jnt(pMdl, "jnt_uprLeg_R"), MDL_get_jnt(pMdl, "jnt_lwrLeg_R"), MDL_get_jnt(pMdl, "jnt_ankle_R"));
		ANM_blend_init(pAnm, 0);
	}
	return pAnm;
}

void ANM_destroy(ANIMATION* pAnm) {
	SYS_free(pAnm);
}

ANM_DATA* ANM_data_create(ANIMATION* pAnm, KFR_HEAD* pKfr) {
	int i, n;
	ANM_GRP_INFO* pInfo;
	ANM_DATA* pData = NULL;
	n = pKfr->nb_grp;
	pData = (ANM_DATA*)SYS_malloc(sizeof(ANM_DATA) + n*sizeof(ANM_GRP_INFO));
	pData->pKfr = pKfr;
	pData->pInfo = (ANM_GRP_INFO*)(pData + 1);
	pInfo = pData->pInfo;
	for (i = 0; i < n; ++i) {
		KFR_GROUP* pGrp = KFR_get_grp(pKfr, i);
		const char* name = KFR_get_grp_name(pKfr, pGrp);
		pInfo->pJnt = NULL;
		pInfo->type = E_ANMGRPTYPE_INVALID;
		if (0 == strcmp(name, "root")) {
			pInfo->type = E_ANMGRPTYPE_ROOT;
		} else if (0 == strcmp(name, "center")) {
			pInfo->type = E_ANMGRPTYPE_CENTER;
			pInfo->pJnt = MDL_get_jnt(pAnm->pMdl, name);
		} else if (0 == strcmp(name, "ctl_legRot_L")) {
			pInfo->type = E_ANMGRPTYPE_KCTOP_L;
		} else if (0 == strcmp(name, "ctl_legEff_L")) {
			pInfo->type = E_ANMGRPTYPE_KCEND_L;
		} else if (0 == strcmp(name, "ctl_legRot_R")) {
			pInfo->type = E_ANMGRPTYPE_KCTOP_R;
		} else if (0 == strcmp(name, "ctl_legEff_R")) {
			pInfo->type = E_ANMGRPTYPE_KCEND_R;
		} else {
			pInfo->type = E_ANMGRPTYPE_JNT;
			pInfo->pJnt = MDL_get_jnt(pAnm->pMdl, name);
			//SYS_log("%s -> %s\n", name, pInfo->pJnt->pInfo->pName);
		}
		++pInfo;
	}
	return pData;
}

void ANM_data_destroy(ANM_DATA* pData) {
	if (pData) {
		KFR_free(pData->pKfr);
		SYS_free(pData);
	}
}

void ANM_set(ANIMATION* pAnm, ANM_DATA* pData, int start_frame) {
	pAnm->pData = pData;
	if (start_frame > pData->pKfr->max_frame) {
		start_frame = 0;
	}
	pAnm->status = 0;
	pAnm->frame = 0.0f;
	pAnm->frame_step = 0.0f;
	ANM_play(pAnm);
	pAnm->move.prev.qv = pAnm->root_pos.qv;
	pAnm->frame = pData->pKfr->max_frame;
	ANM_play(pAnm);
	pAnm->move.end.qv = pAnm->root_pos.qv;

	pAnm->status = 0;
	pAnm->frame = (float)start_frame;
	ANM_play(pAnm);

	pAnm->frame = (float)start_frame;
	pAnm->frame_step = 1.0f;
}

void ANM_play(ANIMATION* pAnm) {
	int i, j, n;
	UVEC dummy_pos;
	UVEC3 dummy_rot;
	ANM_DATA* pData;
	KFR_HEAD* pKfr;
	ANM_GRP_INFO* pInfo;
	float frame = pAnm->frame;

	pData = pAnm->pData;
	if (!pData) return;
	pKfr = pData->pKfr;
	pInfo = pData->pInfo;
	n = pKfr->nb_grp;
	for (i = 0; i < n; ++i) {
		UVEC3* pRot = &dummy_rot;
		UVEC* pPos = &dummy_pos;
		KFR_GROUP* pGrp = KFR_get_grp(pKfr, i);

		if (pInfo->pJnt) {
			pRot = &pInfo->pJnt->rot;
			pPos = &pInfo->pJnt->pos;
		} else {
			switch (pInfo->type) {
				case E_ANMGRPTYPE_ROOT:
					pRot = &pAnm->root_rot;
					pPos = &pAnm->root_pos;
					break;
				case E_ANMGRPTYPE_KCTOP_L:
					pRot = &pAnm->kc_leg_l.top_rot;
					break;
				case E_ANMGRPTYPE_KCEND_L:
					pPos = &pAnm->kc_leg_l.end_pos;
					break;
				case E_ANMGRPTYPE_KCTOP_R:
					pRot = &pAnm->kc_leg_r.top_rot;
					break;
				case E_ANMGRPTYPE_KCEND_R:
					pPos = &pAnm->kc_leg_r.end_pos;
					break;
				default:
					break;
			}
		}
		for (j = 0; j < pGrp->nb_chan; ++j) {
			KFR_CHANNEL* pCh = KFR_get_chan(pKfr, pGrp, j);
			float val = KFR_eval(pKfr, pCh, frame, NULL);
			if (pCh->attr & E_KFRATTR_RX) {
				pRot->x = val;
			} else if (pCh->attr & E_KFRATTR_RY) {
				pRot->y = val;
			} else if (pCh->attr & E_KFRATTR_RZ) {
				pRot->z = val;
			} else if (pCh->attr & E_KFRATTR_TX) {
				pPos->x = val;
			} else if (pCh->attr & E_KFRATTR_TY) {
				pPos->y = val;
			} else if (pCh->attr & E_KFRATTR_TZ) {
				pPos->z = val;
			}
		}
		++pInfo;
	}

	if (pAnm->status & E_ANMSTATUS_LOOP) {
		pAnm->status &= ~E_ANMSTATUS_LOOP;
		pAnm->status |= E_ANMSTATUS_SHIFT;
	}
	pAnm->frame += pAnm->frame_step;
	if (pAnm->frame > pAnm->pData->pKfr->max_frame) {
		pAnm->frame = 0.0f;
		pAnm->status |= E_ANMSTATUS_LOOP;
	}
}

void ANM_move(ANIMATION* pAnm) {
	QMTX m;
	UVEC pos;
	UVEC vel;
	UVEC prev;
	MODEL* pMdl;

	pMdl = pAnm->pMdl;
	pos.qv = pAnm->root_pos.qv;
	prev.qv = pos.qv;
	if (pAnm->status & E_ANMSTATUS_SHIFT) {
		pos.qv = V4_add(pos.qv, pAnm->move.end.qv);
		pAnm->status &= ~E_ANMSTATUS_SHIFT;
	}
	vel.qv = V4_sub(pos.qv, pAnm->move.prev.qv);
	pAnm->move.prev.qv = V4_set_w1(prev.qv);
	MTX_rot_y(m, pAnm->move.heading);
	pAnm->move.vel.qv = MTX_calc_qvec(m, vel.qv);
	pAnm->move.pos.x += pAnm->move.vel.x;
	pAnm->move.pos.z += pAnm->move.vel.z;

	pMdl->pos.x = pAnm->move.pos.x;
	pMdl->pos.z = pAnm->move.pos.z;
	pMdl->rot.y = pAnm->move.heading;
}

static void KC_calc(ANIMATION* pAnm, KIN_CHAIN* pKC) {
	QMTX parent_mtx;
	QMTX top_mtx;
	QMTX ik_mtx;
	QMTX rot_mtx;
	QMTX mx;
	QMTX my;
	QMTX imtx;
	QVEC top_pos;
	QVEC rot_pos;
	QVEC end_pos;
	QVEC ax;
	QVEC ay;
	QVEC az;
	JOINT* pJnt;
	MODEL* pMdl = pAnm->pMdl;
	float dist, len0, len1, rot0, rot1;

	MTX_rot_xyz(top_mtx, pKC->top_rot.x, pKC->top_rot.y, pKC->top_rot.z);
	V4_store(top_mtx[3], V4_set_w1(pKC->pJnt_top->pInfo->offs_len.qv));

	MTX_unit(parent_mtx);
	pJnt = &pMdl->pJnt[pKC->pJnt_top->pInfo->parent_id];
	while (pJnt->pInfo->parent_id >= 0) {
		MTX_mul(parent_mtx, parent_mtx, pJnt->mtx);
		pJnt = &pMdl->pJnt[pJnt->pInfo->parent_id];
	}
	MTX_mul(parent_mtx, parent_mtx, pJnt->mtx);
	MTX_mul(parent_mtx, parent_mtx, pMdl->root_mtx);
	MTX_mul(top_mtx, top_mtx, parent_mtx);

	end_pos = MTX_calc_qpnt(pMdl->root_mtx, pKC->end_pos.qv);
	if (g_ik_floor_func) {
		UVEC floor_pos;
		UVEC floor_nml;
		if (g_ik_floor_func(end_pos, 1.0f, &floor_pos, &floor_nml)) {
			UVEC tpos;
			float ankle_h = pAnm->ankle_height;
			tpos.qv = end_pos;
			if (tpos.y - ankle_h < floor_pos.y) {
				tpos.y = floor_pos.y + ankle_h;
				end_pos = tpos.qv;
			}
		}
	}
	top_pos = V4_load(top_mtx[3]);
	len0 = pKC->pJnt_rot->pInfo->offs_len.w;
	len1 = pKC->pJnt_end->pInfo->offs_len.w;
	az = V4_sub(end_pos, top_pos);
	dist = V4_mag(az);
	if (dist < len0 + len1) {
		float c0 = ( (len0*len0) - (len1*len1) + (dist*dist) ) / (2*len0*dist);
		float c1 = ( (len0*len0) + (len1*len1) - (dist*dist) ) / (2*len0*len1);
		c0 = D_CLAMP(c0, -1, 1);
		c1 = D_CLAMP(c1, -1, 1);
		rot0 = -acosf(c0);
		rot1 = D_PI - acosf(c1);
	}
	ax = MTX_calc_qvec(top_mtx, V4_load(g_identity[0]));
	ax = V4_normalize(ax);
	az = V4_normalize(az);
	ay = V4_normalize(V4_cross(az, ax));
	ax = V4_normalize(V4_cross(ay, az));
	V4_store(mx[0], ax);
	V4_store(mx[1], ay);
	V4_store(mx[2], az);
	V4_store(mx[3], V4_load(g_identity[3]));

	az = V4_set_vec(0.0f, -1.0f, 0.0f);
	ay = V4_set_vec(0.0f, 0.0f, 1.0f);
	ax = V4_normalize(V4_cross(ay, az));
	V4_store(my[0], ax);
	V4_store(my[1], ay);
	V4_store(my[2], az);
	V4_store(my[3], V4_load(g_identity[3]));
	MTX_transpose(my, my);

	MTX_mul(ik_mtx, my, mx);
	MTX_rot_axis(rot_mtx, V4_load(g_identity[0]), rot0);
	MTX_mul(top_mtx, rot_mtx, ik_mtx);
	V4_store(top_mtx[3], V4_set_w1(top_pos));

	rot_pos = MTX_calc_qpnt(top_mtx, pKC->pJnt_rot->pInfo->offs_len.qv);
	MTX_rot_axis(rot_mtx, V4_load(g_identity[0]), rot1);
	MTX_mul(rot_mtx, rot_mtx, top_mtx);
	V4_store(rot_mtx[3], V4_set_w1(rot_pos));

	MTX_invert_fast(imtx, parent_mtx);
	MTX_mul(pKC->pJnt_top->mtx, top_mtx, imtx);

	MTX_invert_fast(imtx, top_mtx);
	MTX_mul(pKC->pJnt_rot->mtx, rot_mtx, imtx);
}

void ANM_calc_ik(ANIMATION* pAnm) {
	KC_calc(pAnm, &pAnm->kc_leg_l);
	KC_calc(pAnm, &pAnm->kc_leg_r);
}

void ANM_blend_init(ANIMATION* pAnm, int duration) {
	int i, n;
	MODEL* pMdl = pAnm->pMdl;
	JOINT* pJnt = pMdl->pJnt;
	JNT_BLEND_POSE* pPose = pAnm->pBlend->pPose;
	n = pMdl->pOmd->nb_jnt;
	for (i = 0; i < n; ++i) {
		pPose->quat.qv = QUAT_from_mtx(pJnt->mtx);
		pPose->pos.qv = pJnt->pos.qv;
		++pJnt;
		++pPose;
	}
	pAnm->pBlend->duration = (float)duration;
	pAnm->pBlend->count = (float)duration;
}

void ANM_blend_calc(ANIMATION* pAnm) {
	QVEC quat;
	QVEC pos;
	int i, n;
	float t;
	MODEL* pMdl = pAnm->pMdl;
	JOINT* pJnt = pMdl->pJnt;
	ANM_BLEND* pBlend = pAnm->pBlend;
	JNT_BLEND_POSE* pPose = pBlend->pPose;

	if (pBlend->count <= 0.0f) return;
	t = (pBlend->duration - pBlend->count) / pBlend->duration;
	n = pMdl->pOmd->nb_jnt;
	for (i = 0; i < n; ++i) {
		quat = QUAT_from_mtx(pJnt->mtx);
		quat = QUAT_slerp(pPose->quat.qv, quat, t);
		pos = V4_lerp(pPose->pos.qv, pJnt->pos.qv, t);
		QUAT_get_mtx(quat, pJnt->mtx);
		pJnt->pos.qv = V4_set_w1(pos);
		++pJnt;
		++pPose;
	}
	--pBlend->count;
}

