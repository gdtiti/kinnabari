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
#define NOMINMAX
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <tchar.h>
#include <psapi.h>

#include "system.h"

INPUT_STATE g_input;

static struct _INP_KEYMAP {
	sys_uint key, code;
} s_key_map[] = {
	{VK_LEFT,     E_KEY_LEFT},
	{VK_RIGHT,    E_KEY_RIGHT},
	{VK_UP,       E_KEY_UP},
	{VK_DOWN,     E_KEY_DOWN}
};

static void Sys_w32_cwd() {
	LPTSTR cmd;
	LPTSTR exe_name;
	TCHAR path[MAX_PATH];

	cmd = GetCommandLine();
	if (*cmd == '"') ++cmd;
	exe_name = _tcsrchr(cmd, '\\');
	if (exe_name) {
		size_t path_size = exe_name - cmd;
		_tcsncpy_s(path, D_ARRAY_LENGTH(path), cmd, path_size);
		path[path_size] = 0;
		SetCurrentDirectory(path);
	}
}

void SYS_init() {
	Sys_w32_cwd();
	memset(&g_input, 0, sizeof(g_input));
}

void* SYS_malloc(int size) {
	void* pMem = NULL;
	if (size) {
		size_t asize = D_ALIGN(size + sizeof(int) + 16, 16);
		void* pMem0 = malloc(asize);
		pMem = (void*)D_ALIGN((sys_intptr)pMem0 + sizeof(int), 16);
		((int*)pMem)[-1] = (int)((sys_intptr)pMem - (sys_intptr)pMem0);
	}
	return pMem;
}

void SYS_free(void* pMem) {
	if (pMem) {
		void* pMem0 = (void*)((sys_intptr)pMem - ((int*)pMem)[-1]);
		free(pMem0);
	}
}

/*
 * DbgView is the easiest way to see the logged messages when not running under debugger:
 * http://technet.microsoft.com/en-us/sysinternals/bb896647
 */
void SYS_log(const char* fmt, ...) {
	char msg[1024*2];
	va_list marker;
	va_start(marker, fmt);
	vsprintf_s(msg, sizeof(msg), fmt, marker);
	va_end(marker);
	OutputDebugStringA(msg);
}


void* SYS_load(const char* fname) {
	FILE* f;
	void* pData = NULL;
	char fpath[256];

	sprintf_s(fpath, sizeof(fpath), "../data/%s", fname);
	if (0 == fopen_s(&f, fpath, "rb") && f) {
		long len = 0;
		long old = ftell(f);
		if (0 == fseek(f, 0, SEEK_END)) {
			len = ftell(f);
		}
		fseek(f, old, SEEK_SET);
		pData = SYS_malloc(len);
		if (pData) {
			fread(pData, len, 1, f);
		}
		fclose(f);
	}
	return pData;
}

void SYS_get_input() {
	int i;
	sys_uint state;
	state = 0;
	for (i = 0; i < D_ARRAY_LENGTH(s_key_map); ++i) {
		int b = !!(GetAsyncKeyState(s_key_map[i].key) & 0x8000);
		if (b) state |= s_key_map[i].code;
	}
	if ( (state & (E_KEY_LEFT | E_KEY_RIGHT)) == (E_KEY_LEFT | E_KEY_RIGHT) ) {
		if (g_input.state_old & E_KEY_LEFT) {
			state &= ~E_KEY_RIGHT;
		} else if (g_input.state_old & E_KEY_RIGHT) {
			state &= ~E_KEY_LEFT;
		} else {
			state &= ~E_KEY_LEFT;
		}
	}
	if ( (state & (E_KEY_UP | E_KEY_DOWN)) == (E_KEY_UP | E_KEY_DOWN) ) {
		if (g_input.state_old & E_KEY_UP) {
			state &= ~E_KEY_DOWN;
		} else if (g_input.state_old & E_KEY_DOWN) {
			state &= ~E_KEY_UP;
		} else {
			state &= ~E_KEY_UP;
		}
	}
	g_input.state_old = g_input.state;
	g_input.state = state;
	g_input.state_chg = state ^ g_input.state_old;
	g_input.state_trg = state & g_input.state_chg;
}

