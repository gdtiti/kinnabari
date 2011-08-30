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

CAMERA g_cam;

static void Cam_ctrl_init(CAMERA* pCam) {
	CAM_CTRL* pCtrl = &pCam->ctrl;
	pCtrl->pos_offs.qv = V4_set_vec(-0.4f, 1.8f, -3.2f);
	pCtrl->tgt_offs.qv = V4_set_vec(0.0f, 0.8f, 0.0f);
	pCtrl->ry = 0.0f;
	pCtrl->t = 0.0f;
	pCtrl->dt = 0.0f;
	pCtrl->dist_s = 0.93f;
	pCtrl->dist_t = 0.0f;
	pCtrl->flg = 0;
}

void CAM_init(CAMERA* pCam) {
	pCam->aspect = 320.0f / 240.0f;
	CAM_set_zoom(pCam, 50.0f, 41.5f);
	CAM_set_z_planes(pCam, 0.1f, 1000.0f);
	CAM_set_view(pCam, V4_set_pnt(-1.5f, 1.25f, 2.75f), V4_set_pnt(0.0f, 0.8f, 0.0f), V4_set_vec(0.0f, 1.0f, 0.0f));
	pCam->prev_pos.qv = pCam->pos.qv;
	Cam_ctrl_init(pCam);
	CAM_update(pCam);
}

void CAM_set_zoom(CAMERA* pCam, float focal, float aperture) {
	float zoom = ((2.0f * focal) / aperture) * pCam->aspect;
	pCam->fovy = 2.0f * atan2f(1.0f, zoom);
}

void CAM_set_fovy(CAMERA* pCam, float fovy) {
	pCam->fovy = fovy;
}

void CAM_set_z_planes(CAMERA* pCam, float znear, float zfar) {
	pCam->znear = znear;
	pCam->zfar = zfar;
}

void CAM_set_view(CAMERA* pCam, QVEC pos, QVEC tgt, QVEC up) {
	pCam->pos.qv = V4_set_w1(pos);
	pCam->tgt.qv = V4_set_w1(tgt);
	pCam->up.qv = V4_normalize(V4_set_w0(up));
}

static QVEC Cam_frustum_norm(QVEC v0, QVEC v1, QVEC v2) {
	return V4_normalize(V4_cross(V4_sub(v0, v1), V4_sub(v2, v1)));
}

static void Cam_update_frustum(CAMERA* pCam) {
	int i;
	float t;
	float x, y, z;
	FRUSTUM* pVol = &pCam->frustum;

	t = tanf(pCam->fovy * 0.5f);
	z = pCam->znear;
	y = t * z;
	x = y * pCam->aspect;
	pVol->pnt[0].qv = V4_set(-x,  y, -z, 1.0f);
	pVol->pnt[1].qv = V4_set( x,  y, -z, 1.0f);
	pVol->pnt[2].qv = V4_set( x, -y, -z, 1.0f);
	pVol->pnt[3].qv = V4_set(-x, -y, -z, 1.0f);
	z = pCam->zfar;
	y = t * z;
	x = y * pCam->aspect;
	pVol->pnt[4].qv = V4_set(-x,  y, -z, 1.0f);
	pVol->pnt[5].qv = V4_set( x,  y, -z, 1.0f);
	pVol->pnt[6].qv = V4_set( x, -y, -z, 1.0f);
	pVol->pnt[7].qv = V4_set(-x, -y, -z, 1.0f);
	pVol->nrm[0].qv = Cam_frustum_norm(pVol->pnt[0].qv, pVol->pnt[1].qv, pVol->pnt[3].qv); /* near */
	pVol->nrm[1].qv = Cam_frustum_norm(pVol->pnt[0].qv, pVol->pnt[3].qv, pVol->pnt[4].qv); /* left */
	pVol->nrm[2].qv = Cam_frustum_norm(pVol->pnt[0].qv, pVol->pnt[4].qv, pVol->pnt[5].qv); /* top */
	pVol->nrm[3].qv = Cam_frustum_norm(pVol->pnt[6].qv, pVol->pnt[2].qv, pVol->pnt[1].qv); /* right */
	pVol->nrm[4].qv = Cam_frustum_norm(pVol->pnt[6].qv, pVol->pnt[7].qv, pVol->pnt[3].qv); /* bottom */
	pVol->nrm[5].qv = Cam_frustum_norm(pVol->pnt[6].qv, pVol->pnt[5].qv, pVol->pnt[4].qv); /* far */
	for (i = 0; i < 8; ++i) {
		pVol->pnt[i].qv = MTX_apply(pCam->mtx_view_i, pVol->pnt[i].qv);
	}
	for (i = 0; i < 6; ++i) {
		pVol->nrm[i].qv = MTX_apply(pCam->mtx_view_i, pVol->nrm[i].qv);
	}
	for (i = 0; i < 6; ++i) {
		static int vtx_no[] = {0, 0, 0, 6, 6, 6};
		pVol->pln[i].qv = GEOM_get_plane(pVol->pnt[vtx_no[i]].qv, pVol->nrm[i].qv);
	}
}

