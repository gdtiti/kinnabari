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

typedef struct _LANE_TBL {
	sys_ui32 offs_name;
	sys_ui32 offs_info;
	sys_ui32 offs_area;
	sys_ui32 nb_vtx;
} LANE_TBL;

typedef struct _LANE_HEAD {
	sys_ui32 magic;
	sys_ui32 nb_lane;
	LANE_TBL tbl[1];
} LANE_HEAD;

typedef struct _LANE_VTX {
	UVEC3 pos;
	float uv[2];
} LANE_VTX;

typedef struct _LANE_INFO {
	GEOM_AABB aabb;
	LANE_VTX  vtx[1];
} LANE_INFO;

typedef struct _LANE_ANIM {
	KFR_GROUP* pGrp_pos;
	KFR_GROUP* pGrp_tgt;
} LANE_ANIM;

typedef struct _FRUSTUM {
	UVEC pnt[8];
	UVEC nrm[6];
	GEOM_PLANE pln[6];
} FRUSTUM;

typedef struct _CAMERA {
	QMTX mtx_view;
	QMTX mtx_view_i;
	QMTX mtx_proj;
	QMTX mtx_proj_i;
	UVEC pos;
	UVEC tgt;
	UVEC up;
	FRUSTUM frustum;
	float aspect;
	float fovy;
	float znear;
	float zfar;

	LANE_HEAD* pLane_data;
	KFR_HEAD* pKfr_data;
	LANE_ANIM* pLane_anm;
} CAMERA;

D_EXTERN_DATA CAMERA g_cam;

D_EXTERN_FUNC void CAM_init(CAMERA* pCam);
D_EXTERN_FUNC void CAM_set_zoom(CAMERA* pCam, float focal, float aperture);
D_EXTERN_FUNC void CAM_set_fovy(CAMERA* pCam, float fovy);
D_EXTERN_FUNC void CAM_set_z_planes(CAMERA* pCam, float znear, float zfar);
D_EXTERN_FUNC void CAM_set_view(CAMERA* pCam, QVEC pos, QVEC tgt, QVEC up);
D_EXTERN_FUNC void CAM_update(CAMERA* pCam);
D_EXTERN_FUNC void CAM_apply(CAMERA* pCam);
D_EXTERN_FUNC int CAM_cull_box(CAMERA* pCam, GEOM_AABB* pBox);
D_EXTERN_FUNC int CAM_cull_box_ex(CAMERA* pCam, GEOM_AABB* pBox);
D_EXTERN_FUNC void CAM_load_data(CAMERA* pCam, const char* fname_kfr, const char* fname_lane);
D_EXTERN_FUNC void CAM_exec(CAMERA* pCam, QVEC pos, float offs_up, float offs_dn);