static void CPU_serialize() {
#if defined (__GNUC__) || defined(_WIN64)
	int dummy[4];
	__cpuid(dummy, 0);
#else
	__asm {
		sub eax, eax
		cpuid
	}
#endif
}

static sys_i64 Sys_timestamp_impl() {
#if 0
	sys_i64 res;
	DWORD_PTR mask;
	HANDLE hThr = GetCurrentThread();
	mask = SetThreadAffinityMask(hThr, 1);
	CPU_serialize();
	res = __rdtsc();
	SetThreadAffinityMask(hThr, mask);
	return res;
#else
	LARGE_INTEGER ctr;
	QueryPerformanceCounter(&ctr);
	return ctr.QuadPart;
#endif
}

sys_i64 SYS_get_timestamp() {
	sys_i64 res;
	CPU_serialize();
	res = Sys_timestamp_impl();
	CPU_serialize();
	return res;
}

void SYS_init_FPU() {
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
	sys_ui16 fcw;
	asm("fnstcw %w0" : "=m" (fcw));
	fcw &= ~(3<<8);
	asm("fldcw %w0" : : "m" (fcw));
#	endif
#endif
}

static void Con_attach() {
#if defined(_WIN64)
	AttachConsole(GetCurrentProcessId());
#else
	BOOL (WINAPI *fnAttachConsole)(DWORD pid) = 0;
	FARPROC* pProc = (FARPROC*)&fnAttachConsole;
	*pProc = GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "AttachConsole");
	if (fnAttachConsole) {
		fnAttachConsole(GetCurrentProcessId());
	}
#endif
}

void SYS_con_init() {
	FILE* pConFile;
	AllocConsole();
	Con_attach();
	freopen_s(&pConFile, "CON", "w", stdout);
	freopen_s(&pConFile, "CON", "w", stderr);
}

void SYS_mutex_init(SYS_MUTEX* pMut) {
	InitializeCriticalSection((CRITICAL_SECTION*)pMut->cs);
}

void SYS_mutex_reset(SYS_MUTEX* pMut) {
	DeleteCriticalSection((CRITICAL_SECTION*)pMut->cs);
}

void SYS_mutex_enter(SYS_MUTEX* pMut) {
	EnterCriticalSection((CRITICAL_SECTION*)pMut->cs);
}

void SYS_mutex_leave(SYS_MUTEX* pMut) {
	LeaveCriticalSection((CRITICAL_SECTION*)pMut->cs);
}

int SYS_adjust_privileges() {
	BOOL b;
	LUID luid;
	HANDLE tok;
	DWORD len;
	TOKEN_PRIVILEGES priv;
	TOKEN_PRIVILEGES priv_old;

	b = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok);
	if (!b) return 0;
	b = LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid);
	if (!b) {
		CloseHandle(tok);
		return 0;
	}

	ZeroMemory(&priv, sizeof(TOKEN_PRIVILEGES));
	priv.PrivilegeCount = 1;
	priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	priv.Privileges[0].Luid = luid;
	len = sizeof(TOKEN_PRIVILEGES);
	b = AdjustTokenPrivileges(tok, FALSE, &priv, sizeof(TOKEN_PRIVILEGES), &priv_old, &len);
	CloseHandle(tok);
	return !!b;
}

static DWORD W32_get_pid(LPTSTR exe_name) {
	DWORD list[256];
	DWORD nbytes;
	if (EnumProcesses(list, sizeof(list), &nbytes)) {
		int i;
		TCHAR name[1024];
		HANDLE hproc;
		int nproc = nbytes / sizeof(DWORD);

		for (i = 0; i < nproc; ++i) {
			hproc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, list[i]);
			if (hproc) {
				TCHAR* pTail;
				GetProcessImageFileName(hproc, name, sizeof(name)/sizeof(TCHAR));
				CloseHandle(hproc);
				pTail = name;
				while (1) {
					if (*pTail == 0) {
						break;
					}
					++pTail;
				}
				if (pTail == name) continue;
				while (1) {
					if (*pTail == '\\') {
						++pTail;
						break;
					}
					--pTail;
					if (pTail == name) break;
				}
				if (0 == _tcscmp(pTail, exe_name)) {
					return list[i];
				}
			}
		}
	}
	return -1;
}

