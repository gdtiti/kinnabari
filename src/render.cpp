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

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <objbase.h>
#include <d3d9.h>

#include "system.h"
#include "calc.h"
#include "util.h"
#include "render.h"

#define D_PIX_BOOL_CK (16)
#define D_PIX_FVEC_CK (48)
#define D_VTX_FVEC_CK (64)

RDR_GPARAM g_rdr_param;

static ULONG Safe_release(IUnknown* pUnk);
static IDirect3DDevice9* Get_dev();
static void Set_prog_const_f(const UVEC* pData, sys_uint org, sys_uint count, bool vtx_flg);
static void Set_prog_const_i(const UVEC* pData, sys_uint org, sys_uint count, bool vtx_flg);
static void Set_prog_const_b(const sys_byte* pData, sys_uint org, sys_uint count, bool vtx_flg);
static void Set_sampler(RDR_SAMPLER* pSmp, sys_uint stage, bool vtx_flg);
static void Set_vtx_shader(IDirect3DVertexShader9* pShader);
static void Set_pix_shader(IDirect3DPixelShader9* pShader);
static RDR_TARGET* Rt_create(int w, int h, bool depth_flg, D3DFORMAT fmt, D3DFORMAT dfmt);
static RDR_TARGET* Rt_create_fp(int w, int h, bool mono_flg, D3DFORMAT fmt, D3DFORMAT dfmt);
static void Batch_exec(RDR_BATCH* pBatch);
static void Set_vb(RDR_VTX_BUFFER* pVB);
static void Set_rt(RDR_TARGET* pRT);

template <int N> struct CONST_CACHE {
	__m128 mState[N];

	void Init() {
		memset(mState, 0, sizeof(mState));
	}

	D_FORCE_INLINE sys_uint Check(const UVEC* pSrc, sys_uint org, sys_uint count) {
		sys_uint i, set_count;
		if (org + count > N) {
			set_count = count;
			if (org < N) {
				__m128* pDst = &mState[org];
				for (i = 0; i < N - org; ++i) {
					*pDst = pSrc->qv;
					++pSrc;
					++pDst;
				}
			}
		} else {
			__m128* pDst = &mState[org];
			set_count = 0;
			for (i = 0; i < count; ++i) {
				__m128 val128 = pSrc->qv;
				if (0xF != _mm_movemask_ps(_mm_cmpeq_ps(*pDst, val128))) {
					*pDst = val128;
					++set_count;
				}
				++pSrc;
				++pDst;
			}
		}
		return set_count;
	}
};

struct GPU_HEAD {
	sys_ui32 magic;
	sys_ui32 nb_vtx;
	sys_ui32 nb_pix;
	sys_ui32 sym_offs;
};

struct GPU_CODE {
	GPU_HEAD* mpHead;
	RDR_PROG_INFO* mpVtx_info;
	RDR_PROG_INFO* mpPix_info;
	RDR_PROG* mpVtx_prog;
	RDR_PROG* mpPix_prog;

	void Init() {
		int i, n;
		mpHead = (GPU_HEAD*)SYS_load("sys/prog.gpu");
		if (!mpHead) return;
		if (mpHead->magic != D_FOURCC('G','P','U','\0')) {
			SYS_log("Invalid GPU program file\n");
			return;
		}
		mpVtx_info = mpHead->nb_vtx ? (RDR_PROG_INFO*)(mpHead + 1) : NULL;
		mpPix_info = mpHead->nb_pix ? (mpVtx_info ? &mpVtx_info[mpHead->nb_vtx] : (RDR_PROG_INFO*)(mpHead + 1)) : NULL;
		mpVtx_prog = NULL;
		mpPix_prog = NULL;
		n = mpHead->nb_vtx;
		if (n) {
			mpVtx_prog = (RDR_PROG*)SYS_malloc(n*sizeof(RDR_PROG));
			for (i = 0; i < n; ++i) {
				mpVtx_prog[i].handle = NULL;
				mpVtx_prog[i].pInfo = mpVtx_info + i;
			}
		}
		n = mpHead->nb_pix;
		if (n) {
			mpPix_prog = (RDR_PROG*)SYS_malloc(n*sizeof(RDR_PROG));
			for (i = 0; i < n; ++i) {
				mpPix_prog[i].handle = NULL;
				mpPix_prog[i].pInfo = mpPix_info + i;
			}
		}
	}

	void Reset() {
		int i, n;
		Set_vtx_shader(NULL);
		Set_pix_shader(NULL);
		n = mpHead->nb_vtx;
		for (i = 0; i < n; ++i) {
			if (mpVtx_prog[i].handle) {
				((IUnknown*)mpVtx_prog[i].handle)->Release();
				mpVtx_prog[i].handle = NULL;
			}
		}
		n = mpHead->nb_pix;
		for (i = 0; i < n; ++i) {
			if (mpPix_prog[i].handle) {
				((IUnknown*)mpPix_prog[i].handle)->Release();
				mpPix_prog[i].handle = NULL;
			}
		}
	}

	RDR_GPARAM_INFO* Get_prog_param_info(RDR_PROG* pProg) {
		return (RDR_GPARAM_INFO*)D_INCR_PTR(mpHead, pProg->pInfo->param_offs);
	}

	RDR_PROG* Get_vtx_prog(sys_ui32 id) {
		RDR_PROG* pProg = NULL;
		if (id < mpHead->nb_vtx) {
			pProg = mpVtx_prog + id;
			if (!pProg->handle) {
				HRESULT hres = Get_dev()->CreateVertexShader((const DWORD*)D_INCR_PTR(mpHead, pProg->pInfo->code_offs), (IDirect3DVertexShader9**)&pProg->handle);
				if (FAILED(hres)) {
					SYS_log("Can't create vertex program #0x%X\n", id);
					pProg = NULL;
				}
			}
		}
		return pProg;
	}

	RDR_PROG* Get_pix_prog(sys_ui32 id) {
		RDR_PROG* pProg = NULL;
		if (id < mpHead->nb_pix) {
			pProg = mpPix_prog + id;
			if (!pProg->handle) {
				HRESULT hres = Get_dev()->CreatePixelShader((const DWORD*)D_INCR_PTR(mpHead, pProg->pInfo->code_offs), (IDirect3DPixelShader9**)&pProg->handle);
				if (FAILED(hres)) {
					SYS_log("Can't create pixel program #0x%X\n", id);
					pProg = NULL;
				}
			}
		}
		return pProg;
	}

	void Set_prog_params(RDR_PROG* pProg, bool vtx_flg) {
		UVEC vtmp[32];
		UVEC* pVec;
		sys_uint i, j;
		sys_uint n = pProg->pInfo->nb_param;
		RDR_GPARAM_INFO* pInfo = Get_prog_param_info(pProg);
		RDR_GPARAM* pGP = &g_rdr_param;

		if (!pProg) return;
		memset(vtmp, 0, sizeof(vtmp));
		for (i = 0; i < n; ++i) {
			switch (pInfo->id.type) {
				case E_RDR_PARAMTYPE_FVEC:
					if (D_RDR_GPTOP_FVEC >= 0) {
						pVec = (UVEC*)D_INCR_PTR(pGP, D_RDR_GPTOP_FVEC) + pInfo->id.offs;
						Set_prog_const_f((const UVEC*)pVec, pInfo->reg, pInfo->len, vtx_flg);
					}
					break;
				case E_RDR_PARAMTYPE_IVEC:
					if (D_RDR_GPTOP_IVEC >= 0) {
						pVec = (UVEC*)D_INCR_PTR(pGP, D_RDR_GPTOP_IVEC) + pInfo->id.offs;
						Set_prog_const_i((const UVEC*)pVec, pInfo->reg, pInfo->len, vtx_flg);
					}
					break;
				case E_RDR_PARAMTYPE_FLOAT:
					if (D_RDR_GPTOP_FLOAT >= 0) {
						float* pFloat = (float*)D_INCR_PTR(pGP, D_RDR_GPTOP_FLOAT) + pInfo->id.offs;
						for (j = 0; j < pInfo->len; ++j) {
							vtmp[j].f[0] = *pFloat;
							++pFloat;
						}
						Set_prog_const_f((const UVEC*)vtmp, pInfo->reg, pInfo->len, vtx_flg);
					}
					break;
				case E_RDR_PARAMTYPE_INT:
					if (D_RDR_GPTOP_INT >= 0) {
						sys_i32* pInt = (sys_i32*)D_INCR_PTR(pGP, D_RDR_GPTOP_INT) + pInfo->id.offs;
						for (j = 0; j < pInfo->len; ++j) {
							vtmp[j].i[0] = *pInt;
							++pInt;
						}
						Set_prog_const_i((const UVEC*)vtmp, pInfo->reg, pInfo->len, vtx_flg);
					}
					break;
				case E_RDR_PARAMTYPE_SMP:
					if (D_RDR_GPTOP_SMP >= 0) {
						RDR_SAMPLER* pSmp = (RDR_SAMPLER*)D_INCR_PTR(pGP, D_RDR_GPTOP_SMP) + pInfo->id.offs;
						Set_sampler(pSmp, pInfo->reg, vtx_flg);
					}
					break;
				case E_RDR_PARAMTYPE_BOOL:
					if (D_RDR_GPTOP_BOOL >= 0) {
						sys_byte* pBool = (sys_byte*)D_INCR_PTR(pGP, D_RDR_GPTOP_BOOL) + pInfo->id.offs;
						Set_prog_const_b((const sys_byte*)pBool, pInfo->reg, pInfo->len, vtx_flg);
					}
					break;
				default:
					break;
			}
			++pInfo;
		}
	}

	void Set_pipeline(sys_ui32 vtx_id, sys_ui32 pix_id) {
		RDR_PROG* pVtx_prog = Get_vtx_prog(vtx_id);
		RDR_PROG* pPix_prog = Get_pix_prog(pix_id);
		Set_prog_params(pVtx_prog, true);
		Set_prog_params(pPix_prog, false);
		if (pVtx_prog) {
			Set_vtx_shader((IDirect3DVertexShader9*)pVtx_prog->handle);
		}
		if (pPix_prog) {
			Set_pix_shader((IDirect3DPixelShader9*)pPix_prog->handle);
		}
	}
};


struct RDR_LAYER;
typedef void (*RDR_LYR_FUNC)(RDR_LAYER* pLyr);

struct RDR_LAYER {
	RDR_BATCH** mppBatch;
	sys_ui32*   mpKey;
	LONG mCount;
	int mSize;
	const char* mName;
	RDR_LYR_FUNC mpPrologue;
	RDR_LYR_FUNC mpEpilogue;

	void Alloc(const char* name, int n, bool sorted) {
		mName = name;
		mSize = n;
		mppBatch = (RDR_BATCH**)SYS_malloc(n*sizeof(RDR_BATCH*));
		mpKey = NULL;
		if (sorted) {
			mpKey = (sys_ui32*)SYS_malloc(n*sizeof(sys_ui32));
		}
		mCount = 0;
		mpPrologue = NULL;
		mpEpilogue = NULL;
	}

	void Reset() {
		mCount = 0;
	}

	void Put(RDR_BATCH* pEntry, sys_ui32 key) {
		LONG idx = _InterlockedExchangeAdd(&mCount, 1);
		if (idx < mSize) {
			mppBatch[idx] = pEntry;
			if (mpKey) {
				mpKey[idx] = key;
			}
		} else {
			SYS_log("Layer overflow [%s]\n", mName);
		}
	}

	void Exec() {
		int i, n;
		RDR_BATCH** ppBatch = mppBatch;
		n = mCount;
		//if (n <= 0) return;
		if (mpPrologue) {
			mpPrologue(this);
		}
		for (i = 0; i < n; ++i) {
			if (*ppBatch) {
				Batch_exec(*ppBatch);
			}
			++ppBatch;
		}
		if (mpEpilogue) {
			mpEpilogue(this);
		}
	}
};

struct RDR_LYR_WK {
	RDR_LAYER mLyr[E_RDR_LAYER_MAX];

	void Init() {
		int n = 32768;
		mLyr[E_RDR_LAYER_ZBUF].Alloc("ZBUF", n, false);
		mLyr[E_RDR_LAYER_CAST].Alloc("CAST", n, false);
		mLyr[E_RDR_LAYER_MTL0].Alloc("MTL0", n, false);
		mLyr[E_RDR_LAYER_MTL1].Alloc("MTL1", n, true);
		mLyr[E_RDR_LAYER_RECV].Alloc("RECV", n, false);
		mLyr[E_RDR_LAYER_MTL2].Alloc("MTL2", n, true);
		mLyr[E_RDR_LAYER_PTCL].Alloc("PTCL", n, true);
	}

