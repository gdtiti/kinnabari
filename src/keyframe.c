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
#include "keyframe.h"

KFR_HEAD* KFR_load(const char* name) {
	KFR_HEAD* pKfr;
	pKfr = SYS_load(name);
	return pKfr;
}

void KFR_free(KFR_HEAD* pKfr) {
	SYS_free(pKfr);
}

KFR_GROUP* KFR_get_grp(KFR_HEAD* pKfr, int grp_no) {
	KFR_GROUP* pGrp = NULL;
	if ((sys_uint)grp_no < pKfr->nb_grp) {
		pGrp = (KFR_GROUP*)D_INCR_PTR(pKfr, pKfr->grp_offs[grp_no]);
	}
	return pGrp;
}

const char* KFR_get_grp_name(KFR_HEAD* pKfr, KFR_GROUP* pGrp) {
	const char* pName = NULL;
	if (pKfr && pGrp) {
		pName = (const char*)D_INCR_PTR(pKfr, pGrp->name_offs);
	}
	return pName;
}

KFR_GROUP* KFR_search_grp(KFR_HEAD* pKfr, const char* name) {
	KFR_GROUP* pGrp = NULL;
	if (pKfr) {
		int i, n;
		n = pKfr->nb_grp;
		for (i = 0; i < n; ++i) {
			KFR_GROUP* pTst_grp = KFR_get_grp(pKfr, i);
			if (pTst_grp) {
				if (0 == strcmp(name, KFR_get_grp_name(pKfr, pTst_grp))) {
					pGrp = pTst_grp;
					break;
				}
			}
		}
	}
	return pGrp;
}

KFR_CHANNEL* KFR_get_chan(KFR_HEAD* pKfr, KFR_GROUP* pGrp, int chan_no) {
	KFR_CHANNEL* pCh = NULL;
	if (pKfr && pGrp && (sys_uint)chan_no < pGrp->nb_chan) {
		pCh = (KFR_CHANNEL*)D_INCR_PTR(pKfr, pGrp->ch_offs[chan_no]);
	}
	return pCh;
}

KFR_KEY* KFR_get_keys(KFR_HEAD* pKfr, KFR_CHANNEL* pCh) {
	KFR_KEY* pKey = NULL;
	if (pKfr && pCh) {
		pKey = (KFR_KEY*)D_ALIGN(&pCh->frm_no[pCh->nb_key], 4);
	}
	return pKey;
}

float KFR_eval(KFR_HEAD* pKfr, KFR_CHANNEL* pCh, float frame, sys_ui16* pState) {
	int fno, istart, iend;
	float t, f0, f1, v0, v1, outgoing, incoming;
	float max_frame = pKfr->max_frame;
	KFR_KEY* pKey = KFR_get_keys(pKfr, pCh);

	if (pCh->nb_key < 2) return pKey[0].val;
	if (frame > max_frame) return pKey[pCh->nb_key-1].val;
	fno = (int)frame;
	if (pState) {
		int cnt = pCh->nb_key;
		istart = *pState;
		if (istart >= pCh->nb_key) {
			istart = 0;
		}
		while (cnt) {
			if (fno == pCh->frm_no[istart]) {
				*pState = istart;
				return pKey[istart].val;
			}
			iend = istart + 1;
			if (iend > pCh->nb_key - 1) {
				istart = 0;
				iend = 1;
			}
			if (fno < pCh->frm_no[iend]) {
				break;
			}
			++istart;
			--cnt;
		}
	} else {
		istart = 0;
		iend = pCh->nb_key;
		do {
			int imid = (istart + iend) / 2;
			if (fno < pCh->frm_no[imid]) {
				iend = imid;
			} else {
				istart = imid;
			}
		} while (iend - istart >= 2);
	}
	if (pState) {
		*pState = istart;
	}
	if (frame == pCh->frm_no[istart]) {
		return pKey[istart].val;
	}
	iend = istart + 1;
	f0 = pCh->frm_no[istart];
	f1 = pCh->frm_no[iend];
	t = (frame - f0) / (f1 - f0);
	v0 = pKey[istart].val;
	outgoing = pKey[istart].rslope;
	v1 = pKey[iend].val;
	incoming = pKey[iend].lslope;
	return SPL_hermite(v0, outgoing, v1, incoming, t);
}

QVEC KFR_eval_grp(KFR_HEAD* pKfr, KFR_GROUP* pGrp, float frame) {
	UVEC res;
	int i;
	KFR_CHANNEL* pCh;

	res.qv = V4_zero();
	for (i = 0; i < 3; ++i) {
		pCh = KFR_get_chan(pKfr, pGrp, i);
		if (pCh) {
			res.f[i] = KFR_eval(pKfr, pCh, frame, NULL);
		}
	}
	return res.qv;
}