sys_ui32 SYS_pid_get(const char* name) {
	int i;
	TCHAR tname[1024];
	sys_ui32 pid = (sys_ui32)-1;
	int n = (int)strlen(name);
	for (i = 0; i < n; ++i) {
		tname[i] = name[i];
	}
	tname[n] = 0;
	pid = W32_get_pid(tname);
	return pid;
}

int SYS_pid_ck(sys_ui32 pid) {
	return pid != (sys_ui32)-1;
}

sys_handle SYS_proc_open(sys_ui32 pid) {
	HANDLE hnd = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	return (sys_handle)hnd;
}

void SYS_proc_close(sys_handle hnd) {
	CloseHandle((HANDLE)hnd);
}

sys_ui32 SYS_proc_read(sys_handle hnd, sys_ui64 addr, void* pDst, int size) {
	MEMORY_BASIC_INFORMATION mi;
	SIZE_T nread = 0;
	union {void* p; sys_ui64 a;} ptr;
	size_t offs;
	HANDLE hproc = (HANDLE)hnd;

	ptr.p = NULL;
	ptr.a = addr;
	VirtualQueryEx(hproc, ptr.p, &mi, sizeof(MEMORY_BASIC_INFORMATION));
	offs = (char*)ptr.p - (char*)mi.BaseAddress;
	ReadProcessMemory(hproc, (char*)mi.BaseAddress + offs, pDst, size, &nread);
	return (sys_ui32)nread;
}

sys_ui32 SYS_proc_write(sys_handle hnd, sys_ui64 addr, void* pSrc, int size) {
	size_t offs;
	DWORD res;
	SIZE_T written = 0;
	MEMORY_BASIC_INFORMATION mi;
	union {void* p; sys_ui64 a;} ptr;
	HANDLE hproc = (HANDLE)hnd;

	ptr.a = addr;
	VirtualQueryEx(hproc, ptr.p, &mi, sizeof(MEMORY_BASIC_INFORMATION));
	offs = (char*)ptr.p - (char*)mi.BaseAddress;
	res = WriteProcessMemory(hproc, (char*)mi.BaseAddress + offs, pSrc, size, &written);
	return (sys_ui32)written;
}

void SYS_proc_call(sys_handle hnd, void* pCode, void* pData) {
	HANDLE hproc = (HANDLE)hnd;
	HANDLE thandle;
	DWORD tid;
	thandle = CreateRemoteThread(hproc, NULL, 0, (LPTHREAD_START_ROUTINE)pCode, pData, CREATE_SUSPENDED, &tid);
	if (thandle) {
		ResumeThread(thandle);
		WaitForSingleObject(thandle, INFINITE);
		CloseHandle(thandle);
	}
}

int SYS_proc_active(sys_handle hnd) {
	HANDLE hproc = (HANDLE)hnd;
	DWORD code;
	if (GetExitCodeProcess(hproc, &code)) {
		return STILL_ACTIVE == code;
	}
	return 0;
}

sys_handle SYS_shared_mem_create(sys_ui32 size) {
	HANDLE hnd = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, NULL);
	return (sys_handle)hnd;
}

void SYS_shared_mem_destroy(sys_handle hnd) {
	CloseHandle((HANDLE)hnd);
}

sys_handle SYS_shared_mem_open(sys_handle hMem, sys_handle hProc) {
	sys_handle h = 0;
	HANDLE hTgt;
	if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hMem, (HANDLE)hProc, &hTgt, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
		h = (sys_handle)hTgt;
	}
	return h;
}

void* SYS_shared_mem_map(sys_handle hMem) {
	void* p = NULL;
	p = MapViewOfFileEx((HANDLE)hMem, FILE_MAP_WRITE, 0, 0, 0, NULL);
	return p;
}

void SYS_shared_mem_unmap(void* p) {
	UnmapViewOfFile(p);
}

