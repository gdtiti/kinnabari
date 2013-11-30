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

#include <tchar.h>

#include <stdio.h>
#include <stddef.h>
#include <malloc.h>

#ifdef __cplusplus
#	define D_EXTERN_FUNC extern "C"
#	define D_EXTERN_DATA extern "C"
#else
#	define D_EXTERN_FUNC
#	define D_EXTERN_DATA extern
#endif

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#	define D_INLINE __inline
#	define D_FORCE_INLINE __forceinline
#	define D_NOINLINE __declspec(noinline) 
#elif defined(__GNUC__)
#	define D_INLINE __inline__
#	define D_FORCE_INLINE __inline__ __attribute__((__always_inline__))
#	define D_NOINLINE 
#else
#	define D_INLINE
#	define D_FORCE_INLINE 
#	define D_NOINLINE 
#endif

#if defined (__GNUC__)
#	define D_DATA_ALIGN16_PREFIX
#	define D_DATA_ALIGN16_POSTFIX __attribute__ ( (aligned(16)) )
#elif defined(_MSC_VER)
#	if (_MSC_VER < 1300)
#		define D_DATA_ALIGN16_PREFIX
#		define D_DATA_ALIGN16_POSTFIX
#	else
#		define D_DATA_ALIGN16_PREFIX __declspec(align(16))
#		define D_DATA_ALIGN16_POSTFIX
#	endif
#else
#	define D_DATA_ALIGN16_PREFIX
#	define D_DATA_ALIGN16_POSTFIX
#endif

#define D_DATA_ALIGN16(_d) D_DATA_ALIGN16_PREFIX _d D_DATA_ALIGN16_POSTFIX

#if defined (__GNUC__)
#	define D_STRUCT_ALIGN16_PREFIX
#	define D_STRUCT_ALIGN16_POSTFIX __attribute__ ( (aligned(16)) )
#elif defined(_MSC_VER)
#	if (_MSC_VER < 1300)
#		define D_STRUCT_ALIGN16_PREFIX
#		define D_STRUCT_ALIGN16_POSTFIX
#	else
#		define D_STRUCT_ALIGN16_PREFIX __declspec(align(16))
#		define D_STRUCT_ALIGN16_POSTFIX
#	endif
#else
#	define D_STRUCT_ALIGN16_PREFIX
#	define D_STRUCT_ALIGN16_POSTFIX
#endif

#define D_INCR_PTR(_ptr, _inc) ( &((char*)(_ptr))[_inc] )
#define D_ARRAY_LENGTH(_arr) ( sizeof((_arr)) / sizeof((_arr)[0]) )
#define D_ALIGN(_x, _n) ( ((sys_intptr)(_x) + ((_n) - 1)) & (~((_n) - 1)) )

#if 0
#define D_FIELD_OFFS(_type, _field) (size_t)((char*)&((_type*)NULL)->_field)
#else
#define D_FIELD_OFFS(_type, _field) offsetof(_type, _field)
#endif

#define D_FOURCC(c1, c2, c3, c4) ((((sys_byte)(c4))<<24)|(((sys_byte)(c3))<<16)|(((sys_byte)(c2))<<8)|((sys_byte)(c1)))

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
	typedef __int8 sys_i8;
	typedef unsigned __int8 sys_ui8;
	typedef __int16 sys_i16;
	typedef unsigned __int16 sys_ui16;
	typedef __int32 sys_i32;
	typedef unsigned __int32 sys_ui32;
	typedef __int64 sys_i64;
	typedef unsigned __int64 sys_ui64;
	typedef struct _SYS_I128STRUC {sys_ui64 lo; sys_i64 hi;} sys_i128;
	typedef struct _SYS_UI128STRUC {sys_ui64 lo; sys_ui64 hi;} sys_ui128;
#else
	typedef signed char sys_i8;
	typedef unsigned char sys_ui8;
	typedef short sys_i16;
	typedef unsigned short sys_ui16;
	typedef long sys_i32;
	typedef unsigned long sys_ui32;
#	if defined(__GNUC__)
		typedef long long sys_i64;
		typedef unsigned long long sys_ui64;
		typedef struct _SYS_I128STRUC {sys_ui64 lo; sys_i64 hi;} sys_i128 __attribute__((aligned(16)));
		typedef struct _SYS_UI128STRUC {sys_ui64 lo; sys_ui64 hi;} sys_ui128 __attribute__((aligned(16)));