	void Reset() {
		for (int i = 0; i < D_ARRAY_LENGTH(mLyr); ++i) {
			mLyr[i].Reset();
		}
	}

	void Exec() {
		for (int i = 0; i < D_ARRAY_LENGTH(mLyr); ++i) {
			mLyr[i].Exec();
		}
	}
};

struct RDR_BATCH_WK {
	RDR_BATCH* mpBatch;
	LONG mCount;
	int mSize;

	void Alloc(int n) {
		mSize = n;
		mpBatch = (RDR_BATCH*)SYS_malloc(n*sizeof(RDR_BATCH));
		mCount = 0;
	}

	void Reset() {
		mCount = 0;
	}

	RDR_BATCH* Get() {
		RDR_BATCH* pRes = NULL;
		LONG idx = _InterlockedExchangeAdd(&mCount, 1);
		if (idx < mSize) {
			pRes = mpBatch + idx;
		} else {
			SYS_log("Batch work overflow\n");
		}
		return pRes;
	}
};

struct RDR_PARAM_WK {
	RDR_BATCH_PARAM* mpParam;
	LONG mCount;
	int mSize;

	void Alloc(int n) {
		mSize = n;
		mpParam = (RDR_BATCH_PARAM*)SYS_malloc(n*sizeof(RDR_BATCH_PARAM));
		mCount = 0;
	}

	void Reset() {
		mCount = 0;
	}

	RDR_BATCH_PARAM* Get(int n) {
		RDR_BATCH_PARAM* pParam = NULL;
		LONG idx = _InterlockedExchangeAdd(&mCount, n);
		if (idx < mSize) {
			pParam = mpParam + idx;
		} else {
			SYS_log("Param work overflow\n");
		}
		return pParam;
	}
};

struct RDR_VAL_WK {
	__m128* mpMem;
	LONG mCount;
	int mSize;

	void Alloc(int n) {
		mSize = n;
		mpMem = (__m128*)SYS_malloc(n*sizeof(__m128));
		mCount = 0;
	}

	void Reset() {
		mCount = 0;
	}

	void* Get(int data_size) {
		void* pRes = NULL;
		data_size = (int)D_ALIGN(data_size, 16);
		int incr = data_size / 16;
		LONG idx = _InterlockedExchangeAdd(&mCount, incr);
		if (idx < mSize) {
			pRes = mpMem + idx;
		} else {
			SYS_log("Value work overflow\n");
		}
		return pRes;
	}

	void* Get_items(E_RDR_PARAMTYPE type, int nb_item) {
		void* pRes = NULL;
		int item_size = 0;
		switch (type) {
			case E_RDR_PARAMTYPE_FVEC:
			case E_RDR_PARAMTYPE_IVEC:
				item_size = sizeof(UVEC);
				break;
			case E_RDR_PARAMTYPE_INT:
				item_size = sizeof(sys_i32);
				break;
			case E_RDR_PARAMTYPE_FLOAT:
				item_size = sizeof(float);
				break;
			case E_RDR_PARAMTYPE_SMP:
				item_size = sizeof(RDR_SAMPLER);
				break;
			case E_RDR_PARAMTYPE_BOOL:
				item_size = sizeof(sys_byte);
				break;
			default:
				break;
		}
		if (item_size) {
			pRes = Get(nb_item*item_size);
		}
		return pRes;
	}
};

struct RDR_DB_WK {
	int mIdx_db;
	RDR_LYR_WK mLyr_wk[2];
	RDR_BATCH_WK mBatch_wk[2];
	RDR_PARAM_WK mParam_wk[2];
	RDR_VAL_WK mVal_wk[2];
	RDR_VIEW mView[2];
	UVEC mShadow_dir[2];

	void Init() {
		mIdx_db = 0;
		mLyr_wk[0].Init();
		mLyr_wk[1].Init();

		int n = 16384;
		mBatch_wk[0].Alloc(n);
		mBatch_wk[1].Alloc(n);

		n = 8192;
		mVal_wk[0].Alloc(n);
		mVal_wk[1].Alloc(n);

		n = 8192;
		mParam_wk[0].Alloc(n);
		mParam_wk[1].Alloc(n);

		RDR_VIEW* pView = &mView[0];
		pView->pos.qv = V4_set_pnt(7.0f, 3.0f, 12.0f);
		MTX_make_view(pView->view, pView->pos.qv, V4_set_w1(V4_zero()), V4_load(g_identity[1]));
		MTX_invert(pView->iview, pView->view);
		pView->dir.qv = V4_scale(V4_load(pView->iview[2]), -1.0f);
		MTX_make_proj(pView->proj, D_DEG2RAD(40.0f), 320.0f/240.0f, 0.1f, 10000.0f);
		MTX_invert(pView->iproj, pView->proj);
		MTX_mul(pView->view_proj, pView->view, pView->proj);
		memcpy(&mView[1], pView, sizeof(RDR_VIEW));

		mShadow_dir[0].qv = V4_normalize(V4_set_vec(-0.15f, -0.9f, -0.6f));
		mShadow_dir[1].qv = mShadow_dir[0].qv;
	}

	void Begin() {
		int idx = mIdx_db;
		mLyr_wk[idx].Reset();
		mBatch_wk[idx].Reset();
		mParam_wk[idx].Reset();
		mVal_wk[idx].Reset();
	}

	void* Get_val(E_RDR_PARAMTYPE type, int nb_item) {
		int idx = mIdx_db;
		return mVal_wk[idx].Get_items(type, nb_item);
	}

	RDR_BATCH_PARAM* Get_param(int n) {
		int idx = mIdx_db;
		return mParam_wk[idx].Get(n);
	}

	RDR_BATCH* Get_batch() {
		int idx = mIdx_db;
		return mBatch_wk[idx].Get();
	}

	void Put_batch(RDR_BATCH* pBatch, sys_ui32 key, sys_uint lyr_no) {
		int idx = mIdx_db;
		mLyr_wk[idx].mLyr[lyr_no].Put(pBatch, key);
	}

	void Exec() {
		int idx = mIdx_db ^ 1;
		mLyr_wk[idx].Exec();
	}

	RDR_LAYER* Get_exec_lyr(sys_uint lyr_no) {
		int idx = mIdx_db ^ 1;
		return &mLyr_wk[idx].mLyr[lyr_no];
	}

	RDR_VIEW* Get_work_view() {
		int idx = mIdx_db;
		return &mView[idx];
	}

	RDR_VIEW* Get_exec_view() {
		int idx = mIdx_db ^ 1;
		return &mView[idx];
	}

	QVEC Get_exec_shadow_dir() {
		int idx = mIdx_db ^ 1;
		return mShadow_dir[idx].qv;
	}

	void Apply_exec_view() {
		QMTX tm;
		int idx = mIdx_db ^ 1;
		MTX_transpose(tm, mView[idx].view_proj);
		g_rdr_param.view_proj[0].qv = V4_load(tm[0]);
		g_rdr_param.view_proj[1].qv = V4_load(tm[1]);
		g_rdr_param.view_proj[2].qv = V4_load(tm[2]);
		g_rdr_param.view_proj[3].qv = V4_load(tm[3]);
		g_rdr_param.view_pos.qv = mView[idx].pos.qv;
	}

	void Flip() {
		mIdx_db ^= 1;
	}
};


template <typename _T> struct RSRC_BLOCK {
	int             bitmask;
	RSRC_BLOCK<_T>* pNext;
	RSRC_BLOCK<_T>* pPrev;
	_T              data[sizeof(int)*8];

	static RSRC_BLOCK<_T>* Alloc_head() {
		RSRC_BLOCK<_T>* pHead = (RSRC_BLOCK<_T>*)SYS_malloc(sizeof(RSRC_BLOCK<_T>));
		pHead->bitmask = 0;
		pHead->pPrev = NULL;
		pHead->pNext = NULL;
		return pHead;
	}

	static void Free_all(RSRC_BLOCK<_T>* pHead) {
		RSRC_BLOCK<_T>* p = pHead;
		while (p) {
			RSRC_BLOCK<_T>* pNext = p->pNext;
			SYS_free(p);
			p = pNext;
		}
	}

	static _T* Add(RSRC_BLOCK<_T>* pHead, _T& item) {
		bool found = false;
		RSRC_BLOCK<_T>* pBlk = pHead;
		while (!found) {
			if (pBlk->bitmask != (int)-1) {
				found = true;
				continue;
			}
			if (pBlk->pNext) {
				pBlk = pBlk->pNext;
			} else {
				RSRC_BLOCK<_T>* pNew = (RSRC_BLOCK<_T>*)SYS_malloc(sizeof(RSRC_BLOCK<_T>));
				pNew->bitmask = 0;
				pNew->pPrev = pBlk;
				pNew->pNext = NULL;
				pBlk->pNext = pNew;
				pBlk = pNew;
				found = true;
			}
		}
#if 0
		int mask = 1;
		int idx = 0;
		while (pBlk->bitmask & mask) {
			mask <<= 1;
			++idx;
		}
#else
		DWORD idx;
#	if defined(__INTEL_COMPILER)
		idx = _bit_scan_forward(pBlk->bitmask ^ (int)-1);
#	else
		_BitScanForward(&idx, pBlk->bitmask ^ (int)-1);
#	endif
		int mask = 1 << idx;
#endif
		pBlk->bitmask |= mask;
		pBlk->data[idx] = item;
		return &pBlk->data[idx];
	}

	static void Remove(RSRC_BLOCK<_T>* pHead, _T* pItem) {
		bool found = false;
		RSRC_BLOCK<_T>* pBlk = pHead;
		while (pBlk && !found) {
			if (pItem >= pBlk->data && pItem < &pBlk->data[D_ARRAY_LENGTH(pBlk->data)]) {
				found = true;
			} else {
				pBlk = pBlk->pNext;
			}
		}
		if (pBlk && found) {
			int idx = (int)(pItem - pBlk->data);
			pBlk->bitmask &= ~(1<<idx);
			if (!s_rdr_keep_mem && pBlk != pHead && pBlk->bitmask == 0) {
				RSRC_BLOCK<_T>* pPrev = pBlk->pPrev;
				RSRC_BLOCK<_T>* pNext = pBlk->pNext;
				if (pPrev) {
					pPrev->pNext = pNext;
				}
				if (pNext) {
					pNext->pPrev = pPrev;
				}
				SYS_free(pBlk);
			}
		}
	}
};

template <typename _T> struct RSRC_LIST {
	_T* mpHead;
	_T* mpCurr;
	_T* mpTail;

	typedef void (*FOREACH_FUNC)(_T* pItem, void* pData);

	RSRC_LIST() : mpHead(NULL), mpCurr(NULL), mpTail(NULL) {}

	void Remove(_T item) {
		if (mpCurr != mpHead) {
			_T* p = mpHead;
			while (p != mpCurr) {
				if (*p == item) {
					while (++p != mpCurr) {
						p[-1] = *p;
					}
					--mpCurr;
					return;
				}
				++p;
			}
		}
	}

	void Reset() {
		if (mpHead) SYS_free(mpHead);
		mpHead = NULL;
		mpCurr = NULL;
		mpTail = NULL;
	}

	void Add(_T item) {
		if (mpTail - mpCurr < 1) {
			int count = (int)(mpHead ? mpCurr - mpHead : 0);
			int old_count = count;
			if (count < 1) count = 1;
			if (count > 4096) {
				count += 32;
			} else {
				count += count;
			}
			while (count < 32) {
				++count;
			}
			int size = count*sizeof(_T);
			_T* pNew = (_T*)SYS_malloc(size);
			memset(pNew, 0, size);
			if (mpHead) memcpy(pNew, mpHead, old_count*sizeof(_T));
			Reset();
			mpHead = pNew;
			mpCurr = mpHead + old_count;
			mpTail = mpHead + count;
		}
		*mpCurr++ = item;
	}

	void Foreach(FOREACH_FUNC pFunc, void* pData) {
		if (mpHead) {
			_T* p = mpHead;
			_T* pEnd = (mpCurr != NULL ? mpCurr : mpHead);
			while (p != pEnd) {
				pFunc(p, pData);
				++p;
			}
		}
	}

	int Get_count() {
		int n = 0;
		if (mpHead) {
			_T* pEnd = (mpCurr != NULL ? mpCurr : mpHead);
			n = (int)(pEnd - mpHead);
		}
		return n;
	}
};