void CAM_update(CAMERA* pCam) {
	MTX_make_view(pCam->mtx_view, pCam->pos.qv, pCam->tgt.qv, pCam->up.qv);
	MTX_invert_fast(pCam->mtx_view_i, pCam->mtx_view);
	MTX_make_proj(pCam->mtx_proj, pCam->fovy, pCam->aspect, pCam->znear, pCam->zfar);
	MTX_invert(pCam->mtx_proj_i, pCam->mtx_proj);
	Cam_update_frustum(pCam);
}

void CAM_apply(CAMERA* pCam) {
	RDR_VIEW* pView = RDR_get_view();
	MTX_cpy(pView->view, pCam->mtx_view);
	MTX_cpy(pView->iview, pCam->mtx_view_i);
	MTX_cpy(pView->proj, pCam->mtx_proj);
	MTX_cpy(pView->iproj, pCam->mtx_proj_i);
	MTX_mul(pView->view_proj, pView->view, pView->proj);
	pView->dir.qv = V4_scale(V4_load(pView->iview[2]), -1.0f);
	pView->pos.qv = V4_set_w1(pCam->pos.qv);
}

D_FORCE_INLINE static int Cam_cull_box_ck(QVEC vec, QVEC rvec, QVEC nrm) {
	QVEC na = V4_abs(nrm);
	return (V4_dot(rvec, na) < V4_dot(vec, nrm));
}

int CAM_cull_box(CAMERA* pCam, GEOM_AABB* pBox) {
	QVEC cvec;
	QVEC rvec;
	QVEC vec;
	FRUSTUM* pVol = &pCam->frustum;

	cvec = V4_scale(V4_add(pBox->min.qv, pBox->max.qv), 0.5f);
	rvec = V4_sub(pBox->max.qv, cvec);
	vec = V4_sub(cvec, pVol->pnt[0].qv);
	if (Cam_cull_box_ck(vec, rvec, pVol->nrm[0].qv)) return 1;
	if (Cam_cull_box_ck(vec, rvec, pVol->nrm[1].qv)) return 1;
	if (Cam_cull_box_ck(vec, rvec, pVol->nrm[2].qv)) return 1;
	vec = V4_sub(cvec, pVol->pnt[6].qv);
	if (Cam_cull_box_ck(vec, rvec, pVol->nrm[3].qv)) return 1;
	if (Cam_cull_box_ck(vec, rvec, pVol->nrm[4].qv)) return 1;
	if (Cam_cull_box_ck(vec, rvec, pVol->nrm[5].qv)) return 1;
	return 0;
}

#define _D_MK_AABB_VTX(_px, _py, _pz) V4_set_pnt(pBox->##_px.x, pBox->##_py.y, pBox->##_pz.z)
#define _D_BOX_EDGE_CK(_p0x, _p0y, _p0z, _p1x, _p1y, _p1z) \
	a = _D_MK_AABB_VTX(_p0x, _p0y, _p0z); \
	b = _D_MK_AABB_VTX(_p1x, _p1y, _p1z); \
	if (GEOM_seg_polyhedron_intersect(a, b, pVol->pln, 6, NULL)) return 0

#define _D_HEX_EDGE_CK(i0, i1) if (GEOM_seg_polyhedron_intersect(pVol->pnt[i0].qv, pVol->pnt[i1].qv, box_vol, 6, NULL)) return 0

