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

#define D_MAX_WORKERS (8)

typedef void* JOB_HANDLE;

typedef struct _JOB_WORKER {
	JOB_HANDLE thandle;
	sys_ui32   tid;
	JOB_HANDLE exec_sig;
	JOB_HANDLE done_sig;
	sys_int    exec_count;
	sys_int    id;
	sys_int    end_flg;
} JOB_WORKER;

typedef void (*JOB_FUNC)(void*);
typedef void (*JOB_WRK_INIT_FUNC)(JOB_WORKER*);

typedef struct _JOB {
	void*    pData;
	JOB_FUNC func;
} JOB;

typedef struct _JOB_QUEUE {
	sys_long count;
	sys_long idx;
	sys_ui32 size;
	JOB*     pJob;
} JOB_QUEUE;

typedef struct _JOB_SCHEDULER {
	JOB_QUEUE* pQue;
	JOB_WORKER worker[D_MAX_WORKERS];
	sys_ui32   main_tid;
} JOB_SCHEDULER;

typedef struct _JOB_SYS {
	JOB_WRK_INIT_FUNC wrk_init_func;
	JOB_SCHEDULER     scheduler;
} JOB_SYS;

D_EXTERN_DATA JOB_SYS g_job_sys;

D_EXTERN_FUNC void JOB_sys_init(JOB_WRK_INIT_FUNC wrk_init_func);
D_EXTERN_FUNC void JOB_sys_reset(void);
D_EXTERN_FUNC JOB_QUEUE* JOB_que_alloc(sys_ui32 size);
D_EXTERN_FUNC void JOB_que_free(JOB_QUEUE* pQue);
D_EXTERN_FUNC void JOB_que_clear(JOB_QUEUE* pQue);
D_EXTERN_FUNC void JOB_put(JOB_QUEUE* pQue, JOB* pJob);
D_EXTERN_FUNC void JOB_schedule(JOB_QUEUE* pQue, sys_int nb_wrk);
D_EXTERN_FUNC void JOB_lock(void);
D_EXTERN_FUNC void JOB_unlock(void);
D_EXTERN_FUNC sys_int JOB_get_worker_id(void);