template <typename _T> struct RSRC_STORE {
	RSRC_BLOCK<_T>* mpBlk;
	RSRC_LIST<_T*> mLst;
	const char* mName;

	void Init(const char* name) {
		mName = name;
		mpBlk = RSRC_BLOCK<_T>::Alloc_head();
	}

	_T* Add(_T& item) {
		_T* p = RSRC_BLOCK<_T>::Add(mpBlk, item);
		mLst.Add(p);
		return p;
	}

	void Remove(_T* p) {
		mLst.Remove(p);
		RSRC_BLOCK<_T>::Remove(mpBlk, p);
	}

	void Release(typename RSRC_LIST<_T*>::FOREACH_FUNC pFunc) {
		int n = mLst.Get_count();
		if (n) SYS_log("Releasing [%s] resources, # items = %d\n", mName, n);
		mLst.Foreach(pFunc, NULL);
		mLst.Reset();
		RSRC_BLOCK<_T>::Free_all(mpBlk);
		mpBlk = NULL;
	}
};

struct RDR_RSRC {
	RSRC_STORE<RDR_VTX_BUFFER> mVtx;
	RSRC_STORE<RDR_IDX_BUFFER> mIdx;
	RSRC_STORE<RDR_TEXTURE> mTex;
	RSRC_STORE<RDR_TARGET> mTgt;

	void Init() {
		mVtx.Init("VTX");
		mIdx.Init("IDX");
		mTex.Init("TEX");
		mTgt.Init("TGT");
	}

	static void Vtx_release(RDR_VTX_BUFFER** ppVB, void* pData) {
		if (!ppVB) return;
		RDR_VTX_BUFFER* pVB = *ppVB;
		if (pVB && pVB->handle) {
			ULONG refs = Safe_release((IUnknown*)pVB->handle);
			if (refs == 0) pVB->handle = NULL;
		}
	}

	static void Idx_release(RDR_IDX_BUFFER** ppIB, void* pData) {
		if (!ppIB) return;
		RDR_IDX_BUFFER* pIB = *ppIB;
		if (pIB && pIB->handle) {
			ULONG refs = Safe_release((IUnknown*)pIB->handle);
			if (refs == 0) pIB->handle = NULL;
		}
	}

	static void Tex_release(RDR_TEXTURE** ppTex, void* pData) {
		if (!ppTex) return;
		RDR_TEXTURE* pTex = *ppTex;
		if (pTex && pTex->handle) {
			ULONG refs = Safe_release((IUnknown*)pTex->handle);
			if (refs == 0) pTex->handle = NULL;
		}
	}

	static void Tgt_release(RDR_TARGET** ppRT, void* pData) {
		if (!ppRT) return;
		RDR_TARGET* pRT = *ppRT;
		if (pRT) {
			ULONG refs;
			if (pRT->hDepth) {
				refs = Safe_release((IUnknown*)pRT->hDepth);
				if (refs == 0) {
					pRT->hDepth = NULL;
				}
			}
			if (pRT->hTgt) {
				refs = Safe_release((IUnknown*)pRT->hTgt);
				if (refs == 0) {
					pRT->hTgt = NULL;
				}
			}
		}
	}

	void Release() {
		mVtx.Release(Vtx_release);
		mIdx.Release(Idx_release);
		mTex.Release(Tex_release);
		mTgt.Release(Tgt_release);
	}
};

struct RDR_DECL {
	IDirect3DVertexDeclaration9* pGen;
	IDirect3DVertexDeclaration9* pSolid;
	IDirect3DVertexDeclaration9* pSkin;
	IDirect3DVertexDeclaration9* pScreen;
};

struct RDR_TGT_LIST {
	RDR_TARGET* mpScene;
	RDR_TARGET* mpDepth;
	RDR_TARGET* mpBloom;
	RDR_TARGET* mpShadow;
	RDR_TARGET* mpLUT;
	RDR_TARGET* mpReduced[4][2];

	static RDR_TARGET* Make_aux(int w, int h) {
		return Rt_create(w, h, /*no depth*/false, /*same as back*/D3DFMT_UNKNOWN, D3DFMT_UNKNOWN);
	}

	void Init(int w, int h, int smap_size) {
		mpScene = Make_aux(w, h);
		mpDepth = Make_aux(w, h);
		mpBloom = Make_aux(w, h);
		mpLUT = Make_aux(0x100, 1);
		mpShadow = Rt_create_fp(smap_size, smap_size, true, D3DFMT_R32F, D3DFMT_D24S8);
	}
};

struct COLOR_CORRECT {
	QMTX mtx;
	UVEC in_black;
	UVEC in_white;
	UVEC out_black;
	UVEC out_white;
	UVEC gamma;
	float lum_scale;
	float lum_bias;
	sys_byte mode;

	void Init() {
		MTX_unit(mtx);
		in_black.qv = V4_set_w1(V4_zero());
		in_white.qv = V4_fill(1.0f);
		out_black.qv = in_black.qv;
		out_white.qv = in_white.qv;
		gamma.qv = V4_fill(1.0f);
		lum_scale = 1.0f;
		lum_bias = 0.0f;
		mode = E_RDR_CCMODE_NONE;
	}
};

struct RDR_IMG {
	RDR_VTX_BUFFER* mpVB;
	COLOR_CORRECT mCC;

	void Init() {
		static QVEC quad_pos[] = {
			{-1, 1, 0, 1},
			{-1, -1, 0, 1},
			{1, 1, 0, 1},
			{1, -1, 0, 1}
		};
		static QVEC quad_uv[] = {
			{0, 0, 0, 0},
			{0, 1, 0, 0},
			{1, 0, 0, 0},
			{1, 1, 0, 0}
		};

		mpVB = RDR_vtx_create_dyn(E_RDR_VTXTYPE_SCREEN, 32);
		if (mpVB) {
			RDR_vtx_lock(mpVB);
			if (mpVB->locked.pScreen) {
				int i;
				RDR_VTX_SCREEN* pScr = mpVB->locked.pScreen;
				for (i = 0; i < 4; ++i) {
					UVEC pos;
					UVEC tex;
					pos.qv = V4_add(quad_pos[i], V4_set(-1e-3f, 2e-3f, 0.0f, 0.0f));
					pScr->pos[0] = pos.x;
					pScr->pos[1] = pos.y;
					pScr->pos[2] = pos.z;
					pScr->clr.argb32 = 0xFFFFFFFF;
					tex.qv = quad_uv[i];
					tex.z = tex.x;
					tex.w = tex.y;
					pScr->tex[0] = tex.x;
					pScr->tex[1] = tex.y;
					pScr->tex[2] = tex.z;
					pScr->tex[3] = tex.w;
					++pScr;
				}
			}
			RDR_vtx_unlock(mpVB);
		}
		mCC.Init();
	}

	void Set_in_black(float r, float g, float b) {mCC.in_black.qv = V4_set_pnt(r, g, b);}
	void Set_out_black(float r, float g, float b) {mCC.out_black.qv = V4_set_pnt(r, g, b);}
	void Set_in_white(float r, float g, float b) {mCC.in_white.qv = V4_set_pnt(r, g, b);}
	void Set_out_white(float r, float g, float b) {mCC.out_white.qv = V4_set_pnt(r, g, b);}
	void Set_gamma(float r, float g, float b) {mCC.gamma.qv = V4_set_pnt(r, g, b);}
	void Set_gamma_val(float val) {Set_gamma(val, val, val);}

	void Apply_levels() {
		RDR_GPARAM* pGP = &g_rdr_param;
		QVEC one = V4_fill(1.0f);
		pGP->in_black.qv = mCC.in_black.qv;
		pGP->out_black.qv = mCC.out_black.qv;
		pGP->in_inv_range.qv = V4_div(one, V4_set_w1(V4_sub(mCC.in_white.qv, mCC.in_black.qv)));
		pGP->out_range.qv = V4_sub(mCC.out_white.qv, mCC.out_black.qv);
		pGP->inv_gamma.qv = V4_div(one, mCC.gamma.qv);
	}

	void Apply_linear() {
		QMTX tm;
		RDR_GPARAM* pGP = &g_rdr_param;
		MTX_transpose(tm, mCC.mtx);
		pGP->cc_mtx[0].qv = V4_load(tm[0]);
		pGP->cc_mtx[1].qv = V4_load(tm[1]);
		pGP->cc_mtx[2].qv = V4_load(tm[2]);
	}

	void Apply_lum() {
		RDR_GPARAM* pGP = &g_rdr_param;
		pGP->lum_param.qv = V4_set(mCC.lum_scale, mCC.lum_bias, 0.0f, 0.0f);
	}

	void Draw_quad(int vtx_id, int pix_id, int start) {
		RDR_BATCH batch;
		batch.pVtx = mpVB;
		batch.pIdx = NULL;
		batch.pParam = NULL;
		batch.count = 2;
		batch.type = E_RDR_PRIMTYPE_TRISTRIP;
		batch.start = start;
		batch.base = 0;
		batch.blend_state.on = false;
		batch.draw_state.color_mask = 0xF;
		batch.draw_state.zwrite = false;
		batch.draw_state.ztest = false;
		batch.draw_state.cull = E_RDR_CULL_NONE;
		batch.draw_state.atest = false;
		batch.draw_state.msaa = false;
		batch.vtx_prog = vtx_id;
		batch.pix_prog = pix_id;
		batch.nb_param = 0;
		Batch_exec(&batch);
	}
};


struct RDR_WORK {
	IDirect3D9* mpD3D;
	IDirect3DDevice9* mpDev;
	D3DFORMAT mAdapter_fmt;
	int max_tex_stg;

	RDR_RSRC mRsrc;
	RDR_DECL mDecl;
	RDR_TGT_LIST mRT;
	RDR_TARGET mMain_rt;
	GPU_CODE mGpu_code;
	RDR_IMG mImg;

	QMTX mShadow_view_proj;
	QMTX mShadow_mtx;
	QVEC mShadow_dir;

	IDirect3DVertexShader9* mpCurr_vtx_shader;
	IDirect3DPixelShader9* mpCurr_pix_shader;

	sys_ui32 mClear_color;

#if (D_VTX_FVEC_CK > 0)
	CONST_CACHE<D_VTX_FVEC_CK> mVtx_fvec_cache;
#endif
#if (D_PIX_FVEC_CK > 0)
	CONST_CACHE<D_PIX_FVEC_CK> mPix_fvec_cache;
#endif
#if (D_PIX_BOOL_CK > 0)
	D_BIT_ARY(mPix_bool_cache, D_PIX_BOOL_CK);
#endif

	RDR_DB_WK mDb_wk;

	void Init() {
#if (D_VTX_FVEC_CK > 0)
		mVtx_fvec_cache.Init();
#endif
#if (D_PIX_FVEC_CK > 0)
		mPix_fvec_cache.Init();
#endif

		mRsrc.Init();
		mRT.Init(mMain_rt.w, mMain_rt.h, 1024);
		mDb_wk.Init();
		mGpu_code.Init();
		mImg.Init();

		mClear_color = D_RDR_ARGB32(0xFF, 0x55, 0x66, 0x77);
	}

	void Set_vtx_const_f(const UVEC* pData, sys_uint org, sys_uint count) {
		HRESULT hres;
#if (D_VTX_FVEC_CK > 0)
		int set_count = mVtx_fvec_cache.Check(pData, org, count);
		if (set_count) {
			hres = mpDev->SetVertexShaderConstantF(org, (const float*)pData, count);
		} else {
			hres = D3D_OK;
		}
#else
		hres = mpDev->SetVertexShaderConstantF(org, (const float*)pData, count);
#endif
	}

	void Set_vtx_const_i(const UVEC* pData, sys_uint org, sys_uint count) {
		HRESULT hres = mpDev->SetVertexShaderConstantI(org, (const int*)pData, count);
	}

	void Set_vtx_const_b(const BOOL* pData, sys_uint org, sys_uint count) {
		HRESULT hres = mpDev->SetVertexShaderConstantB(org, pData, count);
	}

	void Set_pix_const_f(const UVEC* pData, sys_uint org, sys_uint count) {
		HRESULT hres;
#if (D_PIX_FVEC_CK > 0)
		int set_count = mPix_fvec_cache.Check(pData, org, count);
		if (set_count) {
			hres = mpDev->SetPixelShaderConstantF(org, (const float*)pData, count);
		} else {
			hres = D3D_OK;
		}
#else
		hres = mpDev->SetPixelShaderConstantF(org, (const float*)pData, count);
#endif
	}

	void Set_pix_const_i(const UVEC* pData, sys_uint org, sys_uint count) {
		HRESULT hres = mpDev->SetPixelShaderConstantI(org, (const int*)pData, count);
	}

