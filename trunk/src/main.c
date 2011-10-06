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

#include "main.h"

struct _GLOBAL_WORK {
	int   w;
	int   h;
	HWND  hWnd;
	HINSTANCE hInst;
	long frame_start_time;
	long frame_end_time;
	long frame_delta_time;
	int flip_bit;
} g_wk;

static const TCHAR* s_build_date = _T(__DATE__);

static LRESULT CALLBACK Wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	LRESULT res = 0;
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		res = DefWindowProc(hWnd, msg, wParam, lParam);
		break;
	}
	return res;
}


static void Data_init() {
	CAM_init(&g_cam);
	PLR_init();
	g_pl.pAnm->move.pos.qv = V4_set_pnt(3.075f, 0.0f, -0.665f);
	g_pl.pAnm->move.heading = D_DEG2RAD(8.0f);
	ROOM_init(0);
}

static void Data_free() {
	ROOM_free();
	PLR_free();
}

static void Wrk_init_func(JOB_WORKER* pWrk) {
	struct {
		DWORD  type;
		LPCSTR pName;
		DWORD  tid;
		DWORD  flg;
	} info;
	static char* names[] = {"wrk0", "wrk1", "wrk2", "wrk3", "wrk4", "wrk5", "wrk6", "wrk7"};
	SYS_log("Initializing worker %d\n", pWrk->id);
	info.type = 0x1000;
	info.pName = names[pWrk->id & 7];
	info.tid = -1;
	info.flg = 0;
	__try {
		RaiseException(0x406D1388, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR*)&info);
	} __except(EXCEPTION_CONTINUE_EXECUTION) {}
	RDR_init_thread_FPU();
}

static void Init() {
	ATOM atom;
	RECT rect;
	WNDCLASSEX wc;
	static const TCHAR* cname = _T("KnbClass");
	int w = 800;
	int h = 600;
	int style = WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_GROUP;

	SYS_init();
	CFG_init();
	ZeroMemory(&wc, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc = Wnd_proc;
	wc.hInstance = g_wk.hInst;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = cname;

	atom = RegisterClassEx(&wc);
	if (atom) {
		TCHAR title[128];
		g_wk.w = w;
		g_wk.h = h;
		rect.left = 0;
		rect.top = 0;
		rect.right = w;
		rect.bottom = h;
		AdjustWindowRect(&rect, style, FALSE);
		w = rect.right - rect.left;
		h = rect.bottom - rect.top;
		_stprintf_s(title, D_ARRAY_LENGTH(title), _T("%s: build %s"), _T("Kinnabari"), s_build_date);
		g_wk.hWnd = CreateWindowEx(0, cname, title, style, 0, 0, w, h, NULL, NULL, g_wk.hInst, NULL);
		if (g_wk.hWnd) {
			ShowWindow(g_wk.hWnd, SW_SHOW);
			UpdateWindow(g_wk.hWnd);

			RDR_init(g_wk.hWnd, g_wk.w, g_wk.h, FALSE);
			JOB_sys_init(Wrk_init_func);
			MTL_sys_init();
			MDL_sys_init();

			Data_init();
		}
	}

}

static void Draw() {
	static int ftime = 1000/60;

	g_wk.frame_start_time = GetTickCount();

	SYS_get_input();

	RDR_begin();
	PLR_ctrl();
	CAM_exec(&g_cam, g_pl.pMdl->pos.qv, 0.5f, 0.5f, g_pl.pAnm->move.heading);
	CAM_update(&g_cam);
	CAM_apply(&g_cam);

	ROOM_cull(&g_cam);
	ROOM_disp();
	PLR_calc(&g_cam);

	RDR_exec();


	g_wk.flip_bit ^= 1;
	g_wk.frame_end_time = GetTickCount();
	g_wk.frame_delta_time = g_wk.frame_end_time - g_wk.frame_start_time;
	if (g_wk.frame_delta_time < ftime) {
		Sleep(ftime - g_wk.frame_delta_time);
	}
}

static void Loop() {
	MSG msg;
	int done = 0;

	if (!g_wk.hWnd) return;
	while (!done) {
		if (PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE)) {
			if (GetMessage(&msg, NULL, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} else {
				done = 1;
				break;
			}
		} else {
				Draw();
				if (done) PostMessage(g_wk.hWnd, WM_CLOSE, 0, 0);
		}
	}
}

static void Reset() {
	Data_free();
	MTL_sys_reset();
	MDL_sys_reset();
	JOB_sys_reset();
	RDR_reset();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	ZeroMemory(&g_wk, sizeof(g_wk));
	g_wk.hInst = hInstance;

	Init();
	Loop();
	Reset();

	return 0;
}