int CAM_cull_box_ex(CAMERA* pCam, GEOM_AABB* pBox) {
	QVEC a;
	QVEC b;
	GEOM_PLANE box_vol[6];
	FRUSTUM* pVol = &pCam->frustum;
	static QVEC box_nml[6] = {
		{-1, 0, 0, 0},
		{0, -1, 0, 0},
		{0, 0, -1, 0},
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0}
	};

	_D_BOX_EDGE_CK(min, min, min,  max, min, min);
	_D_BOX_EDGE_CK(max, min, min,  max, min, max);
	_D_BOX_EDGE_CK(max, min, max,  min, min, max);
	_D_BOX_EDGE_CK(min, min, max,  min, min, min);

	_D_BOX_EDGE_CK(min, max, min,  max, max, min);
	_D_BOX_EDGE_CK(max, max, min,  max, max, max);
	_D_BOX_EDGE_CK(max, max, max,  min, max, max);
	_D_BOX_EDGE_CK(min, max, max,  min, max, min);

	_D_BOX_EDGE_CK(min, min, min,  min, max, min);
	_D_BOX_EDGE_CK(max, min, min,  max, max, min);
	_D_BOX_EDGE_CK(max, min, max,  max, max, max);
	_D_BOX_EDGE_CK(min, min, max,  min, max, max);

	box_vol[0].qv = GEOM_get_plane(pBox->min.qv, box_nml[0]);
	box_vol[1].qv = GEOM_get_plane(pBox->min.qv, box_nml[1]);
	box_vol[2].qv = GEOM_get_plane(pBox->min.qv, box_nml[2]);
	box_vol[3].qv = GEOM_get_plane(pBox->max.qv, box_nml[3]);
	box_vol[4].qv = GEOM_get_plane(pBox->max.qv, box_nml[4]);
	box_vol[5].qv = GEOM_get_plane(pBox->max.qv, box_nml[5]);

	_D_HEX_EDGE_CK(0, 1);
	_D_HEX_EDGE_CK(1, 2);
	_D_HEX_EDGE_CK(2, 3);
	_D_HEX_EDGE_CK(3, 0);

	_D_HEX_EDGE_CK(4, 5);
	_D_HEX_EDGE_CK(5, 6);
	_D_HEX_EDGE_CK(6, 7);
	_D_HEX_EDGE_CK(7, 4);

	_D_HEX_EDGE_CK(0, 4);
	_D_HEX_EDGE_CK(1, 5);
	_D_HEX_EDGE_CK(2, 6);
	_D_HEX_EDGE_CK(3, 7);

	return 1;
}

void CAM_load_data(CAMERA* pCam, const char* fname_kfr, const char* fname_lane) {
	pCam->pKfr_data = KFR_load(fname_kfr);
	pCam->pLane_data = (LANE_HEAD*)SYS_load(fname_lane);
	if (pCam->pLane_data) {
		int i, n;
		if (pCam->pLane_data->magic != D_FOURCC('L','A','N','E')) {
			SYS_log("Invalid LANE file\n");
			return;
		}
		n = pCam->pLane_data->nb_lane;
		pCam->pLane_anm = (LANE_ANIM*)SYS_malloc(n*sizeof(LANE_ANIM));
		for (i = 0; i < n; ++i) {
			char buf[64];
			const char* lane_name = (const char*)D_INCR_PTR(pCam->pLane_data, pCam->pLane_data->tbl[i].offs_name);
			pCam->pLane_anm[i].pGrp_pos = KFR_search_grp(pCam->pKfr_data, lane_name);
			sprintf_s(buf, sizeof(buf), "%s_tgt", lane_name);
			pCam->pLane_anm[i].pGrp_tgt = KFR_search_grp(pCam->pKfr_data, buf);
		}
	}
}