	void Set_pix_const_b(const BOOL* pData, sys_uint org, sys_uint count) {
		HRESULT hres;
#if (D_PIX_BOOL_CK > 0)
		sys_uint i;
		int set_count = 0;
		for (i = 0; i < count; ++i) {
			sys_uint idx = org + i;
			bool flg = D_BIT_CK(mPix_bool_cache, idx);
			if (flg != !!pData[i]) {
				if (pData[i]) {
					D_BIT_ST(mPix_bool_cache, idx);
				} else {
					D_BIT_CL(mPix_bool_cache, idx);
				}
				++set_count;
			}
		}
		if (set_count) {
			hres = mpDev->SetPixelShaderConstantB(org, pData, count);
		} else {
			hres = D3D_OK;
		}
#else
		hres = mpDev->SetPixelShaderConstantB(org, pData, count);
#endif
	}

	void Set_vb(RDR_VTX_BUFFER* pVB) {
		if (pVB) {
			IDirect3DVertexDeclaration9* pDecl = NULL;
			switch (pVB->type) {
				case E_RDR_VTXTYPE_GENERAL:
					pDecl = mDecl.pGen;
					break;
				case E_RDR_VTXTYPE_SOLID:
					pDecl = mDecl.pSolid;
					break;
				case E_RDR_VTXTYPE_SKIN:
					pDecl = mDecl.pSkin;
					break;
				case E_RDR_VTXTYPE_SCREEN:
					pDecl = mDecl.pScreen;
					break;
				default:
					break;
			}
			if (pDecl) {
				mpDev->SetVertexDeclaration(pDecl);
				mpDev->SetStreamSource(0, (IDirect3DVertexBuffer9*)pVB->handle, 0, pVB->vtx_size);
			} else {
				mpDev->SetStreamSource(0, NULL, 0, 0);
			}
		} else {
			mpDev->SetStreamSource(0, NULL, 0, 0);
		}
	}

	void Set_ib(RDR_IDX_BUFFER* pIB) {
		if (pIB) {
			mpDev->SetIndices((IDirect3DIndexBuffer9*)pIB->handle);
		} else {
			mpDev->SetIndices(NULL);
		}
	}
};

static RDR_WORK s_rdr;

static bool s_rdr_keep_mem = false;

ULONG Safe_release(IUnknown* pUnk) {
	ULONG i, refs, refs0;
	refs = 0;
	if (pUnk) {
		refs = pUnk->AddRef();
		refs0 = refs;
		for (i = 0; i < refs0; ++i) {
			refs = pUnk->Release();
		}
	}
	return refs;
}

IDirect3DDevice9* Get_dev() {
	return s_rdr.mpDev;
}

void Set_prog_const_f(const UVEC* pData, sys_uint org, sys_uint count, bool vtx_flg) {
	if (vtx_flg) {
		s_rdr.Set_vtx_const_f(pData, org, count);
	} else {
		s_rdr.Set_pix_const_f(pData, org, count);
	}
}

void Set_prog_const_i(const UVEC* pData, sys_uint org, sys_uint count, bool vtx_flg) {
	if (vtx_flg) {
		s_rdr.Set_vtx_const_i(pData, org, count);
	} else {
		s_rdr.Set_pix_const_i(pData, org, count);
	}
}

void Set_prog_const_b(const sys_byte* pData, sys_uint org, sys_uint count, bool vtx_flg) {
	BOOL b[32];
	for (sys_uint i = 0; i < count; ++i) {
		b[i] = !!pData[i];
	}
	if (vtx_flg) {
		s_rdr.Set_vtx_const_b(b, org, count);
	} else {
		s_rdr.Set_pix_const_b(b, org, count);
	}
}

static D3DTEXTUREADDRESS Xlat_tex_addr(sys_ui32 tex_addr) {
	D3DTEXTUREADDRESS d3d_tex_addr = (D3DTEXTUREADDRESS)0;
	switch (tex_addr) {
		case E_RDR_TEXADDR_WRAP:
			d3d_tex_addr = D3DTADDRESS_WRAP;
			break;
		case E_RDR_TEXADDR_MIRROR:
			d3d_tex_addr = D3DTADDRESS_MIRROR;
			break;
		case E_RDR_TEXADDR_CLAMP:
			d3d_tex_addr = D3DTADDRESS_CLAMP;
			break;
		case E_RDR_TEXADDR_MIRCLAMP:
			d3d_tex_addr = D3DTADDRESS_MIRRORONCE;
			break;
		case E_RDR_TEXADDR_BORDER:
			d3d_tex_addr = D3DTADDRESS_BORDER;
			break;
	}
	return d3d_tex_addr;
}

static D3DTEXTUREFILTERTYPE Xlat_tex_filter(sys_ui32 filt) {
	D3DTEXTUREFILTERTYPE d3d_filt = (D3DTEXTUREFILTERTYPE)-1;
	switch (filt) {
		case E_RDR_TEXFILTER_POINT:
			d3d_filt = D3DTEXF_POINT;
			break;
		case E_RDR_TEXFILTER_LINEAR:
			d3d_filt = D3DTEXF_LINEAR;
			break;
		case E_RDR_TEXFILTER_ANISOTROPIC:
			d3d_filt = D3DTEXF_ANISOTROPIC;
			break;
		default:
			d3d_filt = D3DTEXF_POINT;
			break;
	}
	return d3d_filt;
}

void Set_sampler(RDR_SAMPLER* pSmp, sys_uint stage, bool vtx_flg) {
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	union {
		float f32;
		sys_ui32 u32;
	} d32;

	if (vtx_flg) {
		stage += D3DVERTEXTEXTURESAMPLER0;
	}
	d32.f32 = pSmp->mip_bias;
	pDev->SetSamplerState(stage, D3DSAMP_MIPMAPLODBIAS, d32.u32);
	pDev->SetSamplerState(stage, D3DSAMP_BORDERCOLOR, pSmp->border.argb32);
	pDev->SetSamplerState(stage, D3DSAMP_ADDRESSU, Xlat_tex_addr(pSmp->addr_u));
	pDev->SetSamplerState(stage, D3DSAMP_ADDRESSV, Xlat_tex_addr(pSmp->addr_v));
	pDev->SetSamplerState(stage, D3DSAMP_ADDRESSW, Xlat_tex_addr(pSmp->addr_w));
	pDev->SetSamplerState(stage, D3DSAMP_MINFILTER, Xlat_tex_filter(pSmp->min));
	pDev->SetSamplerState(stage, D3DSAMP_MAGFILTER, Xlat_tex_filter(pSmp->mag));
	pDev->SetSamplerState(stage, D3DSAMP_MIPFILTER, Xlat_tex_filter(pSmp->mip));
	pDev->SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, pSmp->max_mip);
	pDev->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, pSmp->anisotropy);
	pDev->SetTexture(stage, (IDirect3DBaseTexture9*)pSmp->hTex);
}

void Set_vtx_shader(IDirect3DVertexShader9* pShader) {
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	if (pShader != pRdr->mpCurr_vtx_shader) {
		pRdr->mpCurr_vtx_shader = pShader;
		pDev->SetVertexShader(pShader);
	}
}

void Set_pix_shader(IDirect3DPixelShader9* pShader) {
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	if (pShader != pRdr->mpCurr_pix_shader) {
		pRdr->mpCurr_pix_shader = pShader;
		pDev->SetPixelShader(pShader);
	}
}

RDR_TARGET* Rt_create(int w, int h, bool depth_flg, D3DFORMAT fmt, D3DFORMAT dfmt) {
	HRESULT hres;
	RDR_TARGET rt;
	RDR_TARGET* pRT;
	IDirect3DSurface9* pBack_surf;
	IDirect3DSurface9* pDepth_surf;
	D3DSURFACE_DESC back_desc;
	D3DSURFACE_DESC depth_desc;
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	if (!pDev) return NULL;
	pRT = NULL;
	memset(&rt, 0, sizeof(RDR_TARGET));
	pBack_surf = NULL;
	pDepth_surf = NULL;

	if (fmt == D3DFMT_UNKNOWN) {
		hres = pDev->GetRenderTarget(0, &pBack_surf);
		if (FAILED(hres)) goto _exit;
		hres = pBack_surf->GetDesc(&back_desc);
		if (FAILED(hres)) goto _exit;
		if (w == 0) w = back_desc.Width;
		if (h == 0) h = back_desc.Height;
		fmt = back_desc.Format;
	}
	hres = pDev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, (IDirect3DTexture9**)&rt.hTgt, NULL);
	if (FAILED(hres)) goto _exit;
	if (depth_flg) {
		if (dfmt == D3DFMT_UNKNOWN) {
			hres = pDev->GetDepthStencilSurface(&pDepth_surf);
			if (FAILED(hres)) goto _exit;
			hres = pDepth_surf->GetDesc(&depth_desc);
			if (FAILED(hres)) goto _exit;
			dfmt = depth_desc.Format;
		}
		hres = pDev->CreateTexture(w, h, 1, D3DUSAGE_DEPTHSTENCIL, dfmt, D3DPOOL_DEFAULT, (IDirect3DTexture9**)&rt.hDepth, NULL);
	}
	if (rt.hTgt) {
		hres = ((IDirect3DTexture9*)rt.hTgt)->GetSurfaceLevel(0, (IDirect3DSurface9**)&rt.hTgt_surf);
	}
	if (rt.hDepth) {
		hres = ((IDirect3DTexture9*)rt.hDepth)->GetSurfaceLevel(0, (IDirect3DSurface9**)&rt.hDepth_surf);
	}
	rt.w = (sys_ui16)w;
	rt.h = (sys_ui16)h;
	pRT = pRdr->mRsrc.mTgt.Add(rt);

_exit:
	if (pBack_surf) pBack_surf->Release();
	if (pDepth_surf) pDepth_surf->Release();
	return pRT;
}

static D3DFORMAT Find_fp_fmt() {
	HRESULT hres;
	IDirect3D9* pD3D;
	D3DFORMAT fmt = D3DFMT_UNKNOWN;
	pD3D = s_rdr.mpD3D;
	if (pD3D) {
		D3DFORMAT adapter_fmt = s_rdr.mAdapter_fmt;
		fmt = D3DFMT_A32B32G32R32F;
		hres = pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapter_fmt, D3DUSAGE_QUERY_FILTER | D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt);
		if (FAILED(hres)) {
			fmt = D3DFMT_A16B16G16R16F;
			hres = pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapter_fmt, D3DUSAGE_QUERY_FILTER | D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt);
			if (FAILED(hres)) {
				fmt = D3DFMT_A32B32G32R32F;
				hres = pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapter_fmt, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt);
				if (FAILED(hres)) {
					fmt = D3DFMT_A16B16G16R16F;
					hres = pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapter_fmt, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt);
					if (FAILED(hres)) {
						fmt = D3DFMT_UNKNOWN;
					}
				}
			}
		}
	}
	return fmt;
}

static D3DFORMAT Find_fp_fmt_mono() {
	HRESULT hres;
	IDirect3D9* pD3D;
	D3DFORMAT fmt = D3DFMT_UNKNOWN;
	pD3D = s_rdr.mpD3D;
	if (pD3D) {
		D3DFORMAT adapter_fmt = s_rdr.mAdapter_fmt;
		fmt = D3DFMT_R32F;
		hres = pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapter_fmt, D3DUSAGE_QUERY_FILTER | D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt);
		if (FAILED(hres)) {
			fmt = D3DFMT_G32R32F;
			hres = pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapter_fmt, D3DUSAGE_QUERY_FILTER | D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt);
			if (FAILED(hres)) {
				fmt = D3DFMT_G16R16F;
				hres = pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapter_fmt, D3DUSAGE_QUERY_FILTER | D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt);
				if (FAILED(hres)) {
					fmt = D3DFMT_G32R32F;
					hres = pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapter_fmt, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt);
					if (FAILED(hres)) {
						fmt = D3DFMT_G16R16F;
						hres = pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapter_fmt, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt);
						if (FAILED(hres)) {
							fmt = D3DFMT_UNKNOWN;
						}
					}
				}
			}
		}
	}
	return fmt;
}

