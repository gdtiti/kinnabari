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

#define D_MAX_ANIM_NODE (64)

typedef enum _E_ANMGRPTYPE {
	E_ANMGRPTYPE_INVALID,
	E_ANMGRPTYPE_ROOT,
	E_ANMGRPTYPE_CENTER,
	E_ANMGRPTYPE_JNT,
	E_ANMGRPTYPE_KCTOP_L,
	E_ANMGRPTYPE_KCEND_L,
	E_ANMGRPTYPE_KCTOP_R,
	E_ANMGRPTYPE_KCEND_R
} E_ANMGRPTYPE;

typedef enum _E_ANMSTATUS {
	E_ANMSTATUS_LOOP  = 1,
	E_ANMSTATUS_SHIFT = 2
} E_ANMSTATUS;

typedef enum _E_KCATTR {
	E_KCATTR_FLOORADJ = 1,
	E_KCATTR_FOOTROT  = 2
} E_KCATTR;

typedef struct _JOINT JOINT;
typedef struct _MODEL MODEL;

typedef struct _ANM_GRP_INFO {
	JOINT* pJnt;
	sys_byte type; /* E_ANMGRPTYPE */
} ANM_GRP_INFO;

typedef struct _ANM_DATA {
	KFR_HEAD* pKfr;
	ANM_GRP_INFO* pInfo;
} ANM_DATA;

typedef struct _KIN_CHAIN {
	UVEC end_pos;
	UVEC3 top_rot;
	JOINT* pJnt_top;
	JOINT* pJnt_rot;
	JOINT* pJnt_end;
	sys_ui32 attr; /* E_KCATTR */
	float foot_rx;
	float foot_rz;
	float foot_rz_factor;
} KIN_CHAIN;

typedef struct _ANM_MOVE {
	UVEC pos;
	UVEC prev;
	UVEC end;
	UVEC vel;
	float heading;
} ANM_MOVE;

typedef struct _JNT_BLEND_POSE {
	UVEC quat;
	UVEC pos;
} JNT_BLEND_POSE;

typedef struct _ANM_BLEND {
	float duration;
	float count;
	JNT_BLEND_POSE* pPose;
} ANM_BLEND;

typedef struct _ANIMATION {
	ANM_MOVE move;
	UVEC3 root_rot;
	UVEC root_pos;
	MODEL* pMdl;
	ANM_DATA* pData;
	ANM_BLEND* pBlend;
	KIN_CHAIN kc_leg_l;
	KIN_CHAIN kc_leg_r;
	float ankle_height;
	float frame;
	float frame_step;
	sys_ui32 status;
} ANIMATION;

typedef int (*IK_FLOOR_FUNC)(QVEC pos, float range, UVEC* pFloor_pos, UVEC* pFloor_nml);

D_EXTERN_DATA IK_FLOOR_FUNC g_ik_floor_func;

D_EXTERN_FUNC ANIMATION* ANM_create(MODEL* pMdl);
D_EXTERN_FUNC void ANM_destroy(ANIMATION* pAnm);
D_EXTERN_FUNC ANM_DATA* ANM_data_create(ANIMATION* pAnm, KFR_HEAD* pKfr);
D_EXTERN_FUNC void ANM_data_destroy(ANM_DATA* pData);
D_EXTERN_FUNC void ANM_set(ANIMATION* pAnm, ANM_DATA* pData, int start_frame);
D_EXTERN_FUNC void ANM_play(ANIMATION* pAnm);
D_EXTERN_FUNC void ANM_move(ANIMATION* pAnm);
D_EXTERN_FUNC void ANM_calc_ik(ANIMATION* pAnm);
D_EXTERN_FUNC void ANM_blend_init(ANIMATION* pAnm, int duration);
D_EXTERN_FUNC void ANM_blend_calc(ANIMATION* pAnm);