static int Check_lane(LANE_VTX* pVtx, int nb_pol, QVEC p0, QVEC p1, UVEC* pUV) {
	QVEC quad[4];
	QVEC tri[3];
	QVEC hit_pos;
	QVEC uvec;
	QVEC vvec;
	UVEC bc;
	float u, v;
	int i, j;
	for (i = 0; i < nb_pol; ++i) {
		for (j = 0; j < 4; ++j) {
			quad[j] = V4_set_pnt(pVtx[j].pos.x, pVtx[j].pos.y, pVtx[j].pos.z);
		}
		if (GEOM_seg_quad_intersect(p0, p1, quad, &hit_pos, NULL)) {
			if (0 == GEOM_barycentric(hit_pos, quad, &bc.qv)) {
				uvec = V4_set(pVtx[0].uv[0], pVtx[1].uv[0], pVtx[2].uv[0], 0.0f);
				vvec = V4_set(pVtx[0].uv[1], pVtx[1].uv[1], pVtx[2].uv[1], 0.0f);
			} else {
				tri[0] = quad[0];
				tri[1] = quad[2];
				tri[2] = quad[3];
				GEOM_barycentric(hit_pos, tri, &bc.qv);
				uvec = V4_set(pVtx[0].uv[0], pVtx[2].uv[0], pVtx[3].uv[0], 0.0f);
				vvec = V4_set(pVtx[0].uv[1], pVtx[2].uv[1], pVtx[3].uv[1], 0.0f);
			}
			u = V4_dot4(uvec, bc.qv);
			v = V4_dot4(vvec, bc.qv);
			pUV->qv = V4_set(u, v, 0.0f, 0.0f);
			return 1;
		}
		pVtx += 4;
	}
	return 0;
}

static QVEC Mk_rot_quat(QVEC a, QVEC b) {
	QVEC q = V4_load(g_identity[3]);
	if (!V4_same(q, b)) {
		QVEC v = V4_cross(a, b);
		if (V4_mag2(v) > 1e-8f) {
			float d;
			v = V4_normalize(v);
			d = V4_dot(a, b);
			if (d != 1.0f) {
				float s = sqrtf((1.0f - d) * 0.5f);
				float c = sqrtf((1.0f + d) * 0.5f);
				q = V4_set_w(V4_scale(v, s), c);
			}
		}
	}
	return q;
}

static QVEC Cam_check_floor(QVEC cam_pos) {
	UVEC pos;
	OBST_QUERY qry;

	pos.qv = V4_set_w1(cam_pos);
	qry.p0.qv = pos.qv;
	qry.p0.y += 1.0f;
	qry.p1.qv = pos.qv;
	qry.p1.y -= 10.0f;
	qry.mask = E_OBST_POLYATTR_FLOOR;
	if (OBST_check(&g_room.obst, &qry)) {
		const float min_h = 0.8f;
		const float max_h = 1.8f;
		if (pos.y - qry.hit_pos.y < min_h) {
			pos.y = qry.hit_pos.y + min_h;
		}
		if (pos.y - qry.hit_pos.y > max_h) {
			pos.y = qry.hit_pos.y + max_h;
		}
	}
	return pos.qv;
}

static void Cam_auto_ctrl(CAMERA* pCam, QVEC obj_pos, float heading) {
	QVEC q;
	QVEC v;
	QVEC offs;
	QVEC cam_pos;
	QVEC cam_tgt;
	CAM_CTRL* pCtrl = &pCam->ctrl;

	q = QUAT_from_axis_angle(V4_load(g_identity[1]), pCtrl->ry);
	q = V4_mul(q, V4_set(-1, -1, -1, 1));
	offs = QUAT_apply(q, pCtrl->pos_offs.qv);
	if (pCtrl->dist_t > 0.0f) {
		pCtrl->dist_t -= 1e-2f;
	} else {
		pCtrl->dist_t = 0.0f;
	}
	offs = V4_scale(offs, pCtrl->dist_s + pCtrl->dist_t*0.25f);
	q = QUAT_from_axis_angle(V4_load(g_identity[1]), heading);
	v = QUAT_apply(q, offs);
	cam_pos = V4_add(obj_pos, v);
	q = QUAT_from_axis_angle(V4_load(g_identity[1]), heading - pCtrl->ry);
	offs = QUAT_apply(q, pCtrl->tgt_offs.qv);
	cam_tgt = V4_add(obj_pos, offs);
	if (pCtrl->flg & 1) {
		QVEC vnow = V4_normalize(V4_sub(pCam->pos.qv, obj_pos));
		QVEC vnew = V4_normalize(V4_sub(cam_pos, obj_pos));
		float d = V4_dot(vnow, vnew);
		if (pCtrl->t == 0.0f) {
			if (d < 1.0f) {
				pCtrl->t = 1.0f;
				pCtrl->dt = 1e-3f;
			}
		} else {
			QVEC cvec = V4_cross(vnow, vnew);
			if (V4_mag2(cvec) > 1e-8f && d < 1.0f) {
				float l0, l1;
				q = Mk_rot_quat(vnow, vnew);
				q = QUAT_slerp(q, V4_load(g_identity[3]), pCtrl->t);
				v = QUAT_apply(q, vnow);
				pCtrl->t -= pCtrl->dt;
				pCtrl->dt -= pCtrl->dt*0.05f;
				vnew = V4_sub(cam_pos, obj_pos);
				l0 = V4_mag(v);
				l1 = V4_mag(vnew);
				if (l0*l1 != 0.0f) {
					v = V4_scale(v, l1/l0);
					cam_pos = V4_add(obj_pos, v);
				} else {
					pCtrl->t = 0.0f;
				}
			} else {
				pCtrl->t = 0.0f;
			}
		}
	} else {
		pCtrl->flg |= 1;
	}
	cam_pos = Cam_check_floor(cam_pos);
	CAM_set_view(pCam, V4_set_w1(cam_pos), V4_set_w1(cam_tgt), V4_load(g_identity[1]));
}

