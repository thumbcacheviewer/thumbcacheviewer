/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2015 Eric Kutcher

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

#include "menus.h"
#include "globals.h"

HMENU g_hMenu = NULL;				// Handle to our menu bar.
HMENU g_hMenuSub_context = NULL;	// Handle to our context menu.
HMENU g_hMenuSub_ei_context = NULL;	// Handle to our extended info context menu.

void CreateMenus()
{
	// Create our menu objects.
	g_hMenu = CreateMenu();
	HMENU hMenuSub_file = CreatePopupMenu();
	HMENU hMenuSub_edit = CreatePopupMenu();
	HMENU hMenuSub_view = CreatePopupMenu();
	HMENU hMenuSub_tools = CreatePopupMenu();
	HMENU hMenuSub_help = CreatePopupMenu();
	g_hMenuSub_context = CreatePopupMenu();
	g_hMenuSub_ei_context = CreatePopupMenu();

	// FILE MENU
	MENUITEMINFOA mii = { NULL };
	mii.cbSize = sizeof( MENUITEMINFOA );
	mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
	mii.fType = MFT_STRING;
	mii.dwTypeData = "&Open...\tCtrl+O";
	mii.cch = 15;
	mii.wID = MENU_OPEN;
	InsertMenuItemA( hMenuSub_file, 0, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( hMenuSub_file, 1, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Save All...\tCtrl+S";
	mii.cch = 18;
	mii.wID = MENU_SAVE_ALL;
	mii.fState = MFS_DISABLED;
	InsertMenuItemA( hMenuSub_file, 2, TRUE, &mii );

	mii.dwTypeData = "Save Selected...\tCtrl+Shift+S";
	mii.cch = 29;
	mii.wID = MENU_SAVE_SEL;
	InsertMenuItemA( hMenuSub_file, 3, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( hMenuSub_file, 4, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Export to CSV...\tCtrl+E";
	mii.cch = 23;
	mii.wID = MENU_EXPORT;
	InsertMenuItemA( hMenuSub_file, 5, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( hMenuSub_file, 6, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "E&xit";
	mii.cch = 5;
	mii.wID = MENU_EXIT;
	mii.fState = MFS_ENABLED;
	InsertMenuItemA( hMenuSub_file, 7, TRUE, &mii );

	// EDIT MENU
	mii.fType = MFT_STRING;
	mii.dwTypeData = "Remove Selected\tCtrl+R";
	mii.cch = 22;
	mii.wID = MENU_REMOVE_SEL;
	mii.fState = MFS_DISABLED;
	InsertMenuItemA( hMenuSub_edit, 0, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( hMenuSub_edit, 1, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Copy Selected\tCtrl+C";
	mii.cch = 20;
	mii.wID = MENU_COPY_SEL;
	InsertMenuItemA( hMenuSub_edit, 2, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( hMenuSub_edit, 3, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Select All\tCtrl+A";
	mii.cch = 17;
	mii.wID = MENU_SELECT_ALL;
	InsertMenuItemA( hMenuSub_edit, 4, TRUE, &mii );

	// VIEW MENU
	mii.fType = MFT_STRING;
	mii.dwTypeData = "Hide Blank Entries\tCtrl+H";
	mii.cch = 25;
	mii.wID = MENU_HIDE_BLANK;
	mii.fState = MFS_ENABLED;
	InsertMenuItemA( hMenuSub_view, 0, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( hMenuSub_view, 1, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Extended Information\tCtrl+I";
	mii.cch = 27;
	mii.wID = MENU_INFO;
	mii.fState = MFS_DISABLED;
	InsertMenuItemA( hMenuSub_view, 2, TRUE, &mii );

	// TOOLS MENU
	mii.fType = MFT_STRING;
	mii.dwTypeData = "Verify Checksums\tCtrl+V";
	mii.cch = 23;
	mii.wID = MENU_CHECKSUMS;
	InsertMenuItemA( hMenuSub_tools, 0, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Map File Paths...\tCtrl+M";
	mii.cch = 24;
	mii.wID = MENU_SCAN;
	InsertMenuItemA( hMenuSub_tools, 1, TRUE, &mii );

	// HELP MENU
	mii.dwTypeData = "&About";
	mii.cch = 6;
	mii.wID = MENU_ABOUT;
	mii.fState = MFS_ENABLED;
	InsertMenuItemA( hMenuSub_help, 0, TRUE, &mii );

	// MENU BAR
	mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
	mii.dwTypeData = "&File";
	mii.cch = 5;
	mii.hSubMenu = hMenuSub_file;
	InsertMenuItemA( g_hMenu, 0, TRUE, &mii );

	mii.dwTypeData = "&Edit";
	mii.cch = 5;
	mii.hSubMenu = hMenuSub_edit;
	InsertMenuItemA( g_hMenu, 1, TRUE, &mii );

	mii.dwTypeData = "&View";
	mii.cch = 5;
	mii.hSubMenu = hMenuSub_view;
	InsertMenuItemA( g_hMenu, 2, TRUE, &mii );

	mii.dwTypeData = "&Tools";
	mii.cch = 6;
	mii.hSubMenu = hMenuSub_tools;
	InsertMenuItemA( g_hMenu, 3, TRUE, &mii );

	mii.dwTypeData = "&Help";
	mii.cch = 5;
	mii.hSubMenu = hMenuSub_help;
	InsertMenuItemA( g_hMenu, 4, TRUE, &mii );

	// CONTEXT MENU (for right click)
	mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
	mii.fState = MFS_DISABLED;
	mii.dwTypeData = "Save Selected...";
	mii.cch = 16;
	mii.wID = MENU_SAVE_SEL;
	InsertMenuItemA( g_hMenuSub_context, 0, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( g_hMenuSub_context, 1, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Remove Selected";
	mii.cch = 15;
	mii.wID = MENU_REMOVE_SEL;
	InsertMenuItemA( g_hMenuSub_context, 2, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( g_hMenuSub_context, 3, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Copy Selected";
	mii.cch = 13;
	mii.wID = MENU_COPY_SEL;
	InsertMenuItemA( g_hMenuSub_context, 4, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( g_hMenuSub_context, 5, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Select All";
	mii.cch = 10;
	mii.wID = MENU_SELECT_ALL;
	InsertMenuItemA( g_hMenuSub_context, 6, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( g_hMenuSub_context, 7, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Extended Information";
	mii.cch = 20;
	mii.wID = MENU_INFO;
	InsertMenuItemA( g_hMenuSub_context, 8, TRUE, &mii );

	// EXTENDED INFO CONTEXT MENU (for right click)
	mii.dwTypeData = "Copy Selected";
	mii.cch = 13;
	mii.wID = MENU_COPY_SEL;
	InsertMenuItemA( g_hMenuSub_ei_context, 0, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( g_hMenuSub_ei_context, 1, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "Select All";
	mii.cch = 10;
	mii.wID = MENU_SELECT_ALL;
	InsertMenuItemA( g_hMenuSub_ei_context, 2, TRUE, &mii );

	mii.fType = MFT_SEPARATOR;
	InsertMenuItemA( g_hMenuSub_ei_context, 3, TRUE, &mii );

	mii.fType = MFT_STRING;
	mii.dwTypeData = "View Property Value";
	mii.cch = 19;
	mii.wID = MENU_PROP_VALUE;
	InsertMenuItemA( g_hMenuSub_ei_context, 4, TRUE, &mii );
}

// Enable/Disable the appropriate menu items depending on how many items exist as well as selected.
void UpdateMenus( unsigned char action )
{
	if ( action == UM_DISABLE || action == UM_DISABLE_OVERRIDE )
	{
		if ( action != UM_DISABLE_OVERRIDE )
		{
			EnableMenuItem( g_hMenu, MENU_OPEN, MF_DISABLED );
			EnableMenuItem( g_hMenu, MENU_HIDE_BLANK, MF_DISABLED );
		}

		EnableMenuItem( g_hMenu, MENU_SAVE_ALL, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_SAVE_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_COPY_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_EXPORT, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_REMOVE_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_CHECKSUMS, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_INFO, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_SCAN, MF_DISABLED );
		EnableMenuItem( g_hMenuSub_context, MENU_SAVE_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenuSub_context, MENU_COPY_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenuSub_context, MENU_REMOVE_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_DISABLED );
		EnableMenuItem( g_hMenuSub_context, MENU_INFO, MF_DISABLED );
	}
	else
	{
		int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );
		int sel_count = SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 );

		long type = ( item_count > 0 ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( g_hMenu, MENU_CHECKSUMS, type );
		EnableMenuItem( g_hMenu, MENU_SCAN, type );
		EnableMenuItem( g_hMenu, MENU_SAVE_ALL, type );
		EnableMenuItem( g_hMenu, MENU_EXPORT, type );

		type = ( sel_count > 0 ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( g_hMenu, MENU_SAVE_SEL, type );
		EnableMenuItem( g_hMenu, MENU_COPY_SEL, type );
		EnableMenuItem( g_hMenu, MENU_REMOVE_SEL, type );
		EnableMenuItem( g_hMenu, MENU_INFO, type );
		EnableMenuItem( g_hMenuSub_context, MENU_SAVE_SEL, type );
		EnableMenuItem( g_hMenuSub_context, MENU_COPY_SEL, type );
		EnableMenuItem( g_hMenuSub_context, MENU_REMOVE_SEL, type );
		EnableMenuItem( g_hMenuSub_context, MENU_INFO, type );

		type = ( sel_count != item_count ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( g_hMenu, MENU_SELECT_ALL, type );
		EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, type );

		EnableMenuItem( g_hMenu, MENU_OPEN, MF_ENABLED );
		EnableMenuItem( g_hMenu, MENU_HIDE_BLANK, MF_ENABLED );
	}
}
