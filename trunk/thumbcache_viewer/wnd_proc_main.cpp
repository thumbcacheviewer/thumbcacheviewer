/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2013 Eric Kutcher

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

#include "globals.h"
#include <stdio.h>

#define NUM_COLUMNS 9

WNDPROC ListViewProc = NULL;		// Subclassed listview window.
WNDPROC EditProc = NULL;			// Subclassed listview edit window.

// Function prototypes.
LRESULT CALLBACK ListViewSubProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK EditSubProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// Object variables
HWND g_hWnd_list = NULL;			// Handle to the listview control.
HWND g_hWnd_edit = NULL;			// Handle to the listview edit control.

HMENU g_hMenu = NULL;				// Handle to our menu bar.
HMENU g_hMenuSub_context = NULL;	// Handle to our context menu.

// Window variables
int cx = 0;							// Current x (left) position of the main window based on the mouse.
int cy = 0;							// Current y (top) position of the main window based on the mouse.

RECT last_pos = { 0 };				// The last position of the image window.

RECT current_edit_pos = { 0 };		// Current position of the listview edit control.

bool hide_blank_entries = false;	// Toggled from the main menu. Hides the blank (0 byte) entries.

bool is_kbytes_size = true;			// Toggle the size text.
bool is_kbytes_offset = true;		// Toggle the database location text.
bool is_dc_lower = true;			// Toggle the data checksum text
bool is_hc_lower = true;			// Toggle the header checksum text.
bool is_eh_lower = true;			// Toggle the entry hash text.

bool is_attached = false;			// Toggled when our windows are attached.
bool skip_main = false;				// Prevents the main window from moving the image window if it is about to attach.

bool first_show = false;			// Show the image window for the first time.

RECT last_dim = { 0 };				// Keeps track of the image window's dimension before it gets minimized.

HCURSOR wait_cursor = NULL;			// Temporary cursor while processing entries.

// Image variables
fileinfo *current_fileinfo = NULL;	// Holds information about the currently selected image. Gets deleted in WM_DESTROY.
Gdiplus::Image *gdi_image = NULL;	// GDI+ image object. We need it to handle .png and .jpg images.

bool is_close( int a, int b )
{
	// See if the distance between two points is less than the snap width.
	return abs( a - b ) < SNAP_WIDTH;
}

// Enable/Disable the appropriate menu items depending on how many items exist as well as selected.
void update_menus( bool disable_all )
{
	if ( disable_all == true )
	{
		EnableMenuItem( g_hMenu, MENU_OPEN, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_SAVE_ALL, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_SAVE_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_REMOVE_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_HIDE_BLANK, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_CHECKSUMS, MF_DISABLED );
		EnableMenuItem( g_hMenu, MENU_SCAN, MF_DISABLED );
		EnableMenuItem( g_hMenuSub_context, MENU_SAVE_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenuSub_context, MENU_REMOVE_SEL, MF_DISABLED );
		EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_DISABLED );
	}
	else
	{
		int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );
		int sel_count = SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 );

		if ( item_count > 0 )
		{
			EnableMenuItem( g_hMenu, MENU_CHECKSUMS, MF_ENABLED );
			EnableMenuItem( g_hMenu, MENU_SCAN, MF_ENABLED );
			EnableMenuItem( g_hMenu, MENU_SAVE_ALL, MF_ENABLED );
		}
		else
		{
			EnableMenuItem( g_hMenu, MENU_CHECKSUMS, MF_DISABLED );
			EnableMenuItem( g_hMenu, MENU_SCAN, MF_DISABLED );
			EnableMenuItem( g_hMenu, MENU_SAVE_ALL, MF_DISABLED );
		}

		if ( sel_count > 0 )
		{
			EnableMenuItem( g_hMenu, MENU_SAVE_SEL, MF_ENABLED );
			EnableMenuItem( g_hMenu, MENU_REMOVE_SEL, MF_ENABLED );
			EnableMenuItem( g_hMenuSub_context, MENU_SAVE_SEL, MF_ENABLED );
			EnableMenuItem( g_hMenuSub_context, MENU_REMOVE_SEL, MF_ENABLED );
		}
		else
		{
			EnableMenuItem( g_hMenu, MENU_SAVE_SEL, MF_DISABLED );
			EnableMenuItem( g_hMenu, MENU_REMOVE_SEL, MF_DISABLED );
			EnableMenuItem( g_hMenuSub_context, MENU_SAVE_SEL, MF_DISABLED );
			EnableMenuItem( g_hMenuSub_context, MENU_REMOVE_SEL, MF_DISABLED );
		}

		if ( sel_count != item_count )
		{
			EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_ENABLED );
			EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_ENABLED );
		}
		else
		{
			EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_DISABLED );
			EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_DISABLED );
		}

		EnableMenuItem( g_hMenu, MENU_OPEN, MF_ENABLED );
		EnableMenuItem( g_hMenu, MENU_HIDE_BLANK, MF_ENABLED );
	}
}

int CALLBACK CompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	fileinfo *fi1 = ( ( fileinfo * )lParam1 );
	fileinfo *fi2 = ( ( fileinfo * )lParam2 );

	// We added NUM_COLUMNS to the lParamSort value in order to distinguish between items we want to sort up, and items we want to sort down.
	// Saves us from having to pass some arbitrary struct pointer.
	if ( lParamSort >= NUM_COLUMNS )	// Up
	{
		switch ( lParamSort % NUM_COLUMNS )
		{
			case 1:
			{
				return _wcsicmp( fi1->filename, fi2->filename );
			}
			break;

			case 2:
			{
				return ( fi1->size > fi2->size );
			}
			break;

			case 3:
			{
				return ( fi1->data_offset > fi2->data_offset );
			}
			break;

			case 4:
			{
				if ( fi1->v_data_checksum != fi1->data_checksum && fi2->v_data_checksum == fi2->data_checksum )
				{
					return 0;
				}
				else if ( fi1->v_data_checksum == fi1->data_checksum && fi2->v_data_checksum != fi2->data_checksum )
				{
					return 1;
				}

				return ( fi1->data_checksum > fi2->data_checksum );
			}
			break;

			case 5:
			{
				if ( fi1->v_header_checksum != fi1->header_checksum && fi2->v_header_checksum == fi2->header_checksum )
				{
					return 0;
				}
				else if ( fi1->v_header_checksum == fi1->header_checksum && fi2->v_header_checksum != fi2->header_checksum )
				{
					return 1;
				}

				return ( fi1->header_checksum > fi2->header_checksum );
			}
			break;

			case 6:
			{
				return ( fi1->entry_hash > fi2->entry_hash );
			}
			break;

			case 7:
			{
				return ( fi1->si->system < fi2->si->system );	// 7 before Vista when sorting up.
			}
			break;

			case 8:
			{
				return _wcsicmp( fi1->si->dbpath, fi2->si->dbpath );
			}
			break;

			default:
			{
				return 0;
			}
			break;
		}	
	}
	else	// Down
	{
		switch ( lParamSort )
		{
			case 1:
			{
				return _wcsicmp( fi2->filename, fi1->filename );
			}
			break;

			case 2:
			{
				return ( fi2->size > fi1->size );
			}
			break;

			case 3:
			{
				return ( fi2->data_offset > fi1->data_offset );
			}
			break;

			case 4:
			{
				if ( fi1->v_data_checksum != fi1->data_checksum && fi2->v_data_checksum == fi2->data_checksum )
				{
					return 1;
				}
				else if ( fi1->v_data_checksum == fi1->data_checksum && fi2->v_data_checksum != fi2->data_checksum )
				{
					return 0;
				}

				return ( fi2->data_checksum > fi1->data_checksum );
			}
			break;

			case 5:
			{
				if ( fi1->v_header_checksum != fi1->header_checksum && fi2->v_header_checksum == fi2->header_checksum )
				{
					return 1;
				}
				else if ( fi1->v_header_checksum == fi1->header_checksum && fi2->v_header_checksum != fi2->header_checksum )
				{
					return 0;
				}

				return ( fi2->header_checksum > fi1->header_checksum );
			}
			break;

			case 6:
			{
				return ( fi2->entry_hash > fi1->entry_hash );
			}
			break;

			case 7:
			{
				return ( fi2->si->system < fi1->si->system ); // Vista before 7 when sorting down.
			}
			break;

			case 8:
			{
				return _wcsicmp( fi2->si->dbpath, fi1->si->dbpath );
			}
			break;

			default:
			{
				return 0;
			}
			break;
		}
	}
}