RDR_TARGET* Rt_create_fp(int w, int h, bool mono_flg, D3DFORMAT fmt, D3DFORMAT dfmt) {
	HRESULT hres;
	RDR_TARGET rt;
	RDR_TARGET* pRT;
	IDirect3DSurface9* pBack_surf;
	IDirect3DSurface9* pDepth_surf;
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	if (!pDev) return NULL;
	pRT = NULL;
	memset(&rt, 0, sizeof(RDR_TARGET));
	pBack_surf = NULL;
	pDepth_surf = NULL;

	if (fmt == D3DFMT_UNKNOWN) {
		if (mono_flg) {
			fmt = Find_fp_fmt_mono();
		} else {
			fmt = Find_fp_fmt();
		}
	}
	if (fmt == D3DFMT_UNKNOWN) goto _exit;
	hres = pDev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, (IDirect3DTexture9**)&rt.hTgt, NULL);
	if (FAILED(hres)) goto _exit;
	if (dfmt != D3DFMT_UNKNOWN) {
		hres = pDev->CreateTexture(w, h, 1, D3DUSAGE_DEPTHSTENCIL, dfmt, D3DPOOL_DEFAULT, (IDirect3DTexture9**)&rt.hDepth, NULL);
		if (FAILED(hres)) goto _exit;
	}
	if (rt.hTgt) {
		hres = ((IDirect3DTexture9*)rt.hTgt)->GetSurfaceLevel(0, (IDirect3DSurface9**)&rt.hTgt_surf);
	}
	if (rt.hDepth) {
		hres = ((IDirect3DTexture9*)rt.hDepth)->GetSurfaceLevel(0, (IDirect3DSurface9**)&rt.hDepth_surf);
	}
	rt.w = (sys_ui16)w;
	rt.h = (sys_ui16)h;
	pRT = pRdr->mRsrc.mTgt.Add(rt);

_exit:
	if (pBack_surf) pBack_surf->Release();
	if (pDepth_surf) pDepth_surf->Release();
	return pRT;
}

static D3DBLEND Xlat_blend_mode(E_RDR_BLENDMODE blend) {
	D3DBLEND d3d_blend = (D3DBLEND)-1;
	static D3DBLEND tbl[] = {
	/* ZERO */     D3DBLEND_ZERO,
	/* ONE */      D3DBLEND_ONE,
	/* SRC */      D3DBLEND_SRCCOLOR,
	/* INVSRC */   D3DBLEND_INVSRCCOLOR,
	/* DST */      D3DBLEND_DESTCOLOR,
	/* INVDST */   D3DBLEND_INVDESTCOLOR,
	/* SRCA */     D3DBLEND_SRCALPHA,
	/* INVSRCA */  D3DBLEND_INVSRCALPHA,
	/* SRCASAT */  D3DBLEND_SRCALPHASAT,
	/* SRCA2 */    D3DBLEND_BOTHSRCALPHA,
	/* INVSRCA2 */ D3DBLEND_BOTHINVSRCALPHA,
	/* DSTA */     D3DBLEND_DESTALPHA,
	/* INVDSTA */  D3DBLEND_INVDESTALPHA,
	/* CONST */    D3DBLEND_BLENDFACTOR,
	/* INVCONST */ D3DBLEND_INVBLENDFACTOR,
	               D3DBLEND_ZERO,
	};

	d3d_blend = tbl[(sys_int)blend & E_RDR_BLENDMODE_MASK];
	return d3d_blend;
}

static D3DBLENDOP Xlat_blend_op(E_RDR_BLENDOP op) {
	D3DBLENDOP d3d_op = (D3DBLENDOP)-1;
	static D3DBLENDOP tbl[] = {
		/* ADD  */ D3DBLENDOP_ADD,
		/* SUB  */ D3DBLENDOP_SUBTRACT,
		/* MIN  */ D3DBLENDOP_MIN,
		/* MAX  */ D3DBLENDOP_MAX,
		/* RSUB */ D3DBLENDOP_REVSUBTRACT,
		D3DBLENDOP_ADD,
		D3DBLENDOP_ADD,
		D3DBLENDOP_ADD
	};
	d3d_op = tbl[(sys_int)op & E_RDR_BLENDOP_MASK];
	return d3d_op;
}

static D3DCMPFUNC Xlat_cmp_func(E_RDR_CMPFUNC func) {
	D3DCMPFUNC d3d_func = (D3DCMPFUNC)-1;
	switch (func) {
		case E_RDR_CMPFUNC_NEVER:
			d3d_func = D3DCMP_NEVER;
			break;
		case E_RDR_CMPFUNC_GREATEREQUAL:
			d3d_func = D3DCMP_GREATEREQUAL;
			break;
		case E_RDR_CMPFUNC_EQUAL:
			d3d_func = D3DCMP_EQUAL;
			break;
		case E_RDR_CMPFUNC_GREATER:
			d3d_func = D3DCMP_GREATER;
			break;
		case E_RDR_CMPFUNC_LESSEQUAL:
			d3d_func = D3DCMP_LESSEQUAL;
			break;
		case E_RDR_CMPFUNC_NOTEQUAL:
			d3d_func = D3DCMP_NOTEQUAL;
			break;
		case E_RDR_CMPFUNC_LESS:
			d3d_func = D3DCMP_LESS;
			break;
		case E_RDR_CMPFUNC_ALWAYS:
			d3d_func = D3DCMP_ALWAYS;
			break;
		default:
			break;
	}
	return d3d_func;
}

static void Apply_blend_state(RDR_BLEND_STATE bs) {
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	if (bs.on) {
		pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		pDev->SetRenderState(D3DRS_BLENDOP, Xlat_blend_op((E_RDR_BLENDOP)bs.op));
		pDev->SetRenderState(D3DRS_SRCBLEND, Xlat_blend_mode((E_RDR_BLENDMODE)bs.src));
		pDev->SetRenderState(D3DRS_DESTBLEND, Xlat_blend_mode((E_RDR_BLENDMODE)bs.dst));
		pDev->SetRenderState(D3DRS_BLENDOPALPHA, Xlat_blend_op((E_RDR_BLENDOP)bs.op_a));
		pDev->SetRenderState(D3DRS_SRCBLENDALPHA, Xlat_blend_mode((E_RDR_BLENDMODE)bs.src_a));
		pDev->SetRenderState(D3DRS_DESTBLENDALPHA, Xlat_blend_mode((E_RDR_BLENDMODE)bs.dst_a));
	} else {
		pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	}
}

static void Apply_draw_state(RDR_DRAW_STATE ds) {
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	pDev->SetRenderState(D3DRS_COLORWRITEENABLE, ds.color_mask);

	D3DCULL cull = D3DCULL_NONE;
	switch (ds.cull) {
		case E_RDR_CULL_CCW:
			cull = D3DCULL_CCW;
			break;
		case E_RDR_CULL_CW:
			cull = D3DCULL_CW;
			break;
	}
	pDev->SetRenderState(D3DRS_CULLMODE, cull);

	pDev->SetRenderState(D3DRS_ZWRITEENABLE, !!ds.zwrite);
	pDev->SetRenderState(D3DRS_ZENABLE, !!ds.ztest);
	if (ds.ztest) {
		pDev->SetRenderState(D3DRS_ZFUNC, Xlat_cmp_func((E_RDR_CMPFUNC)ds.zfunc));
	}
	pDev->SetRenderState(D3DRS_ALPHATESTENABLE, !!ds.atest);
	if (ds.atest) {
		pDev->SetRenderState(D3DRS_ALPHAFUNC, Xlat_cmp_func((E_RDR_CMPFUNC)ds.afunc));
	}
	pDev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, !!ds.msaa);
}

void Apply_param(RDR_BATCH_PARAM* pParam) {
	int i, n;
	UVEC* pSrc_v;
	UVEC* pDst_v;
	float* pSrc_f;
	float* pDst_f;
	sys_i32* pSrc_i;
	sys_i32* pDst_i;
	RDR_SAMPLER* pSrc_s;
	RDR_SAMPLER* pDst_s;
	sys_byte* pSrc_b;
	sys_byte* pDst_b;
	RDR_GPARAM* pGP = &g_rdr_param;

	n = pParam->count;
	switch (pParam->id.type) {
		case E_RDR_PARAMTYPE_FVEC:
			pSrc_v = pParam->pVec;
			pDst_v = (UVEC*)D_INCR_PTR(pGP, D_RDR_GPTOP_FVEC) + pParam->id.offs;
			for (i = 0; i < n; ++i) {
				pDst_v->qv = pSrc_v->qv;
				++pSrc_v;
				++pDst_v;
			}
			break;
		case E_RDR_PARAMTYPE_IVEC:
			pSrc_v = pParam->pVec;
			pDst_v = (UVEC*)D_INCR_PTR(pGP, D_RDR_GPTOP_IVEC) + pParam->id.offs;
			for (i = 0; i < n; ++i) {
				pDst_v->iv = pSrc_v->iv;
				++pSrc_v;
				++pDst_v;
			}
			break;
		case E_RDR_PARAMTYPE_FLOAT:
			pSrc_f = pParam->pFloat;
			pDst_f = (float*)D_INCR_PTR(pGP, D_RDR_GPTOP_FLOAT) + pParam->id.offs;
			for (i = 0; i < n; ++i) {
				*pDst_f++ = *pSrc_f++;
			}
			break;
		case E_RDR_PARAMTYPE_INT:
			pSrc_i = pParam->pInt;
			pDst_i = (sys_i32*)D_INCR_PTR(pGP, D_RDR_GPTOP_INT) + pParam->id.offs;
			for (i = 0; i < n; ++i) {
				*pDst_i++ = *pSrc_i++;
			}
			break;
		case E_RDR_PARAMTYPE_SMP:
			pSrc_s = pParam->pSmp;
			pDst_s = (RDR_SAMPLER*)D_INCR_PTR(pGP, D_RDR_GPTOP_SMP) + pParam->id.offs;
			for (i = 0; i < n; ++i) {
				memcpy(pDst_s, pSrc_s, sizeof(RDR_SAMPLER));
				++pSrc_s;
				++pDst_s;
			}
			break;
		case E_RDR_PARAMTYPE_BOOL:
			pSrc_b = pParam->pBool;
			pDst_b = (sys_byte*)D_INCR_PTR(pGP, D_RDR_GPTOP_BOOL) + pParam->id.offs;
			for (i = 0; i < n; ++i) {
				*pDst_b++ = *pSrc_b++;
			}
			break;
		default:
			break;
	}
}

void Batch_exec(RDR_BATCH* pBatch) {
	int i, n;
	RDR_BATCH_PARAM* pParam;
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	D3DPRIMITIVETYPE prim_type;

	n = pBatch->nb_param;
	pParam = pBatch->pParam;
	for (i = 0; i < n; ++i) {
		Apply_param(pParam);
		++pParam;
	}
	Apply_blend_state(pBatch->blend_state);
	Apply_draw_state(pBatch->draw_state);
	switch (pBatch->type) {
		default:
		case E_RDR_PRIMTYPE_TRILIST:
			prim_type = D3DPT_TRIANGLELIST;
			break;
		case E_RDR_PRIMTYPE_TRISTRIP:
			prim_type = D3DPT_TRIANGLESTRIP;
			break;
		case E_RDR_PRIMTYPE_LINELIST:
			prim_type = D3DPT_LINELIST;
			break;
		case E_RDR_PRIMTYPE_LINESTRIP:
			prim_type = D3DPT_LINESTRIP;
			break;
	}
	pRdr->Set_vb(pBatch->pVtx);
	pRdr->mGpu_code.Set_pipeline(pBatch->vtx_prog, pBatch->pix_prog);
	if (pBatch->pIdx) {
		pRdr->Set_ib(pBatch->pIdx);
		pDev->DrawIndexedPrimitive(prim_type, pBatch->base, 0, pBatch->pVtx->nb_vtx, pBatch->start, pBatch->count);
	} else {
		pDev->DrawPrimitive(prim_type, pBatch->start, pBatch->count);
	}
}

static void Dev_init() {
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	pDev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	pDev->SetVertexShader(NULL);
	pDev->SetPixelShader(NULL);
}

