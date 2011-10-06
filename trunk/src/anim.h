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

typedef struct _JOINT JOINT;
typedef struct _MODEL MODEL;

typedef enum _E_KFRATTR {
	E_KFRATTR_TX = 1 << 0,
	E_KFRATTR_TY = 1 << 1,
	E_KFRATTR_TZ = 1 << 2,
	E_KFRATTR_RX = 1 << 3,
	E_KFRATTR_RY = 1 << 4,
	E_KFRATTR_RZ = 1 << 5
} E_KFRATTR;

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
	E_ANMSTATUS_LOOP = 1,
	E_ANMSTATUS_SHIFT = 2
} E_ANMSTATUS;

typedef struct _KFR_HEAD {
	sys_ui32 magic;
	sys_ui16 max_frame;
	sys_ui16 nb_grp;
	sys_ui32 grp_offs[1];
} KFR_HEAD;

typedef struct _KFR_GROUP {
	sys_ui32 name_offs;
	sys_ui16 attr;
	sys_ui16 nb_chan;
	sys_ui32 ch_offs[1];
} KFR_GROUP;

typedef struct _KFR_CHANNEL {
	sys_ui16 attr;
	sys_ui16 nb_key;
	sys_ui16 frm_no[1];
} KFR_CHANNEL;

typedef struct _KFR_KEY {
	float val;
	float lslope;
	float rslope;
} KFR_KEY;


typedef struct _ANM_GRP_INFO {
	JOINT* pJnt;
	sys_byte type; /* E_ANMGRPTYPE */
} ANM_GRP_INFO;

typedef struct _ANM_DATA {
	KFR_HEAD* pKfr;
	ANM_GRP_INFO* pInfo;
} ANM_DATA;

typedef struct _KIN_CHAIN {
	UVEC  end_pos;
	UVEC3 top_rot;
	JOINT* pJnt_top;
	JOINT* pJnt_rot;
	JOINT* pJnt_end;
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
	float frame;
	float frame_step;
	sys_ui32 status;
} ANIMATION;

D_EXTERN_FUNC KFR_HEAD* KFR_load(const char* name);
D_EXTERN_FUNC void KFR_free(KFR_HEAD* pKfr);
D_EXTERN_FUNC KFR_GROUP* KFR_get_grp(KFR_HEAD* pKfr, int grp_no);
D_EXTERN_FUNC const char* KFR_get_grp_name(KFR_HEAD* pKfr, KFR_GROUP* pGrp);
D_EXTERN_FUNC KFR_GROUP* KFR_search_grp(KFR_HEAD* pKfr, const char* name);
D_EXTERN_FUNC KFR_CHANNEL* KFR_get_chan(KFR_HEAD* pKfr, KFR_GROUP* pGrp, int chan_no);
D_EXTERN_FUNC KFR_KEY* KFR_get_keys(KFR_HEAD* pKfr, KFR_CHANNEL* pCh);
D_EXTERN_FUNC float KFR_eval(KFR_HEAD* pKfr, KFR_CHANNEL* pCh, float frame, sys_ui16* pState);
D_EXTERN_FUNC QVEC KFR_eval_grp(KFR_HEAD* pKfr, KFR_GROUP* pGrp, float frame);

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