LRESULT CALLBACK MainWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch ( msg )
    {
		case WM_CREATE:
		{
			// Create our menu objects.
			g_hMenu = CreateMenu();
			HMENU hMenuSub_file = CreatePopupMenu();
			HMENU hMenuSub_edit = CreatePopupMenu();
			HMENU hMenuSub_view = CreatePopupMenu();
			HMENU hMenuSub_tools = CreatePopupMenu();
			HMENU hMenuSub_help = CreatePopupMenu();
			g_hMenuSub_context = CreatePopupMenu();

			// FILE MENU
			MENUITEMINFO mii = { NULL };
			mii.cbSize = sizeof( MENUITEMINFO );
			mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
			mii.fType = MFT_STRING;
			mii.dwTypeData = L"&Open...\tCtrl+O";
			mii.cch = 15;
			mii.wID = MENU_OPEN;
			InsertMenuItem( hMenuSub_file, 0, TRUE, &mii );

			mii.fType = MFT_SEPARATOR;
			InsertMenuItem( hMenuSub_file, 1, TRUE, &mii );

			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Save All...\tCtrl+S";
			mii.cch = 18;
			mii.wID = MENU_SAVE_ALL;
			mii.fState = MFS_DISABLED;
			InsertMenuItem( hMenuSub_file, 2, TRUE, &mii );
			mii.dwTypeData = L"Save Selected...\tCtrl+Shift+S";
			mii.cch = 29;
			mii.wID = MENU_SAVE_SEL;
			InsertMenuItem( hMenuSub_file, 3, TRUE, &mii );

			mii.fType = MFT_SEPARATOR;
			InsertMenuItem( hMenuSub_file, 4, TRUE, &mii );

			mii.fType = MFT_STRING;
			mii.dwTypeData = L"E&xit";
			mii.cch = 5;
			mii.wID = MENU_EXIT;
			mii.fState = MFS_ENABLED;
			InsertMenuItem( hMenuSub_file, 5, TRUE, &mii );

			// EDIT MENU
			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Remove Selected\tCtrl+R";
			mii.cch = 22;
			mii.wID = MENU_REMOVE_SEL;
			mii.fState = MFS_DISABLED;
			InsertMenuItem( hMenuSub_edit, 0, TRUE, &mii );

			mii.fType = MFT_SEPARATOR;
			InsertMenuItem( hMenuSub_edit, 1, TRUE, &mii );

			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Select All\tCtrl+A";
			mii.cch = 17;
			mii.wID = MENU_SELECT_ALL;
			InsertMenuItem( hMenuSub_edit, 2, TRUE, &mii );

			// VIEW MENU
			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Hide Blank Entries\tCtrl+H";
			mii.cch = 25;
			mii.wID = MENU_HIDE_BLANK;
			mii.fState = MFS_ENABLED;
			InsertMenuItem( hMenuSub_view, 0, TRUE, &mii );

			// TOOLS MENU
			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Verify Checksums\tCtrl+V";
			mii.cch = 23;
			mii.wID = MENU_CHECKSUMS;
			mii.fState = MFS_DISABLED;
			InsertMenuItem( hMenuSub_tools, 0, TRUE, &mii );

			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Map File Paths...\tCtrl+M";
			mii.cch = 24;
			mii.wID = MENU_SCAN;
			InsertMenuItem( hMenuSub_tools, 1, TRUE, &mii );

			// HELP MENU
			mii.dwTypeData = L"&About";
			mii.cch = 6;
			mii.wID = MENU_ABOUT;
			mii.fState = MFS_ENABLED;
			InsertMenuItem( hMenuSub_help, 0, TRUE, &mii );

			// MENU BAR
			mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
			mii.dwTypeData = L"&File";
			mii.cch = 5;
			mii.hSubMenu = hMenuSub_file;
			InsertMenuItem( g_hMenu, 0, TRUE, &mii );

			mii.dwTypeData = L"&Edit";
			mii.cch = 5;
			mii.hSubMenu = hMenuSub_edit;
			InsertMenuItem( g_hMenu, 1, TRUE, &mii );

			mii.dwTypeData = L"&View";
			mii.cch = 5;
			mii.hSubMenu = hMenuSub_view;
			InsertMenuItem( g_hMenu, 2, TRUE, &mii );

			mii.dwTypeData = L"&Tools";
			mii.cch = 6;
			mii.hSubMenu = hMenuSub_tools;
			InsertMenuItem( g_hMenu, 3, TRUE, &mii );

			mii.dwTypeData = L"&Help";
			mii.cch = 5;
			mii.hSubMenu = hMenuSub_help;
			InsertMenuItem( g_hMenu, 4, TRUE, &mii );

			// Set our menu bar.
			SetMenu( hWnd, g_hMenu );

			// CONTEXT MENU (for right click)
			mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
			mii.fState = MFS_DISABLED;
			mii.dwTypeData = L"Save Selected...";
			mii.cch = 16;
			mii.wID = MENU_SAVE_SEL;
			InsertMenuItem( g_hMenuSub_context, 0, TRUE, &mii );

			mii.fType = MFT_SEPARATOR;
			InsertMenuItem( g_hMenuSub_context, 1, TRUE, &mii );

			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Remove Selected";
			mii.cch = 15;
			mii.wID = MENU_REMOVE_SEL;
			InsertMenuItem( g_hMenuSub_context, 2, TRUE, &mii );

			mii.fType = MFT_SEPARATOR;
			InsertMenuItem( g_hMenuSub_context, 3, TRUE, &mii );

			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Select All";
			mii.cch = 10;
			mii.wID = MENU_SELECT_ALL;
			InsertMenuItem( g_hMenuSub_context, 4, TRUE, &mii );

			// Create our listview window.
			g_hWnd_list = CreateWindow( WC_LISTVIEW, NULL, LVS_REPORT | LVS_EDITLABELS | LVS_OWNERDRAWFIXED | WS_CHILDWINDOW | WS_VISIBLE, 0, 0, MIN_WIDTH, MIN_HEIGHT, hWnd, NULL, NULL, NULL );
			SendMessage( g_hWnd_list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES );

			// Allow drag and drop for the listview.
			DragAcceptFiles( g_hWnd_list, TRUE );

			// Subclass our listview to receive WM_DROPFILES.
			ListViewProc = ( WNDPROC )GetWindowLong( g_hWnd_list, GWL_WNDPROC );
			SetWindowLong( g_hWnd_list, GWL_WNDPROC, ( LONG )ListViewSubProc );

			// Initliaze our listview columns
			LVCOLUMN lvc = { NULL }; 
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT; 
			lvc.fmt = LVCFMT_CENTER;
			lvc.pszText = L"#";
			lvc.cx = 34;
			SendMessage( g_hWnd_list, LVM_INSERTCOLUMN, 0, ( LPARAM )&lvc );

			lvc.fmt = LVCFMT_LEFT;
			lvc.pszText = L"Filename";
			lvc.cx = 135;
			SendMessage( g_hWnd_list, LVM_INSERTCOLUMN, 1, ( LPARAM )&lvc );

			lvc.fmt = LVCFMT_RIGHT;
			lvc.pszText = L"Size";
			lvc.cx = 65;
			SendMessage( g_hWnd_list, LVM_INSERTCOLUMN, 2, ( LPARAM )&lvc );

			lvc.pszText = L"Entry Location";
			lvc.cx = 88;
			SendMessage( g_hWnd_list, LVM_INSERTCOLUMN, 3, ( LPARAM )&lvc );

			lvc.fmt = LVCFMT_LEFT;
			lvc.pszText = L"Data Checksum";
			lvc.cx = 125;
			SendMessage( g_hWnd_list, LVM_INSERTCOLUMN, 4, ( LPARAM )&lvc );

			lvc.pszText = L"Header Checksum";
			lvc.cx = 125;
			SendMessage( g_hWnd_list, LVM_INSERTCOLUMN, 5, ( LPARAM )&lvc );

			lvc.pszText = L"Entry Hash";
			lvc.cx = 125;
			SendMessage( g_hWnd_list, LVM_INSERTCOLUMN, 6, ( LPARAM )&lvc );

			lvc.pszText = L"System";
			lvc.cx = 85;
			SendMessage( g_hWnd_list, LVM_INSERTCOLUMN, 7, ( LPARAM )&lvc );

			lvc.pszText = L"Location";
			lvc.cx = 200;
			SendMessage( g_hWnd_list, LVM_INSERTCOLUMN, 8, ( LPARAM )&lvc );

			// Save our initial window position.
			GetWindowRect( hWnd, &last_pos );

			return 0;
		}
		break;

		case WM_KEYDOWN:
		{
			// We'll just give the listview control focus since it's handling our keypress events.
			SetFocus( g_hWnd_list );

			return 0;
		}
		break;

		case WM_MOVING:
		{
			POINT cur_pos;
			RECT wa;
			RECT *rc = ( RECT * )lParam;
			GetCursorPos( &cur_pos );
			OffsetRect( rc, cur_pos.x - ( rc->left + cx ), cur_pos.y - ( rc->top + cy ) );

			// Allow our main window to attach to the desktop edge.
			SystemParametersInfo( SPI_GETWORKAREA, 0, &wa, 0 );			
			if( is_close( rc->left, wa.left ) )				// Attach to left side of the desktop.
			{
				OffsetRect( rc, wa.left - rc->left, 0 );
			}
			else if ( is_close( wa.right, rc->right ) )		// Attach to right side of the desktop.
			{
				OffsetRect( rc, wa.right - rc->right, 0 );
			}

			if( is_close( rc->top, wa.top ) )				// Attach to top of the desktop.
			{
				OffsetRect( rc, 0, wa.top - rc->top );
			}
			else if ( is_close( wa.bottom, rc->bottom ) )	// Attach to bottom of the desktop.
			{
				OffsetRect( rc, 0, wa.bottom - rc->bottom );
			}

			// Allow our main window to attach to the image window.
			GetWindowRect( g_hWnd_image, &wa );
			if ( is_attached == false && IsWindowVisible( g_hWnd_image ) )
			{
				if( is_close( rc->right, wa.left ) )			// Attach to left side of image window.
				{
					// Allow it to snap only to the dimensions of the image window.
					if ( ( rc->bottom > wa.top ) && ( rc->top < wa.bottom ) )
					{
						OffsetRect( rc, wa.left - rc->right, 0 );
						is_attached = true;
					}
				}
				else if ( is_close( wa.right, rc->left ) )		// Attach to right side of image window.
				{
					// Allow it to snap only to the dimensions of the image window.
					if ( ( rc->bottom > wa.top ) && ( rc->top < wa.bottom ) )
					{
						OffsetRect( rc, wa.right - rc->left, 0 );
						is_attached = true;
					}
				}

				if( is_close( rc->bottom, wa.top ) )			// Attach to top of image window.
				{
					// Allow it to snap only to the dimensions of the image window.
					if ( ( rc->left < wa.right ) && ( rc->right > wa.left ) )
					{
						OffsetRect( rc, 0, wa.top - rc->bottom );
						is_attached = true;
					}
				}
				else if ( is_close( wa.bottom, rc->top ) )		// Attach to bottom of image window.
				{
					// Allow it to snap only to the dimensions of the image window.
					if ( ( rc->left < wa.right ) && ( rc->right > wa.left ) )
					{
						OffsetRect( rc, 0, wa.bottom - rc->top );
						is_attached = true;
					}
				}
			}

			// See if our image window is visible
			if ( IsWindowVisible( g_hWnd_image ) )
			{
				// If it's attached, then move it in proportion to our main window.
				if ( is_attached == true )
				{
					// If our main window attached itself to the image window, then we'll skip moving the image window.
					if ( skip_main == true )
					{
						// Moves the image window with the main window.
						MoveWindow( g_hWnd_image, wa.left + ( rc->left - last_pos.left ), wa.top + ( rc->top - last_pos.top ), wa.right - wa.left, wa.bottom - wa.top, FALSE );
					}
					else
					{
						// This causes the image window to snap to the main window. Kinda like a magnet. This is how they work by the way.
						MoveWindow( hWnd, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, FALSE );
					}
					
					skip_main = true;
				}
			}

			// Save our last position.
			last_pos.bottom = rc->bottom;
			last_pos.left = rc->left;
			last_pos.right = rc->right;
			last_pos.top = rc->top;

			return TRUE;
		}
		break;

		case WM_ENTERSIZEMOVE:
		{
			//Get the current position of our window before it gets moved.
			POINT cur_pos;
			RECT rc;
			GetWindowRect( hWnd, &rc );
			GetCursorPos( &cur_pos );
			cx = cur_pos.x - rc.left;
			cy = cur_pos.y - rc.top;

			return 0;
		}
		break;

		case WM_SIZE:
		{
			// If our window changes size, assume we aren't attached anymore.
			is_attached = false;
			skip_main = false;

			RECT rc = { 0 };
			GetClientRect( hWnd, &rc );

			// Allow our listview to resize in proportion to the main window.
			HDWP hdwp = BeginDeferWindowPos( 1 );
			DeferWindowPos( hdwp, g_hWnd_list, HWND_TOP, 0, 0, rc.right, rc.bottom, 0 );
			EndDeferWindowPos( hdwp );

			return 0;
		}
		break;

		case WM_MEASUREITEM:
		{
			// Set the row height of the list view.
			if ( ( ( LPMEASUREITEMSTRUCT )lParam )->CtlType = ODT_LISTVIEW )
			{
				( ( LPMEASUREITEMSTRUCT )lParam )->itemHeight = GetSystemMetrics( SM_CYSMICON ) + 2;
			}
			return TRUE;
		}
		break;

		case WM_GETMINMAXINFO:
		{
			// Set the minimum dimensions that the window can be sized to.
			( ( MINMAXINFO * )lParam )->ptMinTrackSize.x = MIN_WIDTH;
			( ( MINMAXINFO * )lParam )->ptMinTrackSize.y = MIN_HEIGHT;
			
			return 0;
		}
		break;

		case WM_CHANGE_CURSOR:
		{
			// SetCursor must be called from the window thread.
			if ( wParam == TRUE )
			{
				wait_cursor = LoadCursor( NULL, IDC_APPSTARTING );	// Arrow + hourglass.
				SetCursor( wait_cursor );
			}
			else
			{
				SetCursor( LoadCursor( NULL, IDC_ARROW ) );	// Default arrow.
				wait_cursor = NULL;
			}
		}
		break;

		case WM_SETCURSOR:
		{
			if ( wait_cursor != NULL )
			{
				SetCursor( wait_cursor );	// Keep setting our cursor if it reverts back to the default.
				return TRUE;
			}

			DefWindowProc( hWnd, msg, wParam, lParam );
			return FALSE;
		}
		break;

		case WM_COMMAND:
		{
			// Check to see if our command is a menu item.
			if ( HIWORD( wParam ) == 0 )
			{
				// Get the id of the menu item.
				switch( LOWORD( wParam ) )
				{
					case MENU_OPEN:
					{
						pathinfo *pi = ( pathinfo * )malloc( sizeof( pathinfo ) );
						pi->filepath = ( wchar_t * )malloc( sizeof( wchar_t ) * MAX_PATH * MAX_PATH );
						wmemset( pi->filepath, 0, MAX_PATH * MAX_PATH );
						OPENFILENAME ofn = { NULL };
						ofn.lStructSize = sizeof( OPENFILENAME );
						ofn.lpstrFilter = L"Thumbcache Database Files (*.db)\0*.db\0Iconcache Database Files (*.db)\0*.db\0All Files (*.*)\0*.*\0";
						ofn.lpstrFile = pi->filepath;
						ofn.nMaxFile = MAX_PATH * MAX_PATH;
						ofn.lpstrTitle = L"Open a Thumbcache Database file";
						ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_READONLY;
						ofn.hwndOwner = hWnd;

						// Display the Open File dialog box
						if( GetOpenFileName( &ofn ) )
						{
							pi->offset = ofn.nFileOffset;
							pi->output_path = NULL;
							cmd_line = 0;

							// filepath will be freed in the thread.
							CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &read_database, ( void * )pi, 0, NULL ) );
						}
						else
						{
							free( pi->filepath );
							free( pi );
						}
					}
					break;

					case MENU_SAVE_ALL:
					case MENU_SAVE_SEL:
					{
						// Open a browse for folder dialog box.
						BROWSEINFO bi = { 0 };
						bi.hwndOwner = hWnd;
						if ( LOWORD( wParam ) == MENU_SAVE_ALL )
						{
							bi.lpszTitle = L"Select a location to save all the file(s).";
						}
						else
						{
							bi.lpszTitle = L"Select a location to save the selected file(s).";
						}
						bi.ulFlags = BIF_EDITBOX | BIF_VALIDATE;

						LPITEMIDLIST lpiidl = SHBrowseForFolder( &bi );
						if ( lpiidl )
						{
							save_param *save_type = ( save_param * )malloc( sizeof( save_param ) );	// Freed in the save_items thread.
							save_type->filepath = NULL;
							save_type->lpiidl = lpiidl;
							save_type->save_all = ( LOWORD( wParam ) == MENU_SAVE_ALL ? true : false );
							CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &save_items, ( void * )save_type, 0, NULL ) );
						}
					}
					break;

					case MENU_HIDE_BLANK:
					{
						hide_blank_entries = !hide_blank_entries;

						// Put a check next to the menu item if we choose to hide blank entries. Remove it if we don't.
						CheckMenuItem( g_hMenu, MENU_HIDE_BLANK, ( hide_blank_entries ? MF_CHECKED : MF_UNCHECKED ) );

						CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &show_hide_items, ( void * )NULL, 0, NULL ) );
					}
					break;

					case MENU_CHECKSUMS:
					{
						// Go through each item in the list and verify the header and data checksums.
						CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &verify_checksums, ( void * )NULL, 0, NULL ) );
					}
					break;

					case MENU_SCAN:
					{
						SendMessage( g_hWnd_scan, WM_PROPAGATE, 0, 0 );
					}
					break;

					case MENU_REMOVE_SEL:
					{
						// Hide the image window since the selected item will be deleted.
						if ( IsWindowVisible( g_hWnd_image ) )
						{
							ShowWindow( g_hWnd_image, SW_HIDE );
						}

						is_attached = false;
						skip_main = false;

						CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &remove_items, ( void * )NULL, 0, NULL ) );
					}
					break;

					case MENU_SELECT_ALL:
					{
						// Set the state of all items to selected.
						LVITEM lvi = { NULL };
						lvi.mask = LVIF_STATE;
						lvi.state = LVIS_SELECTED;
						lvi.stateMask = LVIS_SELECTED;
						SendMessage( g_hWnd_list, LVM_SETITEMSTATE, -1, ( LPARAM )&lvi );

						update_menus( false );
					}
					break;

					case MENU_ABOUT:
					{
						MessageBox( hWnd, L"Thumbcache Viewer is made free under the GPLv3 license.\n\nCopyright \xA9 2011-2013 Eric Kutcher", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONINFORMATION );
					}
					break;

					case MENU_EXIT:
					{
						SendMessage( hWnd, WM_CLOSE, 0, 0 );
					}
					break;
				}
			}
			return 0;
		}
		break;

		case WM_NOTIFY:
		{
			// Get our listview codes.
			switch ( ( ( LPNMHDR )lParam )->code )
			{
				case LVN_KEYDOWN:
				{
					// Make sure the control key is down and that we're not already in a worker thread. Prevents threads from queuing in case the user falls asleep on their keyboard.
					if ( GetKeyState( VK_CONTROL ) & 0x8000 && in_thread == false )
					{
						// Determine which key was pressed.
						switch ( ( ( LPNMLVKEYDOWN )lParam )->wVKey )
						{
							case 'A':	// Select all items if Ctrl + A is down and there are items in the list.
							{
								if ( SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) > 0 )
								{
									SendMessage( hWnd, WM_COMMAND, MENU_SELECT_ALL, 0 );
								}
							}
							break;

							case 'O':	// Open the file dialog box if Ctrl + O is down.
							{
								SendMessage( hWnd, WM_COMMAND, MENU_OPEN, 0 );
							}
							break;

							case 'H':	// Hide blank entries if Ctrl + H is down.
							{
								SendMessage( hWnd, WM_COMMAND, MENU_HIDE_BLANK, 0 );
							}
							break;

							case 'V':	// Verify checksums.
							{
								if ( SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) > 0 )
								{
									SendMessage( hWnd, WM_COMMAND, MENU_CHECKSUMS, 0 );
								}
							}
							break;

							case 'M':	// Map entry hash values to local files.
							{
								if ( SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) > 0 )
								{
									SendMessage( hWnd, WM_COMMAND, MENU_SCAN, 0 );
								}
							}
							break;

							case 'R':	// Remove selected items if Ctrl + R is down and there are selected items in the list.
							{
								if ( SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 ) > 0 )
								{
									SendMessage( hWnd, WM_COMMAND, MENU_REMOVE_SEL, 0 );
								}
							}
							break;

							case 'S':	// Save all/selected items if Ctrl + S or Ctrl + Shift + S is down and there are items in the list.
							{
								if ( SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) > 0 && !( GetKeyState( VK_SHIFT ) & 0x8000 ) )	// Shift not down.
								{
									SendMessage( hWnd, WM_COMMAND, MENU_SAVE_ALL, 0 );
								}
								else if ( SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 ) > 0 && ( GetKeyState( VK_SHIFT ) & 0x8000 ) ) // Shift down.
								{
									SendMessage( hWnd, WM_COMMAND, MENU_SAVE_SEL, 0 );
								}
							}
							break;
						}
					}
				}
				break;

				case LVN_COLUMNCLICK:
				{
					// Change the format of the items in the column if Ctrl is held while clicking the column.
					if ( GetKeyState( VK_CONTROL ) & 0x8000 )
					{
						switch ( ( ( NMLISTVIEW * )lParam )->iSubItem )
						{
							case 2:	// Change the size column info.
							{
								is_kbytes_size = !is_kbytes_size;
								InvalidateRect( g_hWnd_list, NULL, TRUE );
							}
							break;

							case 3:	// Change the database location column info.
							{
								is_kbytes_offset = !is_kbytes_offset;
								InvalidateRect( g_hWnd_list, NULL, TRUE );
							}
							break;

							case 4:	// Change the data checksum column info.
							{
								is_dc_lower = !is_dc_lower;
								InvalidateRect( g_hWnd_list, NULL, TRUE );
							}
							break;
							
							case 5:	// Change the header checksum column info.
							{
								is_hc_lower = !is_hc_lower;
								InvalidateRect( g_hWnd_list, NULL, TRUE );
							}
							break;

							case 6:	// Change the entry hash column info.
							{
								is_eh_lower = !is_eh_lower;
								InvalidateRect( g_hWnd_list, NULL, TRUE );
							}
							break;
						}
					}
					else	// Normal column click. Sort the items in the column.
					{
						LVCOLUMN lvc = { NULL };
						lvc.mask = LVCF_FMT;
						SendMessage( g_hWnd_list, LVM_GETCOLUMN, ( ( NMLISTVIEW * )lParam )->iSubItem, ( LPARAM )&lvc );
						
						if ( HDF_SORTUP & lvc.fmt )	// Column is sorted upward.
						{
							// Sort down
							lvc.fmt = lvc.fmt & ( ~HDF_SORTUP ) | HDF_SORTDOWN;
							SendMessage( g_hWnd_list, LVM_SETCOLUMN, ( WPARAM )( ( NMLISTVIEW * )lParam )->iSubItem, ( LPARAM )&lvc );

							SendMessage( g_hWnd_list, LVM_SORTITEMS, ( ( NMLISTVIEW * )lParam )->iSubItem, ( LPARAM )( PFNLVCOMPARE )CompareFunc );
						}
						else if ( HDF_SORTDOWN & lvc.fmt )	// Column is sorted downward.
						{
							// Sort up
							lvc.fmt = lvc.fmt & ( ~HDF_SORTDOWN ) | HDF_SORTUP;
							SendMessage( g_hWnd_list, LVM_SETCOLUMN, ( ( NMLISTVIEW * )lParam )->iSubItem, ( LPARAM )&lvc );

							SendMessage( g_hWnd_list, LVM_SORTITEMS, ( ( NMLISTVIEW * )lParam )->iSubItem + NUM_COLUMNS, ( LPARAM )( PFNLVCOMPARE )CompareFunc );
						}
						else	// Column has no sorting set.
						{
							// Remove the sort format for all columns.
							for ( int i = 0; i < NUM_COLUMNS; i++ )
							{
								// Get the current format
								SendMessage( g_hWnd_list, LVM_GETCOLUMN, i, ( LPARAM )&lvc );
								// Remove sort up and sort down
								lvc.fmt = lvc.fmt & ( ~HDF_SORTUP ) & ( ~HDF_SORTDOWN );
								SendMessage( g_hWnd_list, LVM_SETCOLUMN, i, ( LPARAM )&lvc );
							}

							// Read current the format from the clicked column
							SendMessage( g_hWnd_list, LVM_GETCOLUMN, ( ( NMLISTVIEW * )lParam )->iSubItem, ( LPARAM )&lvc );
							// Sort down to start.
							lvc.fmt = lvc.fmt | HDF_SORTDOWN;
							SendMessage( g_hWnd_list, LVM_SETCOLUMN, ( ( NMLISTVIEW * )lParam )->iSubItem, ( LPARAM )&lvc );

							SendMessage( g_hWnd_list, LVM_SORTITEMS, ( ( NMLISTVIEW * )lParam )->iSubItem, ( LPARAM )( PFNLVCOMPARE )CompareFunc );
						}
					}
				}
				break;

				case NM_RCLICK:
				{
					// Show our edit context menu as a popup.
					POINT p;
					GetCursorPos( &p ) ;
					TrackPopupMenu( g_hMenuSub_context, 0, p.x, p.y, 0, hWnd, NULL );
				}
				break;

				case LVN_DELETEITEM:
				{
					// Item count will be 1 since the item hasn't yet been deleted.
					if ( SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) == 1 )
					{
						// Disable the menus that require at least one item in the list.
						EnableMenuItem( g_hMenu, MENU_SAVE_ALL, MF_DISABLED );
						EnableMenuItem( g_hMenu, MENU_SAVE_SEL, MF_DISABLED );
						EnableMenuItem( g_hMenu, MENU_REMOVE_SEL, MF_DISABLED );
						EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_DISABLED );
						EnableMenuItem( g_hMenuSub_context, MENU_SAVE_SEL, MF_DISABLED );
						EnableMenuItem( g_hMenuSub_context, MENU_REMOVE_SEL, MF_DISABLED );
						EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_DISABLED );
					}
				}
				break;

				case LVN_ITEMCHANGED:
				{
					NMLISTVIEW *nmlv = ( NMLISTVIEW * )lParam;

					if ( in_thread == false )
					{
						update_menus( false );
					}

					// Only load images that are selected and in focus.
					if ( nmlv->uNewState != ( LVIS_FOCUSED | LVIS_SELECTED ) )
					{
						break;
					}
					
					// Retrieve the lParam value from the selected listview item.
					LVITEM lvi = { NULL };
					lvi.mask = LVIF_PARAM;
					lvi.iItem = nmlv->iItem;
					SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

					// Create a buffer to read in our new bitmap.
					char *current_image = ( char * )malloc( sizeof( char ) * ( ( fileinfo * )lvi.lParam )->size );

					// Attempt to open a file for reading.
					HANDLE hFile = CreateFile( ( ( fileinfo * )lvi.lParam )->si->dbpath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
					if ( hFile != INVALID_HANDLE_VALUE )
					{
						DWORD read = 0;
						// Set our file pointer to the beginning of the database file.
						SetFilePointer( hFile, ( ( fileinfo * )lvi.lParam )->data_offset, 0, FILE_BEGIN );
						// Read the entire image into memory.
						ReadFile( hFile, current_image, ( ( fileinfo * )lvi.lParam )->size, &read, NULL );
						CloseHandle( hFile );

						// If gdi_image exists, then delete it.
						if ( gdi_image != NULL )
						{
							delete gdi_image;
							gdi_image = NULL;
						}

						// Create a stream to store our buffer and then store the stream into a GDI+ image object.
						ULONG written = 0;
						IStream *is = NULL;
						CreateStreamOnHGlobal( NULL, TRUE, &is );
						is->Write( current_image, read, &written );	// Only write what we've read.
						gdi_image = new Gdiplus::Image( is );
						is->Release();

						if ( !IsWindowVisible( g_hWnd_image ) )
						{
							// Move our image window next to the main window on its right side if it's the first time we're showing the image window.
							if ( first_show == false )
							{
								SetWindowPos( g_hWnd_image, HWND_TOPMOST, last_pos.right, last_pos.top, MIN_HEIGHT, MIN_HEIGHT, SWP_NOACTIVATE );
								first_show = true;
							}

							// This is done to keep both windows on top of other windows.
							// Set the image window position on top of all windows except topmost windows, but don't set focus to it.
							SetWindowPos( g_hWnd_image, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );
							// Set our main window on top of the image window.
							SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
							// The image window is on top of everything (except the main window), set it back to non-topmost.
							SetWindowPos( g_hWnd_image, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
						}

						// Set our image window's icon to match the file extension.
						if ( ( ( fileinfo * )lvi.lParam )->flag & 1 )
						{
							SendMessage( g_hWnd_image, WM_SETICON, ICON_SMALL, ( LPARAM )hIcon_bmp );
						}
						else if ( ( ( fileinfo * )lvi.lParam )->flag & 2 )
						{
							SendMessage( g_hWnd_image, WM_SETICON, ICON_SMALL, ( LPARAM )hIcon_jpg );
						}
						else if ( ( ( fileinfo * )lvi.lParam )->flag & 4 )
						{
							SendMessage( g_hWnd_image, WM_SETICON, ICON_SMALL, ( LPARAM )hIcon_png );
						}
						else
						{
							SendMessage( g_hWnd_image, WM_SETICON, ICON_SMALL, NULL );
						}

						// Set the image window's new title.
						wchar_t new_title[ MAX_PATH + 30 ] = { 0 };
						swprintf_s( new_title, MAX_PATH + 30, L"%.259s - %dx%d", ( ( fileinfo * )lvi.lParam )->filename, gdi_image->GetWidth(), gdi_image->GetHeight() );
						SetWindowText( g_hWnd_image, new_title );

						// See if our image window is minimized and set the rectangle to its old size if it is.
						RECT rc = { 0 };
						if ( IsIconic( g_hWnd_image ) == TRUE )
						{
							rc = last_dim;
						}
						else // Otherwise, get the current size.
						{
							GetClientRect( g_hWnd_image, &rc );
						}

						old_pos.x = old_pos.y = 0;
						drag_rect.x = drag_rect.y = 0;

						// Center the image.
						drag_rect.x = ( ( long )gdi_image->GetWidth() - rc.right ) / 2;
						drag_rect.y = ( ( long )gdi_image->GetHeight() - rc.bottom ) / 2;

						scale = 1.0f;	// Reset the image scale.

						// Force our window to repaint itself.
						InvalidateRect( g_hWnd_image, NULL, TRUE );
					}

					// Free our image buffer.
					free( current_image );
				}
				break;

				case LVN_BEGINLABELEDIT:
				{
					NMLVDISPINFO *pdi = ( NMLVDISPINFO * )lParam;
					
					// If no item is being edited, then cancel the edit.
					if ( pdi->item.iItem == -1 )
					{
						return TRUE;
					}

					// Get the current list item text from its lParam.
					LVITEM lvi = { NULL };
					lvi.iItem = pdi->item.iItem;
					lvi.iSubItem = 1;
					lvi.mask = LVIF_PARAM;
					SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

					// Save our current fileinfo.
					current_fileinfo = ( fileinfo * )lvi.lParam;
					
					// Get the bounding box of the Filename column we're editing.
					current_edit_pos.top = 1;
					current_edit_pos.left = LVIR_BOUNDS;
					SendMessage( g_hWnd_list, LVM_GETSUBITEMRECT, pdi->item.iItem, ( LPARAM )&current_edit_pos );
					
					// Get the edit control that the listview creates.
					g_hWnd_edit = ( HWND )SendMessage( g_hWnd_list, LVM_GETEDITCONTROL, 0, 0 );

					// Subclass our edit window to modify its position.
					EditProc = ( WNDPROC )GetWindowLongPtr( g_hWnd_edit, GWL_WNDPROC );
					SetWindowLongPtr( g_hWnd_edit, GWL_WNDPROC, ( LONG )EditSubProc );

					// Set our edit control's text to the list item's text.
					SetWindowText( g_hWnd_edit, current_fileinfo->filename );

					// Get the length of the filename without the extension.
					int ext_len = wcslen( current_fileinfo->filename );
					while ( ext_len != 0 && current_fileinfo->filename[ --ext_len ] != L'.' );

					// Select all the text except the file extension (if ext_len = 0, then everything is selected)
					SendMessage( g_hWnd_edit, EM_SETSEL, 0, ext_len );

					// Allow the edit to proceed.
					return FALSE;
				}
				break;

				case LVN_ENDLABELEDIT:
				{
					NMLVDISPINFO *pdi = ( NMLVDISPINFO * )lParam;

					// Prevent the edit if there's no text.
					if ( pdi->item.pszText == NULL )
					{
						return FALSE;
					}
					// Prevent the edit if the text length is 0.
					unsigned int length = wcslen( pdi->item.pszText );
					if ( length == 0 )
					{
						return FALSE;
					}

					// Free the old filename.
					free( current_fileinfo->filename );
					// Create a new filename based on the editbox's text.
					wchar_t *filename = ( wchar_t * )malloc( sizeof( wchar_t ) * ( length + 1 ) );
					wmemset( filename, 0, length + 1 );
					wcscpy_s( filename, length + 1, pdi->item.pszText );

					// Modify our listview item's fileinfo lParam value.
					current_fileinfo->filename = filename;

					// Set the image window's new title.
					wchar_t new_title[ MAX_PATH + 30 ] = { 0 };
					swprintf_s( new_title, MAX_PATH + 30, L"%.259s - %dx%d", filename, gdi_image->GetWidth(), gdi_image->GetHeight() );
					SetWindowText( g_hWnd_image, new_title );

					return TRUE;
				}
				break;

			}
			return FALSE;
		}
		break;

		case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *dis = ( DRAWITEMSTRUCT * )lParam;
      
			// The item we want to draw is our listview.
			if ( dis->CtlType == ODT_LISTVIEW )
			{
				// Alternate item color's background.
				if ( dis->itemID % 2 )	// Even rows will have a light grey background.
				{
					HBRUSH color = CreateSolidBrush( ( COLORREF )RGB( 0xF7, 0xF7, 0xF7 )  );
					FillRect( dis->hDC, &dis->rcItem, color );
					DeleteObject( color );
				}

				// Set the selected item's color.
				bool selected = false;
				if ( dis->itemState & ( ODS_FOCUS || ODS_SELECTED ) )
				{
					if ( skip_draw == true )
					{
						return TRUE;	// Don't draw selected items because their lParam values are being deleted.
					}

					HBRUSH color = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_HOTLIGHT ) );
					FillRect( dis->hDC, &dis->rcItem, color );
					DeleteObject( color );
					selected = true;
				}

				// Get the item's text.
				wchar_t tbuf[ MAX_PATH ];
				wchar_t *buf = tbuf;
				LVITEM lvi = { 0 };
				lvi.mask = LVIF_PARAM;
				lvi.iItem = dis->itemID;
				SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );	// Get the lParam value from our item.

				// This is the full size of the row.
				RECT last_rc = { 0 };

				// This will keep track of the current colunn's left position.
				int last_left = 0;

				// Adjust the alignment position of the text.
				int RIGHT_COLUMNS = 0;

				LVCOLUMN lvc = { 0 };
				lvc.mask = LVCF_WIDTH;

				// Loop through all the columns
				for ( int i = 0; i < NUM_COLUMNS; ++i )
				{
					RIGHT_COLUMNS = 0;

					// Save the appropriate text in our buffer for the current column.
					switch ( i )
					{
						case 0:
						{
							swprintf_s( buf, MAX_PATH, L"%d", dis->itemID + 1 );
						}
						break;

						case 1:
						{
							buf = ( ( fileinfo * )lvi.lParam )->filename;
						}
						break;

						case 2:
						{
							buf = tbuf;	// Reset the buffer pointer.
							RIGHT_COLUMNS = DT_RIGHT;

							// Depending on our toggle, output the size in either kilobytes or bytes.
							if ( is_kbytes_size == true )
							{
								swprintf_s( buf, MAX_PATH, L"%d KB", ( ( fileinfo * )lvi.lParam )->size / 1024 );
							}
							else
							{
								swprintf_s( buf, MAX_PATH, L"%d B", ( ( fileinfo * )lvi.lParam )->size );
							}
						}
						break;

						case 3:
						{
							RIGHT_COLUMNS = DT_RIGHT;

							// Depending on our toggle, output the offset (db location) in either kilobytes or bytes.
							if ( is_kbytes_offset == true )
							{
								swprintf_s( buf, MAX_PATH, L"%d B", ( ( fileinfo * )lvi.lParam )->data_offset );
							}
							else
							{
								swprintf_s( buf, MAX_PATH, L"%d KB", ( ( fileinfo * )lvi.lParam )->data_offset / 1024 );
							}
						}
						break;

						case 4:
						{
							// Output the hex string in either lowercase or uppercase.
							if ( is_dc_lower == true )
							{
								int out = swprintf_s( buf, MAX_PATH, L"0x%016llx", ( ( fileinfo * )lvi.lParam )->data_checksum );

								if ( ( ( fileinfo * )lvi.lParam )->v_data_checksum != ( ( fileinfo * )lvi.lParam )->data_checksum )
								{
									swprintf_s( buf + out, MAX_PATH - out, L" : 0x%016llx", ( ( fileinfo * )lvi.lParam )->v_data_checksum );
								}
							}
							else
							{
								int out = swprintf_s( buf, MAX_PATH, L"0x%016llX", ( ( fileinfo * )lvi.lParam )->data_checksum );

								if ( ( ( fileinfo * )lvi.lParam )->v_data_checksum != ( ( fileinfo * )lvi.lParam )->data_checksum )
								{
									swprintf_s( buf + out, MAX_PATH - out, L" : 0x%016llX", ( ( fileinfo * )lvi.lParam )->v_data_checksum );
								}
							}
						}
						break;

						case 5:
						{
							// Output the hex string in either lowercase or uppercase.
							if ( is_hc_lower == true )
							{
								int out = swprintf_s( buf, MAX_PATH, L"0x%016llx", ( ( fileinfo * )lvi.lParam )->header_checksum );

								if ( ( ( fileinfo * )lvi.lParam )->v_header_checksum != ( ( fileinfo * )lvi.lParam )->header_checksum )
								{
									swprintf_s( buf + out, MAX_PATH - out, L" : 0x%016llx", ( ( fileinfo * )lvi.lParam )->v_header_checksum );
								}
							}
							else
							{
								int out = swprintf_s( buf, MAX_PATH, L"0x%016llX", ( ( fileinfo * )lvi.lParam )->header_checksum );

								if ( ( ( fileinfo * )lvi.lParam )->v_header_checksum != ( ( fileinfo * )lvi.lParam )->header_checksum )
								{
									swprintf_s( buf + out, MAX_PATH - out, L" : 0x%016llX", ( ( fileinfo * )lvi.lParam )->v_header_checksum );
								}
							}
						}
						break;

						case 6:
						{
							// Output the hex string in either lowercase or uppercase.
							if ( is_eh_lower == true )
							{
								swprintf_s( buf, MAX_PATH, L"0x%016llx", ( ( fileinfo * )lvi.lParam )->entry_hash );
							}
							else
							{
								swprintf_s( buf, MAX_PATH, L"0x%016llX", ( ( fileinfo * )lvi.lParam )->entry_hash );
							}
						}
						break;

						case 7:
						{
							if ( ( ( fileinfo * )lvi.lParam )->si->system == WINDOWS_7 )
							{
								wcscpy_s( buf, MAX_PATH, L"Windows 7" );
							}
							else if ( ( ( fileinfo * )lvi.lParam )->si->system == WINDOWS_8 || ( ( fileinfo * )lvi.lParam )->si->system == WINDOWS_8v2 || ( ( fileinfo * )lvi.lParam )->si->system == WINDOWS_8v3 )
							{
								wcscpy_s( buf, MAX_PATH, L"Windows 8" );
							}
							else if ( ( ( fileinfo * )lvi.lParam )->si->system == WINDOWS_8_1 )
							{
								wcscpy_s( buf, MAX_PATH, L"Windows 8.1" );
							}
							else
							{
								wcscpy_s( buf, MAX_PATH, L"Windows Vista" );
							}
						}
						break;

						case 8:
						{
							buf = ( ( fileinfo * )lvi.lParam )->si->dbpath;
						}
						break;
					}

					// Get the dimensions of the listview column
					SendMessage( g_hWnd_list, LVM_GETCOLUMN, i, ( LPARAM )&lvc );

					last_rc = dis->rcItem;

					// This will adjust the text to fit nicely into the rectangle.
					last_rc.left = 5 + last_left;
					last_rc.right = lvc.cx + last_left - 5;
					last_rc.top += 2;
					
					// Save the height and width of this region.
					int width = last_rc.right - last_rc.left;
					int height = last_rc.bottom - last_rc.top;

					// Normal text position.
					RECT rc = { 0 };
					rc.right = width;
					rc.bottom = height;

					// Shadow text position.
					RECT rc2 = rc;
					rc2.left += 1;
					rc2.top += 1;
					rc2.right += 1;
					rc2.bottom += 1;

					// Create and save a bitmap in memory to paint to.
					HDC hdcMem = CreateCompatibleDC( dis->hDC );
					HBITMAP hbm = CreateCompatibleBitmap( dis->hDC, width, height );
					HBITMAP ohbm = ( HBITMAP )SelectObject( hdcMem, hbm );
					DeleteObject( ohbm );
					DeleteObject( hbm );
					HFONT ohf = ( HFONT )SelectObject( hdcMem, hFont );
					DeleteObject( ohf );

					// Transparent background for text.
					SetBkMode( hdcMem, TRANSPARENT );

					// Draw selected text
					if ( selected == true )
					{
						// Fill the background.
						HBRUSH color = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_HOTLIGHT ) );
						FillRect( hdcMem, &rc, color );
						DeleteObject( color );

						// Shadow color - black.
						SetTextColor( hdcMem, RGB( 0x00, 0x00, 0x00 ) );
						DrawText( hdcMem, buf, -1, &rc2, DT_SINGLELINE | DT_END_ELLIPSIS | RIGHT_COLUMNS );
						// White text.
						SetTextColor( hdcMem, RGB( 0xFF, 0xFF, 0xFF ) );
						DrawText( hdcMem, buf, -1, &rc, DT_SINGLELINE | DT_END_ELLIPSIS | RIGHT_COLUMNS );
						BitBlt( dis->hDC, dis->rcItem.left + last_rc.left, last_rc.top, width, height, hdcMem, 0, 0, SRCCOPY );
					}
					else	// Draw normal text.
					{
						// Fill the background.
						HBRUSH color = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_WINDOW ) );
						FillRect( hdcMem, &rc, color );
						DeleteObject( color );

						// Shadow color - light grey.
						SetTextColor( hdcMem, RGB( 0xE0, 0xE0, 0xE0 ) );
						DrawText( hdcMem, buf, -1, &rc2, DT_SINGLELINE | DT_END_ELLIPSIS | RIGHT_COLUMNS );

						// Show red text if our checksums don't match and black for everything else.
						SetTextColor( hdcMem, RGB( ( ( ( i == 4 && ( ( ( fileinfo * )lvi.lParam )->v_data_checksum != ( ( fileinfo * )lvi.lParam )->data_checksum ) ) || ( i == 5 && ( ( ( fileinfo * )lvi.lParam )->v_header_checksum != ( ( fileinfo * )lvi.lParam )->header_checksum ) ) ) ? 0xFF : 0x00 ), 0x00, 0x00 ) );

						DrawText( hdcMem, buf, -1, &rc, DT_SINGLELINE | DT_END_ELLIPSIS | RIGHT_COLUMNS );
						BitBlt( dis->hDC, dis->rcItem.left + last_rc.left, last_rc.top, width, height, hdcMem, 0, 0, SRCAND );
					}

					// Delete our back buffer.
					DeleteDC( hdcMem );

					// Save the last left position of our column.
					last_left += lvc.cx;
				}
			}
			return TRUE;
		}
		break;

		case WM_CLOSE:
		{
			// Prevent the possibility of running additional processes.
			EnableWindow( hWnd, FALSE );
			ShowWindow( hWnd, SW_HIDE );
			ShowWindow( g_hWnd_image, SW_HIDE );

			// If we're in a secondary thread, then kill it (cleanly) and wait for it to exit.
			if ( in_thread == true )
			{
				CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &cleanup, ( void * )NULL, 0, NULL ) );
			}
			else	// Otherwise, destroy the window normally.
			{
				kill_thread = true;
				DestroyWindow( hWnd );
			}

			return 0;
		}
		break;

		case WM_DESTROY_ALT:
		{
			DestroyWindow( hWnd );
		}
		break;

		case WM_DESTROY:
		{
			// Get the number of items in the listview.
			int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

			LVITEM lvi = { NULL };
			lvi.mask = LVIF_PARAM;

			// Go through each item, and free their lParam values. current_fileinfo will get deleted here.
			for ( int i = 0; i < item_count; ++i )
			{
				lvi.iItem = i;
				SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

				( ( fileinfo * )lvi.lParam )->si->count--;

				// Remove our shared information from the linked list if there's no more items for this database.
				if ( ( ( fileinfo * )lvi.lParam )->si->count == 0 )
				{
					free( ( ( fileinfo * )lvi.lParam )->si );
				}

				// First free the filename pointer.
				free( ( ( fileinfo * )lvi.lParam )->filename );
				// Then free the fileinfo structure.
				free( ( fileinfo * )lvi.lParam );
			}

			// Delete our image object.
			if ( gdi_image != NULL )
			{
				delete gdi_image;
			}

			// Go through the list of blank entries and free any shared info and fileinfo structures.
			blank_entries_linked_list *be = g_be;
			blank_entries_linked_list *del_be = NULL;
			while ( be != NULL )
			{
				del_be = be;
				be = be->next;

				del_be->fi->si->count--;

				if ( del_be->fi->si->count == 0 )
				{
					free( del_be->fi->si );
				}
				free( del_be->fi->filename );
				free( del_be->fi );
				free( del_be );
			}

			// Clean up our fileinfo tree.
			rbt_delete( fileinfo_tree );

			// Since this isn't owned by a window, we need to destroy it.
			DestroyMenu( g_hMenuSub_context );

			PostQuitMessage( 0 );
			return 0;
		}
		break;

		default:
		{
			return DefWindowProc( hWnd, msg, wParam, lParam );
		}
		break;
	}
	return TRUE;
}