void RDR_init(void* hWnd, int width, int height, int fullscreen) {
	HRESULT hres;
	UINT adapter;
	D3DCAPS9 dev_caps;
	D3DADAPTER_IDENTIFIER9 adapter_data;
	DWORD dev_flags;
	D3DPRESENT_PARAMETERS present_params;
	RDR_WORK* pRdr = &s_rdr;

	CoInitialize(NULL);
	memset(pRdr, 0, sizeof(RDR_WORK));
	memset(&g_rdr_param, 0, sizeof(RDR_GPARAM));

	pRdr->mpD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!pRdr->mpD3D) {
		CoUninitialize();
		SYS_log("Can't initialize D3D\n");
		return;
	}
	adapter = D3DADAPTER_DEFAULT;
	hres = pRdr->mpD3D->GetDeviceCaps(adapter, D3DDEVTYPE_HAL, &dev_caps);
	if (FAILED(hres)) {
		pRdr->mpD3D->Release();
		pRdr->mpD3D = NULL;
		CoUninitialize();
		return;
	}
	hres = pRdr->mpD3D->GetAdapterIdentifier(adapter, 0, &adapter_data);
	if (FAILED(hres)) {
		pRdr->mpD3D->Release();
		pRdr->mpD3D = NULL;
		CoUninitialize();
		return;
	}
	dev_flags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
	if (!(dev_caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)) {
		dev_flags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}
	if (dev_flags == D3DCREATE_HARDWARE_VERTEXPROCESSING) {
		if (dev_caps.DevCaps & D3DDEVCAPS_PUREDEVICE) {
			dev_flags |= D3DCREATE_PUREDEVICE;
		}
	}
	pRdr->max_tex_stg = dev_caps.MaxSimultaneousTextures;
	memset(&present_params, 0, sizeof(D3DPRESENT_PARAMETERS));
	present_params.hDeviceWindow = (HWND)hWnd;
	present_params.Windowed = fullscreen ? FALSE : TRUE;
	present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	if (present_params.Windowed) {
		present_params.BackBufferFormat = D3DFMT_UNKNOWN;
		present_params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	} else {
		present_params.BackBufferFormat = D3DFMT_A8R8G8B8;
		present_params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		present_params.FullScreen_RefreshRateInHz = 60;
	}
	present_params.BackBufferWidth = width;
	present_params.BackBufferHeight = height;
	present_params.EnableAutoDepthStencil = TRUE;
	present_params.AutoDepthStencilFormat = D3DFMT_D16;
	pRdr->mAdapter_fmt = present_params.BackBufferFormat;
	hres = pRdr->mpD3D->CreateDevice(adapter, D3DDEVTYPE_HAL, (HWND)hWnd, dev_flags, &present_params, &pRdr->mpDev);
	if (FAILED(hres)) {
		pRdr->mpD3D->Release();
		pRdr->mpD3D = NULL;
		CoUninitialize();
		return;
	}

	memset(&pRdr->mMain_rt, 0, sizeof(RDR_TARGET));
	pRdr->mMain_rt.w = width;
	pRdr->mMain_rt.h = height;
	pRdr->mpDev->GetRenderTarget(0, (IDirect3DSurface9**)&pRdr->mMain_rt.hTgt_surf);
	pRdr->mpDev->GetDepthStencilSurface((IDirect3DSurface9**)&pRdr->mMain_rt.hDepth_surf);

	static D3DVERTEXELEMENT9 elem_gen[] = {
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, pos), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, nrm), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, tng), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, clr), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, uv0), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, uv1), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, uv2), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, uv3), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, idx), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, wgt), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0},
		{0, D_FIELD_OFFS(RDR_VTX_GENERAL, bnm), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0},
		D3DDECL_END()
	};
	hres = pRdr->mpDev->CreateVertexDeclaration(elem_gen, &pRdr->mDecl.pGen);
	if (FAILED(hres)) {
		SYS_log("Can't create vertex declaration (GENERAL)\n");
	}

	static D3DVERTEXELEMENT9 elem_solid[] = {
		{0, D_FIELD_OFFS(RDR_VTX_SOLID, pos), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SOLID, nrm), D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SOLID, tng), D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SOLID, clr), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SOLID, tex), D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()
	};
	hres = pRdr->mpDev->CreateVertexDeclaration(elem_solid, &pRdr->mDecl.pSolid);
	if (FAILED(hres)) {
		SYS_log("Can't create vertex declaration (SOLID)\n");
	}

	static D3DVERTEXELEMENT9 elem_skin[] = {
		{0, D_FIELD_OFFS(RDR_VTX_SKIN, pos), D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SKIN, nrm), D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SKIN, tng), D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SKIN, tex), D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SKIN, idx), D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SKIN, wgt), D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0},
		D3DDECL_END()
	};
	hres = pRdr->mpDev->CreateVertexDeclaration(elem_skin, &pRdr->mDecl.pSkin);
	if (FAILED(hres)) {
		SYS_log("Can't create vertex declaration (SKIN)\n");
	}

	static D3DVERTEXELEMENT9 elem_screen[] = {
		{0, D_FIELD_OFFS(RDR_VTX_SCREEN, pos), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SCREEN, clr), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		{0, D_FIELD_OFFS(RDR_VTX_SCREEN, tex), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()
	};
	hres = pRdr->mpDev->CreateVertexDeclaration(elem_screen, &pRdr->mDecl.pScreen);
	if (FAILED(hres)) {
		SYS_log("Can't create vertex declaration (SCREEN)\n");
	}

	pRdr->Init();
	Dev_init();
}


#define D_RELEASE_DECL(_fmt) if (s_rdr.mDecl.p##_fmt) { \
		Safe_release(s_rdr.mDecl.p##_fmt); \
		s_rdr.mDecl.p##_fmt = NULL; \
	} \

void RDR_reset() {
	int i;
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	if (pDev) {
		for (i = 0; i < pRdr->max_tex_stg; ++i) {
			pDev->SetTexture(i, NULL);
		}
		pDev->SetVertexShader(NULL);
		pDev->SetPixelShader(NULL);
	}

	pRdr->mGpu_code.Reset();
	pRdr->mRsrc.Release();

	D_RELEASE_DECL(Gen);
	D_RELEASE_DECL(Solid);
	D_RELEASE_DECL(Skin);
	D_RELEASE_DECL(Screen);

	if (pRdr->mMain_rt.hTgt_surf) {
		((IDirect3DSurface9*)pRdr->mMain_rt.hTgt_surf)->Release();
		pRdr->mMain_rt.hTgt_surf = NULL;
	}
	if (pRdr->mMain_rt.hDepth_surf) {
		((IDirect3DSurface9*)pRdr->mMain_rt.hDepth_surf)->Release();
		pRdr->mMain_rt.hDepth_surf = NULL;
	}

	if (pRdr->mpDev) {
		pRdr->mpDev->Release();
		pRdr->mpDev = NULL;
	}
	if (pRdr->mpD3D) {
		pRdr->mpD3D->Release();
		pRdr->mpD3D = NULL;
	}
	CoUninitialize();
}

void RDR_init_thread_FPU() {
#if !defined(_WIN64)
#	if defined(_MSC_VER) || defined(__INTEL_COMPILER)
	__asm {
		push eax
		fnstcw word ptr [esp]
		pop eax
		and ah, NOT 3
		push eax
		fldcw word ptr [esp]
		pop eax
	}
#	elif defined (__GNUC__)
#	endif
#endif
}

void RDR_begin() {
	s_rdr.mDb_wk.Begin();
}

void Set_vb(RDR_VTX_BUFFER* pVB) {
	RDR_WORK* pRdr = &s_rdr;
	pRdr->Set_vb(pVB);
}

void Set_rt(RDR_TARGET* pRT) {
	HRESULT hres;
	RDR_WORK* pRdr = &s_rdr;
	if (!pRT) {
		pRT = &pRdr->mMain_rt;
	}
	if (pRT->hTgt_surf) {
		hres = pRdr->mpDev->SetRenderTarget(0, (IDirect3DSurface9*)pRT->hTgt_surf);
	}
	if (pRT->hDepth_surf) {
		hres = pRdr->mpDev->SetDepthStencilSurface((IDirect3DSurface9*)pRT->hDepth_surf);
	} else {
		hres = pRdr->mpDev->SetDepthStencilSurface(NULL);
	}
}

static void Cpy_back(RDR_TARGET* pRT, D3DTEXTUREFILTERTYPE filt) {
	HRESULT hres;
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	IDirect3DSurface9* pBack_surf = (IDirect3DSurface9*)pRdr->mMain_rt.hTgt_surf;
	if (pBack_surf) {
		hres = pDev->StretchRect(pBack_surf, NULL, (IDirect3DSurface9*)pRT->hTgt_surf, NULL, filt);
	}
}

static void Rdr_shadow_calc() {
	QMTX vm;
	QMTX cm;
	QMTX tm;
	QMTX fm;
	QMTX sm;
	QMTX inv_vp;
	UVEC sdir;
	UVEC vdir;
	UVEC iprj;
	UVEC up;
	UVEC tgt;
	UVEC pos;
	UVEC tv;
	UVEC offs;
	UVEC vmin;
	UVEC vmax;
	UVEC box[8];
	RDR_WORK* pRdr = &s_rdr;
	RDR_VIEW* pView = pRdr->mDb_wk.Get_exec_view();
	int i;
	float vdist, snear, sfar, zmin, zmax, ymin, ymax, y, sy, t;

	static QMTX bias = {
		{0.5f, 0.0f, 0.0f, 0.0f},
		{0.0f, -0.5f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.5f, 0.5f, 0.0f, 1.0f}
	};


	sdir.qv = V4_normalize(pRdr->mDb_wk.Get_exec_shadow_dir());
	pRdr->mShadow_dir = sdir.qv;
	vdir.qv = V4_scale(V4_load(pView->iview[2]), -1.0f);
	vdist = 25.0f * pView->proj[1][1];
	iprj.qv = MTX_calc_qpnt(pView->iproj, V4_zero());
	snear = -iprj.z/iprj.w;
	sfar = snear + (vdist - snear) * (1.0f - fabsf(V4_dot(sdir.qv, V4_load(pView->iview[2]))) * 0.8f);
	pos.qv = MTX_calc_qpnt(pView->view_proj, V4_add(V4_scale(vdir.qv, snear), V4_load(pView->iview[3])));
	zmin = pos.z / pos.w;
	pos.qv = MTX_calc_qpnt(pView->view_proj, V4_add(V4_scale(vdir.qv, sfar), V4_load(pView->iview[3])));
	zmax = pos.z / pos.w;
	if (zmin < 0.0f) zmin = 0.0f;
	if (zmax > 1.0f) zmax = 1.0f;
	box[0].qv = V4_set_pnt(-1.0f, -1.0f, zmin);
	box[1].qv = V4_set_pnt(1.0f, -1.0f, zmin);
	box[2].qv = V4_set_pnt(-1.0f, 1.0f, zmin);
	box[3].qv = V4_set_pnt(1.0f, 1.0f, zmin);
	box[4].qv = V4_set_pnt(-1.0f, -1.0f, zmax);
	box[5].qv = V4_set_pnt(1.0f, -1.0f, zmax);
	box[6].qv = V4_set_pnt(-1.0f, 1.0f, zmax);
	box[7].qv = V4_set_pnt(1.0f, 1.0f, zmax);
	MTX_invert(inv_vp, pView->view_proj);
	for (i = 0; i < 8; ++i) {
		box[i].qv = MTX_calc_qpnt(inv_vp, box[i].qv);
		box[i].qv = V4_set_w1(V4_scale(box[i].qv, 1.0f/box[i].w));
	}
	up.qv = V4_normalize(V4_cross(sdir.qv, V4_cross(vdir.qv, sdir.qv)));
	pos.qv = V4_load(pView->iview[3]);
	tgt.qv = V4_sub(pos.qv, sdir.qv);
	MTX_make_view(vm, pos.qv, tgt.qv, up.qv);
	ymin = D_MAX_FLOAT;
	ymax = -D_MAX_FLOAT;
	for (i = 0; i < 8; ++i) {
		tv.qv = MTX_calc_qpnt(vm, box[i].qv);
		ymin = F_min(ymin, tv.y);
		ymax = F_max(ymax, tv.y);
	}
	sy = sqrtf(1.0f - D_SQ(V4_dot(vdir.qv, sdir.qv)));
	t = (snear + sqrtf(snear*sfar)) / sy;
	y = (ymax - ymin) / t;
	MTX_unit(cm);
	cm[0][0] = -1.0f;
	cm[1][1] = (y + t) / (y - t);
	cm[1][3] = 1.0f;
	cm[3][1] = (-2.0f*y*t) / (y - t);
	cm[3][3] = 0.0f;

	pos.qv = V4_set_w1(V4_add(V4_load(pView->iview[3]), V4_scale(up.qv, ymin - t)));
	tgt.qv = V4_sub(pos.qv, sdir.qv);
	MTX_make_view(vm, pos.qv, tgt.qv, up.qv);
	MTX_mul(tm, vm, cm);
	vmin.qv = V4_fill(D_MAX_FLOAT);
	vmax.qv = V4_fill(-D_MAX_FLOAT);
	for (i = 0; i < 8; ++i) {
		tv.qv = MTX_calc_qpnt(tm, box[i].qv);
		tv.qv = V4_set_w1(V4_scale(tv.qv, 1.0f/tv.w));
		vmin.qv = V4_min(vmin.qv, tv.qv);
		vmax.qv = V4_max(vmax.qv, tv.qv);
	}
	offs.qv = V4_set_w0(V4_scale(sdir.qv, 40.0f));
	zmin = vmin.z;
	for (i = 0; i < 8; ++i) {
		tv.qv = V4_sub(box[i].qv, offs.qv);
		tv.qv = MTX_calc_qpnt(tm, tv.qv);
		tv.z /= tv.w;
		zmin = F_min(zmin, tv.z);
	}
	vmin.z = zmin;
	MTX_unit(fm);
	tv.qv = V4_div(V4_fill(1.0f), V4_set_w1(V4_sub(vmax.qv, vmin.qv)));
	fm[0][0] = 2.0f * tv.x;
	fm[1][1] = 2.0f * tv.y;
	fm[2][2] = tv.z;
	fm[3][0] = -(vmin.x + vmax.x) * tv.x;
	fm[3][1] = -(vmin.y + vmax.y) * tv.y;
	fm[3][2] = -vmin.z * tv.z;
	MTX_mul(fm, cm, fm);
	MTX_mul(sm, vm, fm);
	MTX_cpy(pRdr->mShadow_view_proj, sm);
	MTX_mul(sm, sm, bias);
	MTX_cpy(pRdr->mShadow_mtx, sm);
}

