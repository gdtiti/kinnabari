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
typedef sys_i32 sys_intptr;
#endif

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

#ifdef __cplusplus
}
#endif