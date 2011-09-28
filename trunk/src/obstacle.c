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
#include "obstacle.h"

typedef struct _BVH_WORK {
	GEOM_AABB bbox;
	OBST_QUERY* pQry;
	float min_dist2;
	int res;
} BVH_WORK;

void OBST_load(OBSTACLE* pObst, const char* obs_name, const char* bvh_name) {
	OBST_HEAD* pHead;

	pHead = SYS_load(obs_name);
	pObst->pData = pHead;
	pObst->pPnt = NULL;
	pObst->pPol = NULL;
	if (pHead && pHead->magic == D_FOURCC('O','B','S','T')) {
		pObst->pPnt = (UVEC3*)(pHead + 1);
		pObst->pPol = (OBST_POLY*)&pObst->pPnt[pHead->nb_pnt];
	}
	pObst->pBVH = NULL;
	if (bvh_name) {
		BVH_HEAD* pBVH = SYS_load(bvh_name);
		if (pBVH && pBVH->magic == D_FOURCC('B','V','H',0)) {
			pObst->pBVH = pBVH;
		}
	}
}

static QVEC Get_vtx(OBSTACLE* pObst, int idx) {
	UVEC3* pPnt = &pObst->pPnt[idx];
	return V4_set(pPnt->x, pPnt->y, pPnt->z, 1.0f);
}

static int Obst_check_direct(OBSTACLE* pObst, OBST_QUERY* pQry) {
	QVEC vtx[4];
	QVEC hit_pos;
	QVEC hit_nml;
	int i, j, n, res;
	OBST_POLY* pPol;
	float dist2;
	float min_dist2 = D_MAX_FLOAT;

	res = 0;
	n = pObst->pData->nb_pol;
	pPol = pObst->pPol;
	for (i = 0; i < n; ++i) {
		if (pPol->attr & pQry->mask) {
			for (j = 0; j < 4; ++j) {
				vtx[j] = Get_vtx(pObst, pPol->idx[j]);
			}
			if (GEOM_seg_quad_intersect(pQry->p0.qv, pQry->p1.qv, vtx, &hit_pos, &hit_nml)) {
				dist2 = V4_dist2(pQry->p0.qv, hit_pos);
				if (dist2 < min_dist2) {
					pQry->hit_pos.qv = hit_pos;
					pQry->hit_nml.qv = hit_nml;
					pQry->dist2 = dist2;
					pQry->pol_no = i;
					min_dist2 = dist2;
				}
				res = 1;
			}
		}
		++pPol;
	}
	pQry->res = res;
	return res;
}

GEOM_AABB* BVH_get_node_bbox(BVH_HEAD* pBVH, int i) {
	return (GEOM_AABB*)(pBVH + 1) + i;
}

BVH_NODE* BVH_get_node(BVH_HEAD* pBVH, int i) {
	return (BVH_NODE*)BVH_get_node_bbox(pBVH, pBVH->nb_node) + i;
}

BVH_NODE* BVH_get_root(BVH_HEAD* pBVH) {
	return BVH_get_node(pBVH, 0);
}

int BVH_get_node_id(BVH_HEAD* pBVH, BVH_NODE* pNode) {
	return (int)(pNode - BVH_get_root(pBVH));
}

D_FORCE_INLINE static int Node_hit_ck(OBSTACLE* pObst, BVH_NODE* pNode, BVH_WORK* pWk) {
	GEOM_AABB* pBox = BVH_get_node_bbox(pObst->pBVH, BVH_get_node_id(pObst->pBVH, pNode));
	if (GEOM_aabb_overlap(&pWk->bbox, pBox)) {
		if (GEOM_seg_aabb_test(pWk->pQry->p0.qv, pWk->pQry->p1.qv, pBox)) {
			return 1;
		}
	}
	return 0;
}

static void Obst_bvh_ck(OBSTACLE* pObst, BVH_NODE* pNode, BVH_WORK* pWk) {
	QVEC vtx[4];
	QVEC hit_pos;
	QVEC hit_nml;
	int i;
	float dist2;
	if (Node_hit_ck(pObst, pNode, pWk)) {
		if (pNode->right < 0) {
			OBST_QUERY* pQry = pWk->pQry;
			OBST_POLY* pPol = &pObst->pPol[pNode->prim];
			if (pPol->attr & pQry->mask) {
				for (i = 0; i < 4; ++i) {
					vtx[i] = Get_vtx(pObst, pPol->idx[i]);
				}
				if (GEOM_seg_quad_intersect(pQry->p0.qv, pQry->p1.qv, vtx, &hit_pos, &hit_nml)) {
					dist2 = V4_dist2(pQry->p0.qv, hit_pos);
					if (dist2 < pWk->min_dist2) {
						pQry->hit_pos.qv = hit_pos;
						pQry->hit_nml.qv = hit_nml;
						pQry->dist2 = dist2;
						pQry->pol_no = pNode->prim;
						pWk->min_dist2 = dist2;
					}
					pWk->res = 1;
				}
			}
		} else {
			Obst_bvh_ck(pObst, BVH_get_node(pObst->pBVH, pNode->left), pWk);
			Obst_bvh_ck(pObst, BVH_get_node(pObst->pBVH, pNode->right), pWk);
		}
	}
}

static int Obst_check_bvh(OBSTACLE* pObst, OBST_QUERY* pQry) {
	BVH_WORK wk;

	wk.bbox.min.qv = V4_set_w1(V4_min(pQry->p0.qv, pQry->p1.qv));
	wk.bbox.max.qv = V4_set_w1(V4_max(pQry->p0.qv, pQry->p1.qv));
	wk.pQry = pQry;
	wk.res = 0;
	wk.min_dist2 = D_MAX_FLOAT;
	Obst_bvh_ck(pObst, BVH_get_root(pObst->pBVH), &wk);
	pQry->res = wk.res;
	return wk.res;
}

int OBST_check(OBSTACLE* pObst, OBST_QUERY* pQry) {
	if (pObst->pBVH) {
		return Obst_check_bvh(pObst, pQry);
	} else {
		return Obst_check_direct(pObst, pQry);
	}
}

int OBST_collide(OBSTACLE* pObst, QVEC cur_pos, QVEC prev_pos, float r, sys_ui32 mask, QVEC* pNew_pos) {
	QMTX m;
	QVEC pos;
	QVEC xz;
	QVEC dir;
	QVEC ck_dir;
	OBST_QUERY qry;
	int i, flg;
	static float rot[] = {0.0f, D_DEG2RAD(30), -D_DEG2RAD(30)};

	flg = 0;
	pos = cur_pos;
	dir = V4_normalize(V4_sub(pos, prev_pos));
	qry.mask = mask;
	for (i = 0; i < D_ARRAY_LENGTH(rot); ++i) {
		MTX_rot_y(m, rot[i]);
		ck_dir = MTX_calc_qvec(m, dir);
		qry.p0.qv = V4_set_w1(pos);
		qry.p1.qv = V4_set_w1(V4_add(pos, V4_scale(ck_dir, r)));
		xz = V4_zero();
		if (OBST_check(pObst, &qry)) {
			xz = V4_sub(qry.p1.qv, qry.hit_pos.qv);
			pos = V4_sub(pos, xz);
			flg = 1;
		}
	}
	*pNew_pos = V4_set(V4_at(pos, 0), V4_at(cur_pos, 1), V4_at(pos, 2), 1.0f);
	return flg;
}
