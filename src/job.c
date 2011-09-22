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

#include "system.h"
#include "job.h"

#define D_SYNC_INC(pVal) ((sys_i32)_InterlockedIncrement((sys_long*)(pVal)))
#define D_SYNC_DEC(pVal) ((sys_i32)_InterlockedDecrement((sys_long*)(pVal)))
#define D_SYNC_XCHG(pVal, new_val) ((sys_i32)_InterlockedExchange((sys_long*)(pVal), (sys_long)(new_val)))

JOB_SYS g_job_sys = {NULL};
static JOB_WRK_INIT_FUNC s_job_wrk_init_func = NULL;

static void Job_exec(JOB_WORKER* pWrk) {
	long count;
	long* pIdx;
	long idx;
	JOB* pJob;
	JOB_SCHEDULER* pSdl = &g_job_sys.scheduler;
	JOB_QUEUE* pQue = pSdl->pQue;

	if (!pQue) return;
	count = pQue->count;
	if (!count) return;
	pIdx = &pQue->idx;
	while (*pIdx < count) {
		idx = D_SYNC_INC(pIdx);
		if (idx > count) break;
		pJob = &pQue->pJob[idx-1];
		pJob->func(pJob->pData);
		++pWrk->exec_count;
	}
}

static DWORD APIENTRY Job_worker_main(void* pData) {
	JOB_SYS* pSys = &g_job_sys;
	JOB_WORKER* pWrk = (JOB_WORKER*)pData;
	if (pSys->wrk_init_func) {
		pSys->wrk_init_func(pWrk);
	}
	SetEvent(pWrk->done_sig);
	while (!pWrk->end_flg) {
		if (WaitForSingleObject(pWrk->exec_sig, INFINITE) == WAIT_OBJECT_0) {
			ResetEvent(pWrk->exec_sig);
			Job_exec(pWrk);
			SetEvent(pWrk->done_sig);
		}
	}
	return 0;
}

void JOB_sys_init(JOB_WRK_INIT_FUNC wrk_init_func) {
	int i;
	JOB_WORKER* pWrk;
	JOB_SYS* pSys = &g_job_sys;
	JOB_SCHEDULER* pSdl = &pSys->scheduler;

	pSys->wrk_init_func = wrk_init_func;
	pSdl->main_tid = GetCurrentThreadId();
	pSdl->pQue = NULL;
	pWrk = &pSdl->worker[0];
	for (i = 0; i < D_MAX_WORKERS; ++i) {
		pWrk->thandle = CreateThread(NULL, 0, Job_worker_main, pWrk, CREATE_SUSPENDED, &pWrk->tid);
		pWrk->exec_sig = CreateEvent(NULL, FALSE, FALSE, NULL);
		pWrk->done_sig = CreateEvent(NULL, FALSE, FALSE, NULL);
		pWrk->end_flg = 0;
		pWrk->id = i;
		++pWrk;
	}
	pWrk = &pSdl->worker[0];
	for (i = 0; i < D_MAX_WORKERS; ++i) {
		ResumeThread(pWrk->thandle);
		WaitForSingleObject(pWrk->done_sig, INFINITE);
		++pWrk;
	}
}

void JOB_sys_reset() {
	int i;
	JOB_WORKER* pWrk;
	JOB_SYS* pSys = &g_job_sys;
	JOB_SCHEDULER* pSdl = &pSys->scheduler;

	pWrk = &pSdl->worker[0];
	for (i = 0; i < D_MAX_WORKERS; ++i) {
		pWrk->end_flg = 1;
		ResetEvent(pWrk->done_sig);
		SetEvent(pWrk->exec_sig);
		WaitForSingleObject(pWrk->thandle, INFINITE);
		++pWrk;
	}
	pWrk = &pSdl->worker[0];
	for (i = 0; i < D_MAX_WORKERS; ++i) {
		CloseHandle(pWrk->exec_sig);
		CloseHandle(pWrk->done_sig);
		CloseHandle(pWrk->thandle);
		++pWrk;
	}
}

JOB_QUEUE* JOB_que_alloc(sys_ui32 size) {
	JOB_QUEUE* pQue = NULL;
	sys_uint mem_size = sizeof(JOB_QUEUE) + sizeof(JOB)*size;
	pQue = SYS_malloc(mem_size);
	if (pQue) {
		pQue->size = size;
		pQue->pJob = (JOB*)(pQue+1);
		JOB_que_clear(pQue);
	}
	return pQue;
}

void JOB_que_free(JOB_QUEUE* pQue) {
	SYS_free(pQue);
}

void JOB_que_clear(JOB_QUEUE* pQue) {
	pQue->count = 0;
	pQue->idx = 0;
}

void JOB_put(JOB_QUEUE* pQue, JOB* pJob) {
	memcpy(&pQue->pJob[pQue->count], pJob, sizeof(JOB));
	++pQue->count;
}

void JOB_schedule(JOB_QUEUE* pQue, sys_int nb_wrk) {
	int i;
	JOB_WORKER* pWrk;
	JOB_SYS* pSys = &g_job_sys;
	JOB_SCHEDULER* pSdl = &pSys->scheduler;
	JOB_HANDLE hlist[D_MAX_WORKERS];

	pSdl->pQue = pQue;
	pWrk = &pSdl->worker[0];
	for (i = 0; i < D_MAX_WORKERS; ++i) {
		pWrk->exec_count = 0;
		++pWrk;
	}
	if (nb_wrk > 1) {
		pWrk = &pSdl->worker[0];
		if (nb_wrk > D_MAX_WORKERS) nb_wrk = D_MAX_WORKERS;
		for (i = 0; i < nb_wrk; ++i) {
			ResetEvent(pWrk->done_sig);
			SetEvent(pWrk->exec_sig);
			++pWrk;
		}
		pWrk = &pSdl->worker[0];
		for (i = 0; i < nb_wrk; ++i) {
			hlist[i] = pWrk->done_sig;
			++pWrk;
		}
		WaitForMultipleObjects(nb_wrk, hlist, TRUE, INFINITE); /* barrier */
	} else {
		int n = pQue->count;
		JOB* pJob = pQue->pJob;
		for (i = 0; i < n; ++i) {
			pJob->func(pJob->pData);
			++pJob;
		}
	}
	JOB_que_clear(pQue);
	pSdl->pQue = NULL;
}

sys_int JOB_get_worker_id() {
	int i;
	JOB_SYS* pSys = &g_job_sys;
	JOB_SCHEDULER* pSdl = &pSys->scheduler;
	JOB_WORKER* pWrk = &pSdl->worker[0];
	sys_ui32 tid = GetCurrentThreadId();
	sys_int id = -1;
	for (i = 0; i < D_MAX_WORKERS; ++i) {
		if (pWrk->tid == tid) {
			id = pWrk->id;
			break;
		}
		++pWrk;
	}
	return id;
}