static void Rdr_zbuf_prologue(RDR_LAYER* pLyr) {
	RDR_WORK* pRdr = &s_rdr;
	RDR_GPARAM* pGP = &g_rdr_param;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	Set_rt(NULL);
	pDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFFFFFFF, 1.0f, 0);
	pRdr->mDb_wk.Apply_exec_view();
}

static void Rdr_zbuf_epilogue(RDR_LAYER* pLyr) {
	RDR_WORK* pRdr = &s_rdr;
	Cpy_back(pRdr->mRT.mpDepth, D3DTEXF_POINT);
}

static void Rdr_cast_prologue(RDR_LAYER* pLyr) {
	QMTX tm;
	RDR_WORK* pRdr = &s_rdr;
	RDR_GPARAM* pGP = &g_rdr_param;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	Set_rt(pRdr->mRT.mpShadow);
	pDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFFFFFFF, 1.0f, 0);

	MTX_transpose(tm, pRdr->mShadow_view_proj);
	pGP->view_proj[0].qv = V4_load(tm[0]);
	pGP->view_proj[1].qv = V4_load(tm[1]);
	pGP->view_proj[2].qv = V4_load(tm[2]);
	pGP->view_proj[3].qv = V4_load(tm[3]);

	pGP->shadow_offs.qv = V4_scale(pRdr->mShadow_dir, 0.0f);
}

static void Rdr_mtl_prologue(RDR_LAYER* pLyr) {
	SH_PARAM sh_param;
	SH_COEF sh_dir;
	SH_COEF sh_coef;
	UVEC dir_vec;
	UVEC dir_color;
	RDR_WORK* pRdr = &s_rdr;
	RDR_GPARAM* pGP = &g_rdr_param;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	static SH_COEF sh_env = {
		{(float)3.783264, (float)-2.887813, (float)0.3790306, (float)-1.033029, (float)0.623285, (float)-0.07800784, (float)-0.9355605, (float)-0.5741177, (float)0.2033477},
		{(float)4.260424, (float)-3.586802, (float)0.2952161, (float)-1.031689, (float)0.5558126, (float)0.1486536, (float)-1.25426, (float)-0.5034486, (float)-0.0442012},
		{(float)4.504587, (float)-4.147052, (float)0.09856746, (float)-0.8849242, (float)0.3977577, (float)0.47249, (float)-1.52563, (float)-0.3643064, (float)-0.4521801}
	};

	Set_rt(NULL);
	pDev->Clear(0, NULL, D3DCLEAR_TARGET, pRdr->mClear_color, 0.0f, 0);

	pRdr->mDb_wk.Apply_exec_view();
	pGP->vtx_param.qv = V4_set(/*dbias*/0.0f, 0.0f, 0.0f, 0.0f);
	pGP->base_color.qv = V4_fill(1.0f);
	pGP->world[0].qv = V4_load(g_identity[0]);
	pGP->world[1].qv = V4_load(g_identity[1]);
	pGP->world[2].qv = V4_load(g_identity[2]);

	// default headlight
	dir_vec.qv = V4_scale(V4_load(pRdr->mDb_wk.Get_exec_view()->iview[2]), -1.0f);
	dir_color.r = 0.7f;
	dir_color.g = 0.7f;
	dir_color.b = 0.7f;
	SH_calc_dir(&sh_dir, &dir_color, &dir_vec);
	SH_scale(&sh_dir, &sh_dir, 1.0f);

	SH_scale(&sh_coef, &sh_env, 0.1f);
	SH_add(&sh_coef, &sh_coef, &sh_dir);
	SH_calc_param(&sh_param, &sh_coef);
	memcpy(&pGP->SH, &sh_param, sizeof(SH_PARAM));

#ifdef D_RDRPROG_pix_toon
	pGP->toon_sun_color.qv = V4_set(0.82f, 0.541106f, 0.4264f, 1.0f);
	pGP->toon_sun_param.qv = V4_set(0.75f, 0.5f, 1.0f, 0.8f);
	pGP->toon_sun_dir.qv = dir_vec.qv;
	pGP->toon_rim_color.qv = V4_set(0.0f, 0.0f, 0.0f, 1.0f);
	pGP->toon_rim_param.qv = V4_set(0.2f, -0.15f, 5.0f, 1.0f);

	pGP->toon_ctrl.qv = V4_set(0.1f, 0.0f, 0.0f, 0.0f);
#endif
}

static void Rdr_recv_prologue(RDR_LAYER* pLyr) {
	QMTX tm;
	RDR_WORK* pRdr = &s_rdr;
	RDR_GPARAM* pGP = &g_rdr_param;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	float smap_size = pRdr->mRT.mpShadow->w;

	Set_rt(NULL);
	pRdr->mDb_wk.Apply_exec_view();
	pGP->smp_shadow.addr_u = E_RDR_TEXADDR_WRAP;
	pGP->smp_shadow.addr_v = E_RDR_TEXADDR_WRAP;
	pGP->smp_shadow.addr_w = E_RDR_TEXADDR_WRAP;
	pGP->smp_shadow.min = E_RDR_TEXFILTER_POINT;
	pGP->smp_shadow.mag = E_RDR_TEXFILTER_POINT;
	pGP->smp_shadow.mip = E_RDR_TEXFILTER_POINT;
	pGP->smp_shadow.hTex = pRdr->mRT.mpShadow->hTgt;
	MTX_transpose(tm, pRdr->mShadow_mtx);
	pGP->shadow_proj[0].qv = V4_load(tm[0]);
	pGP->shadow_proj[1].qv = V4_load(tm[1]);
	pGP->shadow_proj[2].qv = V4_load(tm[2]);
	pGP->shadow_proj[3].qv = V4_load(tm[3]);
	pGP->shadow_color.qv = V4_set(0.1f, 0.1f, 0.0f, 0.8f);
	pGP->shadow_param.qv = V4_set(1.0f/smap_size, smap_size, 1000.0f, 0.0f);
}

static void Rdr_exec_cc() {
	RDR_WORK* pRdr = &s_rdr;
	RDR_GPARAM* pGP = &g_rdr_param;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	bool flg;
	int pix_id;

	Set_rt(NULL);
	flg = true;
	switch (pRdr->mImg.mCC.mode) {
		case E_RDR_CCMODE_NONE:
		default:
			flg = false;
			break;
		case E_RDR_CCMODE_LEVELS:
			pRdr->mImg.Apply_levels();
			pix_id = D_RDRPROG_pix_cc_levels;
			break;
		case E_RDR_CCMODE_LINEAR:
			pRdr->mImg.Apply_linear();
			pix_id = D_RDRPROG_pix_cc_linear;
			break;
		case E_RDR_CCMODE_LUM:
			pRdr->mImg.Apply_lum();
			pix_id = D_RDRPROG_pix_cc_lum;
			break;
	}

	if (flg) {
		pGP->smp_img0.addr_u = E_RDR_TEXADDR_WRAP;
		pGP->smp_img0.addr_v = E_RDR_TEXADDR_WRAP;
		pGP->smp_img0.addr_w = E_RDR_TEXADDR_WRAP;
		pGP->smp_img0.min = E_RDR_TEXFILTER_POINT;
		pGP->smp_img0.mag = E_RDR_TEXFILTER_POINT;
		pGP->smp_img0.mip = E_RDR_TEXFILTER_POINT;
		pGP->smp_img0.hTex = pRdr->mRT.mpScene->hTgt;
		pRdr->mImg.Draw_quad(D_RDRPROG_vtx_img, pix_id, 0);
	}
}

void RDR_exec() {
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;

	pRdr->mDb_wk.Get_exec_lyr(E_RDR_LAYER_ZBUF)->mpPrologue = Rdr_zbuf_prologue;
	pRdr->mDb_wk.Get_exec_lyr(E_RDR_LAYER_ZBUF)->mpEpilogue = Rdr_zbuf_epilogue;
	pRdr->mDb_wk.Get_exec_lyr(E_RDR_LAYER_CAST)->mpPrologue = Rdr_cast_prologue;
	pRdr->mDb_wk.Get_exec_lyr(E_RDR_LAYER_MTL0)->mpPrologue = Rdr_mtl_prologue;
	pRdr->mDb_wk.Get_exec_lyr(E_RDR_LAYER_RECV)->mpPrologue = Rdr_recv_prologue;

	pDev->BeginScene();
	Rdr_shadow_calc();
	pRdr->mDb_wk.Exec();
	Cpy_back(pRdr->mRT.mpScene, D3DTEXF_POINT);
	Rdr_exec_cc();
	pDev->EndScene();

	pDev->Present(NULL, NULL, NULL, NULL);
	pRdr->mDb_wk.Flip();/////////////////////////
}

RDR_VIEW* RDR_get_view() {
	return s_rdr.mDb_wk.Get_work_view();
}

UVEC* RDR_get_val_v(int n) {
	return (UVEC*)s_rdr.mDb_wk.Get_val(E_RDR_PARAMTYPE_FVEC, n);
}

float* RDR_get_val_f(int n) {
	return (float*)s_rdr.mDb_wk.Get_val(E_RDR_PARAMTYPE_FLOAT, n);
}

sys_i32* RDR_get_val_i(int n) {
	return (sys_i32*)s_rdr.mDb_wk.Get_val(E_RDR_PARAMTYPE_INT, n);
}

RDR_SAMPLER* RDR_get_val_s(int n) {
	return (RDR_SAMPLER*)s_rdr.mDb_wk.Get_val(E_RDR_PARAMTYPE_SMP, n);
}

sys_byte* RDR_get_val_b(int n) {
	return (sys_byte*)s_rdr.mDb_wk.Get_val(E_RDR_PARAMTYPE_BOOL, n);
}

RDR_BATCH_PARAM* RDR_get_param(int n) {
	RDR_BATCH_PARAM* pParam = s_rdr.mDb_wk.Get_param(n);
	return pParam;
}

RDR_BATCH* RDR_get_batch() {
	RDR_BATCH* pBatch = s_rdr.mDb_wk.Get_batch();
	if (pBatch) {
		pBatch->nb_param = 0;
		pBatch->vtx_prog = D_RDRPROG_vtx_default;
		pBatch->pix_prog = D_RDRPROG_pix_default;
		pBatch->blend_state.on = false;
		pBatch->blend_state.op = E_RDR_BLENDOP_ADD;
		pBatch->blend_state.src = E_RDR_BLENDMODE_SRCA;
		pBatch->blend_state.dst = E_RDR_BLENDMODE_INVSRCA;
		pBatch->blend_state.op_a = E_RDR_BLENDOP_MAX;
		pBatch->blend_state.src_a = E_RDR_BLENDMODE_ONE;
		pBatch->blend_state.dst_a = E_RDR_BLENDMODE_ONE;
		pBatch->draw_state.color_mask = 0xF;
		pBatch->draw_state.cull = E_RDR_CULL_CCW;
		pBatch->draw_state.ztest = true;
		pBatch->draw_state.zwrite = true;
		pBatch->draw_state.zfunc = E_RDR_CMPFUNC_LESSEQUAL;
		pBatch->draw_state.atest = false;
		pBatch->draw_state.afunc = E_RDR_CMPFUNC_GREATER;
		pBatch->draw_state.msaa = true;
		pBatch->base = 0;
	}
	return pBatch;
}

