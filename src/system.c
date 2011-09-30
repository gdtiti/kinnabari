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