void CAM_exec(CAMERA* pCam, QVEC pos, float offs_up, float offs_dn, float heading) {
	UVEC p0;
	UVEC p1;
	UVEC uv;
	UVEC cam_pos;
	UVEC cam_tgt;
	GEOM_AABB seg_box;
	GEOM_AABB* pBox;
	KFR_GROUP* pGrp;
	LANE_HEAD* pLane;
	LANE_INFO* pInfo;
	float frame;
	int i, n, nb_vtx, nb_pol, offs, lane_idx;

	pCam->prev_pos.qv = V4_set_w1(pCam->pos.qv);
	pLane = pCam->pLane_data;
	if (!pLane) {
		Cam_auto_ctrl(pCam, pos, heading);
		return;
	}
	p0.qv = V4_set_w1(V4_add(pos, V4_set_vec(0.0f, offs_up, 0.0f)));
	p1.qv = V4_set_w1(V4_sub(pos, V4_set_vec(0.0f, offs_dn, 0.0f)));
	n = pLane->nb_lane;
	lane_idx = -1;
	for (i = 0; i < n; ++i) {
		offs = pLane->tbl[i].offs_area;
		if (offs) {
			pBox = (GEOM_AABB*)D_INCR_PTR(pLane, offs);
			if (!GEOM_pnt_inside_aabb(pos, pBox)) continue;
		}
		nb_vtx = pLane->tbl[i].nb_vtx;
		nb_pol = nb_vtx / 4;
		offs = pLane->tbl[i].offs_info;
		pInfo = (LANE_INFO*)D_INCR_PTR(pLane, offs);
		seg_box.max.qv = p0.qv;
		seg_box.min.qv = p1.qv;
		if (GEOM_aabb_overlap(&seg_box, &pInfo->aabb)) {
			if (Check_lane(pInfo->vtx, nb_pol, p0.qv, p1.qv, &uv)) {
				lane_idx = i;
				break;
			}
		}
	}
	if (lane_idx >= 0) {
		cam_pos.qv = V4_set_w1(V4_zero());
		cam_tgt.qv = V4_set_pnt(0, 0, -1);
		if (pCam->pKfr_data) {
			frame = uv.y * (pCam->pKfr_data->max_frame);
			pGrp = pCam->pLane_anm[lane_idx].pGrp_pos;
			if (pGrp) {
				cam_pos.qv = V4_set_w1(KFR_eval_grp(pCam->pKfr_data, pGrp, frame));
			}
			pGrp = pCam->pLane_anm[lane_idx].pGrp_tgt;
			if (pGrp) {
				cam_tgt.qv = V4_set_w1(KFR_eval_grp(pCam->pKfr_data, pGrp, frame));
			}
		}
		CAM_set_view(pCam, cam_pos.qv, cam_tgt.qv, V4_load(g_identity[1]));
	} else {
		Cam_auto_ctrl(pCam, pos, heading);
	}
}