void RDR_put_batch(RDR_BATCH* pBatch, sys_ui32 key, sys_uint lyr_id) {
	if (lyr_id >= E_RDR_LAYER_MAX) return;
	s_rdr.mDb_wk.Put_batch(pBatch, key, lyr_id);
}

static RDR_VTX_BUFFER* VB_create(E_RDR_VTXTYPE type, int n, bool dyn_flg) {
	HRESULT hres;
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	RDR_VTX_BUFFER* pVB = NULL;
	RDR_VTX_BUFFER buff;
	int vsize = 0;

	memset(&buff, 0, sizeof(RDR_VTX_BUFFER));
	switch (type) {
		case E_RDR_VTXTYPE_GENERAL:
			vsize = sizeof(RDR_VTX_GENERAL);
			break;
		case E_RDR_VTXTYPE_SOLID:
			vsize = sizeof(RDR_VTX_SOLID);
			break;
		case E_RDR_VTXTYPE_SKIN:
			vsize = sizeof(RDR_VTX_SKIN);
			break;
		case E_RDR_VTXTYPE_SCREEN:
			vsize = sizeof(RDR_VTX_SCREEN);
			break;
		default:
			break;
	}
	if (vsize) {
		DWORD usg = D3DUSAGE_WRITEONLY;
		if (dyn_flg) {
			usg |= D3DUSAGE_DYNAMIC;
		}
		hres = pDev->CreateVertexBuffer(vsize*n, usg, 0, D3DPOOL_DEFAULT, (IDirect3DVertexBuffer9**)&buff.handle, NULL);
		if (FAILED(hres)) {
			SYS_log("Can't create vertex buffer.\n");
		} else {
			buff.type = type;
			buff.nb_vtx = n;
			buff.vtx_size = vsize;
			if (dyn_flg) {
				buff.attr |= E_RDR_RSRCATTR_DYNAMIC;
			}
			pVB = pRdr->mRsrc.mVtx.Add(buff);
		}
	}
	return pVB;
}

RDR_VTX_BUFFER* RDR_vtx_create(E_RDR_VTXTYPE type, int n) {
	return VB_create(type, n, false);
}

RDR_VTX_BUFFER* RDR_vtx_create_dyn(E_RDR_VTXTYPE type, int n) {
	return VB_create(type, n, true);
}

void RDR_vtx_release(RDR_VTX_BUFFER* pVB) {
	RDR_WORK* pRdr = &s_rdr;
	if (pVB && pVB->handle) {
		ULONG refs = ((IDirect3DVertexBuffer9*)pVB->handle)->Release();
		if (0 == refs) {
			pRdr->mRsrc.mVtx.Remove(pVB);
		}
	}
}

void RDR_vtx_lock(RDR_VTX_BUFFER* pVB) {
	if (pVB) {
		if (pVB->locked.pData) {
			SYS_log("RDR_vtx_lock: already locked\n");
		} else {
			((IDirect3DVertexBuffer9*)pVB->handle)->Lock(0, 0, &pVB->locked.pData, 0);
		}
	}
}

void RDR_vtx_unlock(RDR_VTX_BUFFER* pVB) {
	if (pVB) {
		if (pVB->locked.pData) {
			((IDirect3DVertexBuffer9*)pVB->handle)->Unlock();
			pVB->locked.pData = NULL;
		} else {
			SYS_log("RDR_vtx_unlock: not locked\n");
		}
	}
}

static RDR_IDX_BUFFER* IB_create(E_RDR_IDXTYPE type, int n, bool dyn_flg) {
	HRESULT hres;
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	RDR_IDX_BUFFER* pIB = NULL;
	RDR_IDX_BUFFER buff;
	int len = 0;
	D3DFORMAT d3d_fmt = D3DFMT_UNKNOWN;

	memset(&buff, 0, sizeof(RDR_IDX_BUFFER));
	switch (type) {
		case E_RDR_IDXTYPE_16BIT:
			d3d_fmt = D3DFMT_INDEX16;
			len = n*sizeof(sys_ui16);
			break;
		case E_RDR_IDXTYPE_32BIT:
			d3d_fmt = D3DFMT_INDEX32;
			len = n*sizeof(sys_ui32);
			break;
		default:
			break;
	}
	if (len) {
		DWORD usg = D3DUSAGE_WRITEONLY;
		if (dyn_flg) {
			usg |= D3DUSAGE_DYNAMIC;
		}
		hres = pDev->CreateIndexBuffer(len, usg, d3d_fmt, D3DPOOL_DEFAULT, (IDirect3DIndexBuffer9**)&buff.handle, NULL);
		if (FAILED(hres)) {
			SYS_log("Can't create index buffer.\n");
		} else {
			buff.nb_idx = n;
			buff.type = type;
			if (dyn_flg) {
				buff.attr |= E_RDR_RSRCATTR_DYNAMIC;
			}
			pIB = pRdr->mRsrc.mIdx.Add(buff);
		}
	}

	return pIB;
}

RDR_IDX_BUFFER* RDR_idx_create(E_RDR_IDXTYPE type, int n) {
	return IB_create(type, n, false);
}

RDR_IDX_BUFFER* RDR_idx_create_dyn(E_RDR_IDXTYPE type, int n) {
	return IB_create(type, n, true);
}

void RDR_idx_release(RDR_IDX_BUFFER* pIB) {
	RDR_WORK* pRdr = &s_rdr;
	if (pIB && pIB->handle) {
		ULONG refs = ((IDirect3DIndexBuffer9*)pIB->handle)->Release();
		if (0 == refs) {
			pRdr->mRsrc.mIdx.Remove(pIB);
		}
	}
}

void RDR_idx_lock(RDR_IDX_BUFFER* pIB) {
	if (pIB) {
		if (pIB->pData) {
			SYS_log("RDR_idx_lock: already locked\n");
		} else {
			((IDirect3DIndexBuffer9*)pIB->handle)->Lock(0, 0, &pIB->pData, 0);
		}
	}
}

void RDR_idx_unlock(RDR_IDX_BUFFER* pIB) {
	if (pIB) {
		if (pIB->pData) {
			((IDirect3DIndexBuffer9*)pIB->handle)->Unlock();
			pIB->pData = NULL;
		} else {
			SYS_log("RDR_idx_unlock: not locked\n");
		}
	}
}

static RDR_TEXTURE* Tex_create(int w, int h, int d, int nb_lvl, E_RDR_TEXTYPE type, D3DFORMAT fmt) {
	HRESULT hres;
	RDR_WORK* pRdr = &s_rdr;
	IDirect3DDevice9* pDev = pRdr->mpDev;
	RDR_TEXTURE tex;
	RDR_TEXTURE* pTex = NULL;
	const D3DPOOL pool = D3DPOOL_MANAGED;

	memset(&tex, 0, sizeof(RDR_TEXTURE));
	tex.type = type;
	switch (type) {
		case E_RDR_TEXTYPE_2D:
			hres = pDev->CreateTexture(w, h, nb_lvl, 0, fmt, pool, (IDirect3DTexture9**)&tex.handle, NULL);
			if (SUCCEEDED(hres) && tex.handle) {
				tex.w = w;
				tex.h = h;
			}
			break;
		case E_RDR_TEXTYPE_CUBE:
			hres = pDev->CreateCubeTexture(w, nb_lvl, 0, fmt, pool, (IDirect3DCubeTexture9**)&tex.handle, NULL);
			if (SUCCEEDED(hres) && tex.handle) {
				tex.w = w;
				tex.h = h;
			}
			break;
		case E_RDR_TEXTYPE_VOLUME:
			hres = pDev->CreateVolumeTexture(w, h, d, nb_lvl, 0, fmt, pool, (IDirect3DVolumeTexture9**)&tex.handle, NULL);
			if (SUCCEEDED(hres) && tex.handle) {
				tex.w = w;
				tex.h = h;
				tex.d = d;
			}
			break;
		default:
			SYS_log("Unknown texture type: 0x%X\n", type);
			break;
	}
	if (tex.handle) {
		tex.nb_lvl = (sys_byte)((IDirect3DBaseTexture9*)tex.handle)->GetLevelCount();
		pTex = pRdr->mRsrc.mTex.Add(tex);
	}

	return pTex;
}

static int Tex_fmt_ck(sys_ui32 fmt) {
	if (fmt == D3DFMT_DXT1 || fmt == D3DFMT_DXT3 || fmt == D3DFMT_DXT5) {
		return 1;
	}
	return 0;
}

RDR_TEXTURE* RDR_tex_create(void* pTop, int w, int h, int d, int nb_lvl, sys_ui32 fmt, sys_i32* pOffs) {
	int i;
	int n, nb_face;
	E_RDR_TEXTYPE type;
	D3DLOCKED_RECT rect;
	RDR_TEXTURE* pTex = NULL;

	if (!Tex_fmt_ck(fmt)) {
		SYS_log("Unknown texture format: 0x%X\n", fmt);
		return NULL;
	}
	nb_face = 1;
	if (d == 0) {
		type = E_RDR_TEXTYPE_2D;
	} else if (d < 0) {
		nb_face = 6;
		type = E_RDR_TEXTYPE_CUBE;
	} else {
		type = E_RDR_TEXTYPE_VOLUME;
		return NULL;
	}

	pTex = Tex_create(w, h, d, nb_lvl, type, (D3DFORMAT)fmt);
	if (!pTex) return NULL;

	for (n = 0; n < nb_face; ++n) {
		w = pTex->w;
		h = pTex->h;
		for (i = 0; i < nb_lvl; ++i) {
			void* pLvl = D_INCR_PTR(pTop, *pOffs);
			int lvl_size = 0;
			switch (fmt) {
				case D3DFMT_DXT1:
					lvl_size = ((w+3)/4) * ((h+3)/4) * 8;
					break;
				case D3DFMT_DXT3:
				case D3DFMT_DXT5:
					lvl_size = ((w+3)/4) * ((h+3)/4) * 16;
					break;
				default:
					break;
			}
			rect.pBits = NULL;
			if (type == E_RDR_TEXTYPE_2D) {
				((IDirect3DTexture9*)pTex->handle)->LockRect(i, &rect, NULL, 0);
			} else if (type == E_RDR_TEXTYPE_CUBE) {
				((IDirect3DCubeTexture9*)pTex->handle)->LockRect((D3DCUBEMAP_FACES)n, i, &rect, NULL, 0);
			}
			if (rect.pBits) {
				int y;
				void* pSrc;
				void* pDst;
				if (fmt == D3DFMT_DXT1 || fmt == D3DFMT_DXT3 || fmt == D3DFMT_DXT5) {
					int blk_flg = 0;
					if (fmt == D3DFMT_DXT1) {
						if (rect.Pitch != w*2) {
							blk_flg = 1;
						}
					} else {
						if (rect.Pitch != w*4) {
							blk_flg = 1;
						}
					}
					if (blk_flg) {
						int nb_blk_w = w > 0 ? D_MAX(1, w/4) : 0;
						int nb_blk_h = h > 0 ? D_MAX(1, h/4) : 0;
						int src_pitch = fmt == D3DFMT_DXT1 ? nb_blk_w*8 : nb_blk_w*16;
						pSrc = pLvl;
						pDst = rect.pBits;
						for (y = 0; y < nb_blk_h; ++y) {
							memset(pDst, 0, rect.Pitch);
							memcpy(pDst, pSrc, src_pitch);
							pSrc = D_INCR_PTR(pSrc, src_pitch);
							pDst = D_INCR_PTR(pDst, rect.Pitch);
						}
					} else {
						memcpy(rect.pBits, pLvl, lvl_size);
					}
				}
			}
			if (type == E_RDR_TEXTYPE_2D) {
				((IDirect3DTexture9*)pTex->handle)->UnlockRect(i);
			} else if (type == E_RDR_TEXTYPE_CUBE) {
				((IDirect3DCubeTexture9*)pTex->handle)->UnlockRect((D3DCUBEMAP_FACES)n, i);
			}
			w >>= 1;
			if (!w) w = 1;
			h >>= 1;
			if (!h) h = 1;
			++pOffs;
		}
	}
	return pTex;
}


void RDR_tex_release(RDR_TEXTURE* pTex) {
	RDR_WORK* pRdr = &s_rdr;
	if (pTex && pTex->handle) {
		ULONG refs = ((IDirect3DBaseTexture9*)pTex->handle)->Release();
		if (0 == refs) {
			pRdr->mRsrc.mTex.Remove(pTex);
		}
	}
}

