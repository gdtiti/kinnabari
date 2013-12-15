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

D_EXTERN_FUNC void INP_init(void);
D_EXTERN_FUNC void INP_reset(void);
D_EXTERN_FUNC void INP_update(void);