#	else
		/* little-endian */
		typedef struct _SYS_I64STRUC {sys_ui32 lo; sys_i32_t hi;} sys_i64;
		typedef struct _SYS_UI64STRUC {sys_ui32 lo; sys_ui32_t hi;} sys_ui64;
		typedef struct _SYS_I128STRUC {sys_ui64 lo; sys_i64 hi;} sys_i128;
		typedef struct _SYS_UI128STRUC {sys_ui64 lo; sys_ui64 hi;} sys_ui128;
#	endif
#endif

typedef sys_ui8       sys_byte;
typedef int           sys_int;
typedef unsigned int  sys_uint;
typedef long          sys_long;
typedef unsigned long sys_ulong;

#ifdef _INTPTR_T_DEFINED
typedef intptr_t sys_intptr;
#else
# if defined(_WIN64)
typedef sys_i64 sys_intptr;
# else
typedef sys_i32 sys_intptr;
# endif
#endif

typedef void* sys_handle;

typedef enum _E_KEY {
	E_KEY_LEFT   = (1<<0),
	E_KEY_RIGHT  = (1<<1),
	E_KEY_UP     = (1<<2),
	E_KEY_DOWN   = (1<<3)
} E_KEY;

typedef struct _INPUT_STATE {
	sys_ui32 state;
	sys_ui32 state_old;
	sys_ui32 state_chg;
	sys_ui32 state_trg;
} INPUT_STATE;

D_EXTERN_DATA INPUT_STATE g_input;

typedef union _SYS_ADDR {
	sys_ui64 addr64;
	void* ptr;
	struct {
		sys_ui32 low;
		sys_ui32 high;
	};
} SYS_ADDR;

typedef struct _SYS_MUTEX {
#if defined(_WIN64)
	sys_ui8 cs[0x28];
#else
	sys_ui8 cs[0x18];
#endif
} SYS_MUTEX;

typedef struct _SYS_REMOTE_INFO {
	SYS_ADDR addr;
	sys_ui32 pid;
} SYS_REMOTE_INFO;

typedef struct _SYS_REMOTE_GATEWAY {
	SYS_ADDR code;
	SYS_ADDR data;
} SYS_REMOTE_GATEWAY;

#ifdef __cplusplus
extern "C" {
#endif

void SYS_init(void);
void* SYS_malloc(int size);
void SYS_free(void* pMem);
void SYS_log(const char* fmt, ...);
void* SYS_load(const char* fname);
void SYS_get_input(void);
sys_i64 SYS_get_timestamp(void);
void SYS_init_FPU(void);
void SYS_con_init(void);
void SYS_mutex_init(SYS_MUTEX* pMut);
void SYS_mutex_reset(SYS_MUTEX* pMut);
void SYS_mutex_enter(SYS_MUTEX* pMut);
void SYS_mutex_leave(SYS_MUTEX* pMut);
int SYS_adjust_privileges(void);
sys_ui32 SYS_pid_get(const char* name);
int SYS_pid_ck(sys_ui32 pid);
sys_handle SYS_proc_open(sys_ui32 pid);
void SYS_proc_close(sys_handle hnd);
sys_ui32 SYS_proc_read(sys_handle hnd, sys_ui64 addr, void* pDst, int size);
sys_ui32 SYS_proc_write(sys_handle hnd, sys_ui64 addr, void* pSrc, int size);
void SYS_proc_call(sys_handle hnd, void* pCode, void* pData);
int SYS_proc_active(sys_handle hnd);
sys_handle SYS_shared_mem_create(sys_ui32 size);
void SYS_shared_mem_destroy(sys_handle hnd);
sys_handle SYS_shared_mem_open(sys_handle hMem, sys_handle hProc);
void* SYS_shared_mem_map(sys_handle hMem);
void SYS_shared_mem_unmap(void* p);
void SYS_remote_init(sys_handle hWnd, SYS_ADDR addr);
int SYS_remote_handshake(const char* proc_name, const char* class_name, SYS_REMOTE_INFO* pInfo);

#ifdef __cplusplus
}
#endif
