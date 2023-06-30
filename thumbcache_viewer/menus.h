/*
	thumbcache_viewer will extract thumbnail images from thumbcache database files.
	Copyright (C) 2011-2023 Eric Kutcher

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MENUS_H
#define MENUS_H

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#define MENU_OPEN		1001
#define MENU_SAVE_ALL	1002
#define MENU_SAVE_SEL	1003
#define MENU_EXPORT		1004
#define MENU_EXIT		1005
#define MENU_ABOUT		1006
#define MENU_SELECT_ALL	1007
#define MENU_REMOVE_SEL	1008
#define MENU_HIDE_BLANK	1009
#define MENU_CHECKSUMS	1010
#define MENU_SCAN		1011
#define MENU_INFO		1012
#define MENU_PROP_VALUE	1013
#define MENU_COPY_SEL	1014
#define MENU_HOME_PAGE	1015

#define UM_DISABLE			0
#define UM_ENABLE			1
#define UM_DISABLE_OVERRIDE	2

void UpdateMenus( unsigned char action );
void CreateMenus();

extern HMENU g_hMenu;				// Handle to our menu bar.
extern HMENU g_hMenuSub_context;	// Handle to our context menu.
extern HMENU g_hMenuSub_ei_context;	// Handle to our extended info context menu.

#endif
