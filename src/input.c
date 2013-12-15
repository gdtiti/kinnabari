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
#include "input.h"

INPUT_STATE g_input;

static struct _INP_KEYMAP {
	sys_uint key, code;
} s_key_map[] = {
	{VK_LEFT,     E_KEY_LEFT},
	{VK_RIGHT,    E_KEY_RIGHT},
	{VK_UP,       E_KEY_UP},
	{VK_DOWN,     E_KEY_DOWN}
};

void INP_init() {
	memset(&g_input, 0, sizeof(g_input));
}

void INP_reset() {
}

void INP_update() {
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