LRESULT CALLBACK ListViewSubProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch( msg )
	{
		// This will essentially reconstruct the string that the open file dialog box creates when selecting multiple files.
		case WM_DROPFILES:
		{
			int count = DragQueryFile( ( HDROP )wParam, -1, NULL, 0 );

			pathinfo *pi = ( pathinfo * )malloc( sizeof( pathinfo ) );
			pi->filepath = NULL;
			pi->offset = 0;
			pi->output_path = NULL;
			cmd_line = 0;

			int file_offset = 0;	// Keeps track of the last file in filepath.

			// Go through the list of paths.
			for ( int i = 0; i < count; i++ )
			{
				// Get the length of the file path.
				int file_path_length = DragQueryFile( ( HDROP )wParam, i, NULL, 0 );

				// Get the file path.
				wchar_t *fpath = ( wchar_t * )malloc( sizeof( wchar_t ) * ( file_path_length + 1 ) );
				DragQueryFile( ( HDROP )wParam, i, fpath, file_path_length + 1 );

				// Skip any folders that were dropped.
				if ( ( GetFileAttributes( fpath ) & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
				{
					free( fpath );
					continue;
				}

				// Copy the root directory into filepath.
				if ( pi->filepath == NULL )
				{
					pi->filepath = ( wchar_t * )malloc( sizeof( wchar_t ) * ( ( MAX_PATH * count ) + 1 ) );
					pi->offset = file_path_length;
					// Find the last occurance of "\" in the string.
					while ( pi->offset != 0 && fpath[ --pi->offset ] != L'\\' );

					// Save the root directory name.
					wcsncpy_s( pi->filepath, pi->offset + 1, fpath, pi->offset );

					file_offset = ( ++pi->offset );
				}

				// Copy the file name. Each is separated by the NULL character.
				wcscpy_s( pi->filepath + file_offset, file_path_length - pi->offset + 1, fpath + pi->offset );
				file_offset += ( file_path_length - pi->offset + 1 );

				free( fpath );
			}

			DragFinish( ( HDROP )wParam );

			if ( pi->filepath != NULL )
			{
				// Terminate the last concatenated string.
				wmemset( pi->filepath + file_offset, L'\0', 1 );

				// filepath will be freed in the thread.
				CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &read_database, ( void * )pi, 0, NULL ) );
			}
			else	// No files were dropped.
			{
				free( pi );
			}

			return 0;
		}
		break;
	}

	// Everything that we don't handle gets passed back to the parent to process.
	return CallWindowProc( ListViewProc, hWnd, msg, wParam, lParam );
}

LRESULT CALLBACK EditSubProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch( msg )
	{
		case WM_WINDOWPOSCHANGING:
		{
			// Modify the position of the listview edit control. We're moving it to the Filename column.
			WINDOWPOS *wp = ( WINDOWPOS * )lParam;
			wp->x = current_edit_pos.left;
			wp->y = current_edit_pos.top;
			wp->cx = current_edit_pos.right - current_edit_pos.left + 1;
			wp->cy = current_edit_pos.bottom - current_edit_pos.top - 1;
		}
		break;
	}

	// Everything that we don't handle gets passed back to the parent to process.
	return CallWindowProc( EditProc, hWnd, msg, wParam, lParam );
}
