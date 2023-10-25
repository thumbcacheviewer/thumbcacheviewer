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

#include "globals.h"
#include "map_entries.h"
#include "utilities.h"

#include "lite_user32.h"

#define BTN_SCAN		1001
#define BTN_CANCEL		1002
#define BTN_LOAD		1003
#define BTN_DETAILS		1004
#define EDIT_PATH		1005

HWND g_hWnd_tab = NULL;
HWND g_hWnd_scan_tab = NULL;
HWND g_hWnd_static1 = NULL;
HWND g_hWnd_path[ 2 ] = { NULL };
HWND g_hWnd_extensions[ 2 ] = { NULL };
HWND g_hWnd_chk_folders[ 2 ] = { NULL };
HWND g_hWnd_hashing[ 2 ] = { NULL };
HWND g_hWnd_extended_information = NULL;
HWND g_hWnd_btn_scan = NULL;
HWND g_hWnd_btn_cancel = NULL;
HWND g_hWnd_load = NULL;
HWND g_hWnd_static_hash[ 2 ] = { NULL };
HWND g_hWnd_static_count[ 2 ] = { NULL };
HWND g_hWnd_btn_details = NULL;
HWND g_hWnd_static2 = NULL;
HWND g_hWnd_static3 = NULL;
HWND g_hWnd_static4 = NULL;
HWND g_hWnd_static5 = NULL;

unsigned char scan_type = 0;	// 0 = scan directories, 1 = scan ese database
unsigned char tab_index = 0;	// The current tab.

UINT current_dpi_scan = 0;//, last_dpi_scan = USER_DEFAULT_SCREEN_DPI;

HFONT hFont_scan = NULL;

LRESULT CALLBACK ScanTabWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch ( msg )
	{
		case WM_CREATE:
		{
			RECT rc;
			GetClientRect( hWnd, &rc );

			wchar_t current_directory[ MAX_PATH ] = { 0 };
			GetCurrentDirectory( MAX_PATH, current_directory );

			g_hWnd_static1 = CreateWindowA( WC_STATICA, "Initial scan directory:", WS_CHILD | WS_VISIBLE, 0, 0, rc.right, _SCALE_( 15, dpi_scan ), hWnd, NULL, NULL, NULL );
			g_hWnd_path[ 0 ] = CreateWindowEx( WS_EX_CLIENTEDGE, WC_EDIT, current_directory, ES_AUTOHSCROLL | ES_READONLY | WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, ( HMENU )EDIT_PATH, NULL, NULL );
			g_hWnd_path[ 1 ] = CreateWindowEx( WS_EX_CLIENTEDGE, WC_EDIT, NULL, ES_AUTOHSCROLL | ES_READONLY | WS_CHILD, 0, 0, 0, 0, hWnd, ( HMENU )EDIT_PATH, NULL, NULL );
			SendMessage( g_hWnd_path[ 0 ], EM_LIMITTEXT, MAX_PATH - 1, 0 );
			SendMessage( g_hWnd_path[ 1 ], EM_LIMITTEXT, MAX_PATH - 1, 0 );

			g_hWnd_load = CreateWindowA( WC_BUTTONA, "...", WS_CHILD | WS_TABSTOP | WS_VISIBLE, 0, 0, 0, 0, hWnd, ( HMENU )BTN_LOAD, NULL, NULL );

			g_hWnd_static2 = CreateWindowA( WC_STATICA, "Limit scan to the following file types:", WS_CHILD | WS_VISIBLE, 0, _SCALE_( 48, dpi_scan ), rc.right, _SCALE_( 15, dpi_scan ), hWnd, NULL, NULL, NULL );
			g_hWnd_extensions[ 0 ] = CreateWindowEx( WS_EX_CLIENTEDGE, WC_EDIT, L".jpg|.jpeg|.png|.bmp|.gif", ES_AUTOHSCROLL | WS_CHILD | WS_TABSTOP | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, NULL, NULL );
			g_hWnd_extensions[ 1 ] = CreateWindowEx( WS_EX_CLIENTEDGE, WC_EDIT, L".jpg|.jpeg|.png|.bmp|.gif", ES_AUTOHSCROLL | WS_CHILD | WS_TABSTOP, 0, 0, 0, 0, hWnd, NULL, NULL, NULL );
			SendMessage( g_hWnd_extensions[ 0 ], EM_LIMITTEXT, MAX_PATH - 1, 0 );
			SendMessage( g_hWnd_extensions[ 1 ], EM_LIMITTEXT, MAX_PATH - 1, 0 );

			g_hWnd_chk_folders[ 0 ] = CreateWindowA( WC_BUTTONA, "Include Folders", BS_AUTOCHECKBOX | WS_CHILD | WS_TABSTOP | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, NULL, NULL );
			g_hWnd_chk_folders[ 1 ] = CreateWindowA( WC_BUTTONA, "Include Folders", BS_AUTOCHECKBOX | WS_CHILD | WS_TABSTOP, 0, 0, 0, 0, hWnd, NULL, NULL, NULL );

			g_hWnd_extended_information = CreateWindowA( WC_BUTTONA, "Retrieve Extended Information", BS_AUTOCHECKBOX | WS_CHILD | WS_TABSTOP, 0, 0, 0, 0, hWnd, NULL, NULL, NULL );

			g_hWnd_static3 = CreateWindowA( WC_STATICA, "Current file/folder:", WS_CHILD, 0, _SCALE_( 96, dpi_scan ), rc.right, _SCALE_( 15, dpi_scan ), hWnd, NULL, NULL, NULL );
			g_hWnd_hashing[ 0 ] = CreateWindowEx( WS_EX_CLIENTEDGE, WC_EDIT, NULL, ES_AUTOHSCROLL | ES_READONLY | WS_CHILD, 0, 0, 0, 0, hWnd, NULL, NULL, NULL );
			g_hWnd_hashing[ 1 ] = CreateWindowEx( WS_EX_CLIENTEDGE, WC_EDIT, NULL, ES_AUTOHSCROLL | ES_READONLY | WS_CHILD, 0, 0, 0, 0, hWnd, NULL, NULL, NULL );

			g_hWnd_static4 = CreateWindowA( WC_STATICA, "Current file/folder hash:", WS_CHILD, 0, _SCALE_( 144, dpi_scan ), _SCALE_( 200, dpi_scan ), _SCALE_( 15, dpi_scan ), hWnd, NULL, NULL, NULL );
			g_hWnd_static_hash[ 0 ] = CreateWindow( WC_STATIC, NULL, WS_CHILD, _SCALE_( 205, dpi_scan ), _SCALE_( 144, dpi_scan ), rc.right - _SCALE_( 205, dpi_scan ), _SCALE_( 15, dpi_scan ), hWnd, NULL, NULL, NULL );
			g_hWnd_static_hash[ 1 ] = CreateWindow( WC_STATIC, NULL, WS_CHILD, _SCALE_( 205, dpi_scan ), _SCALE_( 144, dpi_scan ), rc.right - _SCALE_( 205, dpi_scan ), _SCALE_( 15, dpi_scan ), hWnd, NULL, NULL, NULL );

			g_hWnd_static5 = CreateWindowA( WC_STATICA, "Total files and/or folders:", WS_CHILD, 0, _SCALE_( 163, dpi_scan ), _SCALE_( 200, dpi_scan ), _SCALE_( 15, dpi_scan ), hWnd, NULL, NULL, NULL );
			g_hWnd_static_count[ 0 ] = CreateWindow( WC_STATIC, NULL, WS_CHILD, _SCALE_( 205, dpi_scan ), _SCALE_( 163, dpi_scan ), rc.right - _SCALE_( 205, dpi_scan ), _SCALE_( 15, dpi_scan ), hWnd, NULL, NULL, NULL );
			g_hWnd_static_count[ 1 ] = CreateWindow( WC_STATIC, NULL, WS_CHILD, _SCALE_( 205, dpi_scan ), _SCALE_( 163, dpi_scan ), rc.right - _SCALE_( 205, dpi_scan ), _SCALE_( 15, dpi_scan ), hWnd, NULL, NULL, NULL );

			// Make pretty font.
			SendMessage( g_hWnd_static1, WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_static2, WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_static3, WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_static4, WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_static5, WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_path[ 0 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_path[ 1 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_load, WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_extensions[ 0 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_extensions[ 1 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_chk_folders[ 0 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_chk_folders[ 1 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_extended_information, WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_hashing[ 0 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_hashing[ 1 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_static_hash[ 0 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_static_hash[ 1 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_static_count[ 0 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_static_count[ 1 ], WM_SETFONT, ( WPARAM )hFont_scan, 0 );

			return 0;
		}
		break;

		case WM_CTLCOLORSTATIC:
		{
			return ( LRESULT )( GetSysColorBrush( COLOR_WINDOW ) );
		}
		break;

		case WM_COMMAND:
		{
			switch( LOWORD( wParam ) )
			{
				case BTN_LOAD:
				{
					wchar_t scan_directory[ MAX_PATH ] = { 0 };

					if ( tab_index == 0 )
					{
						// Open a browse for folder dialog box.
						BROWSEINFO bi = { 0 };
						bi.hwndOwner = hWnd;
						bi.lpszTitle = L"Select a location to scan.";
						bi.ulFlags = BIF_EDITBOX | BIF_VALIDATE;

						OleInitialize( NULL );

						LPITEMIDLIST lpiidl = SHBrowseForFolder( &bi );
						if ( lpiidl )
						{
							// Get the directory path from the id list.
							SHGetPathFromIDList( lpiidl, scan_directory );
							CoTaskMemFree( lpiidl );

							SendMessage( g_hWnd_path[ 0 ], WM_SETTEXT, 0, ( LPARAM )scan_directory );
						}

						OleUninitialize();
					}
					else if ( tab_index == 1 )
					{
						OPENFILENAME ofn = { NULL };
						ofn.lStructSize = sizeof( OPENFILENAME );
						ofn.lpstrFilter = L"Windows Search Database Files (*.edb;*.db)\0*.edb;*.db\0All Files (*.*)\0*.*\0\0";
						ofn.lpstrFile = scan_directory;
						ofn.nMaxFile = MAX_PATH;
						ofn.lpstrTitle = L"Open a Windows Search database file";
						ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_READONLY;
						ofn.hwndOwner = hWnd;

						// Display the Open File dialog box
						if ( GetOpenFileName( &ofn ) )
						{
							SendMessage( g_hWnd_path[ 1 ], WM_SETTEXT, 0, ( LPARAM )scan_directory );
						}
					}
				}
				break;

				case EDIT_PATH:
				{
					if ( HIWORD( wParam ) == EN_CHANGE )
					{
						// Ensure that there's at least 3 characters (for a drive) to scan.
						EnableWindow( g_hWnd_btn_scan, ( SendMessage( ( HWND )lParam, WM_GETTEXTLENGTH, 0, 0 ) >= 3 ) ? TRUE : FALSE );
					}
				}
				break;
			}

			return 0;
		}
		break;

		case WM_SIZE:
		{
			RECT rc;
			GetClientRect( hWnd, &rc );

			// Allow our controls to move in relation to the parent window.
			HDWP hdwp = BeginDeferWindowPos( 15 );
			DeferWindowPos( hdwp, g_hWnd_static1, HWND_TOP, 0, 0, rc.right, _SCALE_( 15, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_static2, HWND_TOP, 0, _SCALE_( 48, dpi_scan ), rc.right, _SCALE_( 15, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_static3, HWND_TOP, 0, _SCALE_( 96, dpi_scan ), rc.right, _SCALE_( 15, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_static4, HWND_TOP, 0, _SCALE_( 144, dpi_scan ), _SCALE_( 200, dpi_scan ), _SCALE_( 15, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_static5, HWND_TOP, 0, _SCALE_( 163, dpi_scan ), _SCALE_( 200, dpi_scan ), _SCALE_( 15, dpi_scan ), SWP_NOZORDER );

			DeferWindowPos( hdwp, g_hWnd_path[ 1 ], HWND_TOP, 0, _SCALE_( 18, dpi_scan ), rc.right - _SCALE_( 35, dpi_scan ), _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_path[ 0 ], HWND_TOP, 0, _SCALE_( 18, dpi_scan ), rc.right - _SCALE_( 35, dpi_scan ), _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_load, HWND_TOP, rc.right - _SCALE_( 30, dpi_scan ), _SCALE_( 18, dpi_scan ), _SCALE_( 30, dpi_scan ), _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_extensions[ 1 ], HWND_TOP, 0, _SCALE_( 66, dpi_scan ), rc.right - _SCALE_( 295, dpi_scan ), _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_extensions[ 0 ], HWND_TOP, 0, _SCALE_( 66, dpi_scan ), rc.right - _SCALE_( 105, dpi_scan ), _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_chk_folders[ 1 ], HWND_TOP, rc.right - _SCALE_( 290, dpi_scan ), _SCALE_( 68, dpi_scan ), _SCALE_( 100, dpi_scan ), _SCALE_( 20, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_chk_folders[ 0 ], HWND_TOP, rc.right - _SCALE_( 100, dpi_scan ), _SCALE_( 68, dpi_scan ), _SCALE_( 100, dpi_scan ), _SCALE_( 20, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_extended_information, HWND_TOP, rc.right - _SCALE_( 180, dpi_scan ), _SCALE_( 68, dpi_scan ), _SCALE_( 180, dpi_scan ), _SCALE_( 20, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_hashing[ 1 ], HWND_TOP, 0, _SCALE_( 114, dpi_scan ), rc.right, _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_hashing[ 0 ], HWND_TOP, 0, _SCALE_( 114, dpi_scan ), rc.right, _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			EndDeferWindowPos( hdwp );

			return 0;
		}
		break;

		default:
		{
			return DefWindowProc( hWnd, msg, wParam, lParam );
		}
		break;
	}
}

LRESULT CALLBACK ScanWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch ( msg )
	{
		case WM_CREATE:
		{
			current_dpi_scan = GetDpiForWindow( hWnd );
			hFont_scan = UpdateFontsAndMetrics( current_dpi_scan, /*last_dpi_scan,*/ NULL );

			RECT rc;
			GetClientRect( hWnd, &rc );

			g_hWnd_tab = CreateWindowEx( WS_EX_CONTROLPARENT, WC_TABCONTROL, NULL, WS_CHILD | WS_CLIPCHILDREN | WS_TABSTOP | WS_VISIBLE, _SCALE_( 10, dpi_scan ), _SCALE_( 10, dpi_scan ), rc.right - _SCALE_( 20, dpi_scan ), rc.bottom - _SCALE_( 50, dpi_scan ), hWnd, NULL, NULL, NULL );

			TCITEMA ti = { 0 };
			ti.mask = TCIF_PARAM | TCIF_TEXT;	// The tab will have text and an lParam value.
			ti.pszText = "Scan Directory";
			ti.lParam = ( LPARAM )0;
			SendMessageA( g_hWnd_tab, TCM_INSERTITEMA, 0, ( LPARAM )&ti );	// Insert a new tab at the end.

			ti.pszText = "Load Windows Search Database";
			ti.lParam = ( LPARAM )1;
			SendMessageA( g_hWnd_tab, TCM_INSERTITEMA, 1, ( LPARAM )&ti );	// Insert a new tab at the end.

			SendMessage( g_hWnd_tab, WM_SETFONT, ( WPARAM )hFont_scan, 0 );

			// Set the tab's font before we get the item rect so that we have an accurate height.
			RECT rc_tab;
			GetClientRect( g_hWnd_tab, &rc );
			SendMessage( g_hWnd_tab, TCM_GETITEMRECT, 0, ( LPARAM )&rc_tab );

			g_hWnd_scan_tab = CreateWindowEx( WS_EX_CONTROLPARENT, L"scan_tab", NULL, WS_CHILD | WS_TABSTOP | WS_VISIBLE, _SCALE_( 10, dpi_scan ), ( rc_tab.bottom + rc_tab.top ) + _SCALE_( 8, dpi_scan ), rc.right - _SCALE_( 20, dpi_scan ), rc.bottom - ( ( rc_tab.bottom + rc_tab.top ) + _SCALE_( 16, dpi_scan ) ), g_hWnd_tab, NULL, NULL, NULL );

			g_hWnd_btn_details = CreateWindowA( WC_BUTTONA, "Show Details \xBB", WS_CHILD | WS_TABSTOP | WS_VISIBLE, 0, 0, 0, 0, hWnd, ( HMENU )BTN_DETAILS, NULL, NULL );

			g_hWnd_btn_scan = CreateWindowA( WC_BUTTONA, "Scan", BS_DEFPUSHBUTTON | WS_CHILD | WS_TABSTOP | WS_VISIBLE, 0, 0, 0, 0, hWnd, ( HMENU )BTN_SCAN, NULL, NULL );
			g_hWnd_btn_cancel = CreateWindowA( WC_BUTTONA, "Cancel", WS_CHILD | WS_TABSTOP | WS_VISIBLE, 0, 0, 0, 0, hWnd, ( HMENU )BTN_CANCEL, NULL, NULL );

			// Make pretty font.
			SendMessage( g_hWnd_btn_details, WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_btn_scan, WM_SETFONT, ( WPARAM )hFont_scan, 0 );
			SendMessage( g_hWnd_btn_cancel, WM_SETFONT, ( WPARAM )hFont_scan, 0 );

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

			return 0;
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
			switch( LOWORD( wParam ) )
			{
				case IDOK:
				case BTN_SCAN:
				{
					if ( !g_kill_scan )
					{
						EnableWindow( g_hWnd_btn_scan, FALSE );
						g_kill_scan = true;
						break;
					}

					int length = ( int )SendMessage( g_hWnd_path[ tab_index ], WM_GETTEXT, MAX_PATH, ( LPARAM )g_filepath );
					if ( length >= 3 )
					{
						// We need to have at least the drive path. Example: "C:\"
						if ( g_filepath[ 1 ] == L':' && g_filepath[ 2 ] == L'\\' )
						{
							// Remove any trailing "\" from the path.
							if ( g_filepath[ length - 1 ] == L'\\' )
							{
								g_filepath[ length - 1 ] = '\0';
							}

							// Now get our extension filters.
							length = ( int )SendMessage( g_hWnd_extensions[ tab_index ], WM_GETTEXT, MAX_PATH, ( LPARAM )( g_extension_filter + 1 ) );
							if ( length > 0 )
							{
								g_extension_filter[ 0 ] = L'|';					// Append the delimiter to the beginning of the string.
								g_extension_filter[ length + 1 ] = L'|';		// Append the delimiter to the end of the string.
								g_extension_filter[ length + 2 ] = L'\0';
								_wcslwr_s( g_extension_filter, length + 3 );	// Set them to lowercase for later comparison.
							}
							else
							{
								g_extension_filter[ 0 ] = L'\0';
							}

							g_include_folders = SendMessage( g_hWnd_chk_folders[ tab_index ], BM_GETCHECK, 0, 0 ) ? true : false;

							g_retrieve_extended_information = SendMessage( g_hWnd_extended_information, BM_GETCHECK, 0, 0 ) ? true : false;

							scan_type = tab_index;	// scan_type will allow the correct windows to update regardless of the selected tab.

							// If use_scanner == false, then read the ese database.
							CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &MapEntries, ( void * )scan_type, 0, NULL ) );
						}
						else
						{
							MessageBoxA( hWnd, "You must specify a valid path.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
						}
					}
				}
				break;

				case BTN_CANCEL:
				{
					SendMessage( hWnd, WM_CLOSE, 0, 0 );
				}
				break;

				case BTN_DETAILS:
				{
					g_show_details = !g_show_details;

					// Hiding the details will allow for a faster scan since it doesn't have to update our controls.
					if ( g_show_details )
					{
						SendMessageA( g_hWnd_btn_details, WM_SETTEXT, 0, ( LPARAM )"Hide Details \xAB" );
						ShowWindow( g_hWnd_static3, SW_SHOW );
						ShowWindow( g_hWnd_static4, SW_SHOW );
						ShowWindow( g_hWnd_static5, SW_SHOW );
						ShowWindow( g_hWnd_hashing[ tab_index ], SW_SHOW );
						ShowWindow( g_hWnd_static_hash[ tab_index ], SW_SHOW );
						ShowWindow( g_hWnd_static_count[ tab_index ], SW_SHOW );
					}
					else
					{
						SendMessageA( g_hWnd_btn_details, WM_SETTEXT, 0, ( LPARAM )"Show Details \xBB" );
						ShowWindow( g_hWnd_static3, SW_HIDE );
						ShowWindow( g_hWnd_static4, SW_HIDE );
						ShowWindow( g_hWnd_static5, SW_HIDE );
						ShowWindow( g_hWnd_hashing[ 0 ], SW_HIDE );
						ShowWindow( g_hWnd_hashing[ 1 ], SW_HIDE );
						ShowWindow( g_hWnd_static_hash[ 0 ], SW_HIDE );
						ShowWindow( g_hWnd_static_hash[ 1 ], SW_HIDE );
						ShowWindow( g_hWnd_static_count[ 0 ], SW_HIDE );
						ShowWindow( g_hWnd_static_count[ 1 ], SW_HIDE );
					}

					// Adjust the window height.
					RECT rc;
					GetWindowRect( hWnd, &rc );
					SetWindowPos( hWnd, NULL, 0, 0, rc.right - rc.left, _SCALE_( ( 324 - ( g_show_details ? 14 : 100 ) ), dpi_scan ), SWP_NOMOVE );
				}
				break;
			}

			return 0;
		}
		break;

		case WM_NOTIFY:
		{
			// Get our listview codes.
			switch ( ( ( LPNMHDR )lParam )->code )
			{
				case TCN_SELCHANGING:		// The tab that's about to lose focus
				{
					NMHDR *nmhdr = ( NMHDR * )lParam;

					int index = ( int )SendMessage( nmhdr->hwndFrom, TCM_GETCURSEL, 0, 0 );		// Get the selected tab
					if ( index == 0 || index == 1 )
					{
						ShowWindow( g_hWnd_path[ index ], SW_HIDE );
						ShowWindow( g_hWnd_extensions[ index ], SW_HIDE );
						ShowWindow( g_hWnd_chk_folders[ index ], SW_HIDE );
						ShowWindow( g_hWnd_hashing[ index ], SW_HIDE );
						ShowWindow( g_hWnd_static_hash[ index ], SW_HIDE );
						ShowWindow( g_hWnd_static_count[ index ], SW_HIDE );

						if ( index == 1 )
						{
							ShowWindow( g_hWnd_extended_information, SW_HIDE );
						}
					}

					return FALSE;
				}
				break;

				case TCN_SELCHANGE:			// The tab that gains focus
				{
					NMHDR *nmhdr = ( NMHDR * )lParam;

					int index = ( int )SendMessage( nmhdr->hwndFrom, TCM_GETCURSEL, 0, 0 );		// Get the selected tab
					if ( index == 0 || index == 1 )
					{
						SendMessageA( g_hWnd_static1, WM_SETTEXT, 0, ( LPARAM )( index == 0 ? "Initial scan directory:" : "Windows Search database file:" ) );

						ShowWindow( g_hWnd_path[ index ], SW_SHOW );
						ShowWindow( g_hWnd_extensions[ index ], SW_SHOW );
						ShowWindow( g_hWnd_chk_folders[ index ], SW_SHOW );

						if ( g_show_details )
						{
							ShowWindow( g_hWnd_hashing[ index ], SW_SHOW );
							ShowWindow( g_hWnd_static_hash[ index ], SW_SHOW );
							ShowWindow( g_hWnd_static_count[ index ], SW_SHOW );
						}

						if ( index == 1 )
						{
							ShowWindow( g_hWnd_extended_information, SW_SHOW );
						}

						// If we're scanning, then enable the scan button. Otherwise, check for a valid path.
						EnableWindow( g_hWnd_btn_scan, ( !g_kill_scan || SendMessage( g_hWnd_path[ index ], WM_GETTEXTLENGTH, 0, 0 ) >= 3 ) ? TRUE : FALSE );

						tab_index = ( unsigned char )index;
					}

					return FALSE;
				}
				break;
			}

			return FALSE;
		}
		break;

		case WM_SIZE:
		{
			RECT rc;
			GetClientRect( hWnd, &rc );

			// Allow our controls to move in relation to the parent window.
			HDWP hdwp = BeginDeferWindowPos( 4 );
			DeferWindowPos( hdwp, g_hWnd_tab, HWND_TOP, _SCALE_( 10, dpi_scan ), _SCALE_( 10, dpi_scan ), rc.right - _SCALE_( 20, dpi_scan ), rc.bottom - _SCALE_( 50, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_btn_details, HWND_TOP, _SCALE_( 10, dpi_scan ), rc.bottom - _SCALE_( 32, dpi_scan ), _SCALE_( 100, dpi_scan ), _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_btn_scan, HWND_TOP, rc.right - _SCALE_( 175, dpi_scan ), rc.bottom - _SCALE_( 32, dpi_scan ), _SCALE_( 80, dpi_scan ), _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_btn_cancel, HWND_TOP, rc.right - _SCALE_( 90, dpi_scan ), rc.bottom - _SCALE_( 32, dpi_scan ), _SCALE_( 80, dpi_scan ), _SCALE_( 23, dpi_scan ), SWP_NOZORDER );
			EndDeferWindowPos( hdwp );

			RECT rc_tab;
			GetClientRect( g_hWnd_tab, &rc );
			SendMessage( g_hWnd_tab, TCM_GETITEMRECT, 0, ( LPARAM )&rc_tab );

			SetWindowPos( g_hWnd_scan_tab, HWND_TOP, _SCALE_( 10, dpi_scan ), ( rc_tab.bottom + rc_tab.top ) + _SCALE_( 8, dpi_scan ), rc.right - _SCALE_( 20, dpi_scan ), rc.bottom - ( ( rc_tab.bottom + rc_tab.top ) + _SCALE_( 16, dpi_scan ) ), SWP_NOZORDER );

			return 0;
		}
		break;

		case WM_GETMINMAXINFO:
		{
			if ( current_dpi_scan == 0 )
			{
				//last_dpi_scan = USER_DEFAULT_SCREEN_DPI;
				current_dpi_scan = GetDpiForWindow( hWnd );
			}

			// Set the minimum dimensions that the window can be sized to.
			( ( MINMAXINFO * )lParam )->ptMinTrackSize.x = _SCALE_( MIN_WIDTH, dpi_scan );
			( ( MINMAXINFO * )lParam )->ptMinTrackSize.y = _SCALE_( ( 324 - ( g_show_details ? 14 : 100 ) ), dpi_scan );
			( ( MINMAXINFO * )lParam )->ptMaxTrackSize.y = ( ( MINMAXINFO * )lParam )->ptMinTrackSize.y;

			return 0;
		}
		break;

		case WM_DPICHANGED:
		{
			//last_dpi_scan = current_dpi_scan;
			current_dpi_scan = HIWORD( wParam );
			HFONT hFont = UpdateFontsAndMetrics( current_dpi_scan, /*last_dpi_scan,*/ NULL );
			EnumChildWindows( hWnd, EnumChildProc, ( LPARAM )hFont );
			DeleteObject( hFont_scan );
			hFont_scan = hFont;

			RECT *rc = ( RECT * )lParam;
			SetWindowPos( hWnd, NULL, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, SWP_NOZORDER | SWP_NOACTIVATE );

			return 0;
		}
		break;

		case WM_ACTIVATE:
		{
			// 0 = inactive, > 0 = active
			g_hWnd_active = ( wParam == 0 ? NULL : hWnd );

			return FALSE;
		}
		break;

		case WM_CLOSE:
		{
			g_kill_scan = true;

			// Reenable the main window.
			EnableWindow( g_hWnd_main, TRUE );
			SetForegroundWindow( g_hWnd_main );

			ShowWindow( hWnd, SW_HIDE );

			return 0;
		}
		break;

		case WM_DESTROY:
		{
			// Delete our font.
			DeleteObject( hFont_scan );

			return 0;
		}
		break;

		case WM_ALERT:
		{
			MessageBoxA( hWnd, ( LPCSTR )lParam, PROGRAM_CAPTION_A, MB_APPLMODAL | ( wParam == 1 ? MB_ICONINFORMATION : MB_ICONWARNING ) | MB_SETFOREGROUND );

			return 0;
		}
		break;

		case WM_PROPAGATE:
		{
			if ( wParam == 1 )
			{
				g_kill_scan = false;

				EnableWindow( g_hWnd_path[ 0 ], FALSE );
				EnableWindow( g_hWnd_path[ 1 ], FALSE );
				EnableWindow( g_hWnd_load, FALSE );
				EnableWindow( g_hWnd_extensions[ 0 ], FALSE );
				EnableWindow( g_hWnd_extensions[ 1 ], FALSE );
				EnableWindow( g_hWnd_chk_folders[ 0 ], FALSE );
				EnableWindow( g_hWnd_chk_folders[ 1 ], FALSE );
				EnableWindow( g_hWnd_extended_information, FALSE );

				// We're scanning. Set the button's text to "Stop".
				SendMessageA( g_hWnd_btn_scan, WM_SETTEXT, 0, ( LPARAM )"Stop" );
			}
			else if ( wParam == 2 )
			{
				// Clear the current file info if we finished the scan without stopping.
				if ( !g_kill_scan )
				{
					SendMessage( g_hWnd_hashing[ scan_type ], WM_SETTEXT, 0, 0 );
					SendMessageA( g_hWnd_static_hash[ scan_type ], WM_SETTEXT, 0, 0 );
				}

				g_kill_scan = true;

				EnableWindow( g_hWnd_path[ 0 ], TRUE );
				EnableWindow( g_hWnd_path[ 1 ], TRUE );
				EnableWindow( g_hWnd_load, TRUE );
				EnableWindow( g_hWnd_extensions[ 0 ], TRUE );
				EnableWindow( g_hWnd_extensions[ 1 ], TRUE );
				EnableWindow( g_hWnd_chk_folders[ 0 ], TRUE );
				EnableWindow( g_hWnd_chk_folders[ 1 ], TRUE );
				EnableWindow( g_hWnd_extended_information, TRUE );

				// We've stopped/finished scanning. Set the button's text to "Scan".
				SendMessageA( g_hWnd_btn_scan, WM_SETTEXT, 0, ( LPARAM )"Scan" );
				EnableWindow( g_hWnd_btn_scan, ( SendMessage( g_hWnd_path[ tab_index ], WM_GETTEXTLENGTH, 0, 0 ) >= 3 ) ? TRUE : FALSE );
			}
			else if ( wParam == 3 )
			{
				SendMessage( g_hWnd_hashing[ scan_type ], WM_SETTEXT, 0, lParam );
			}
			else if ( wParam == 4 )
			{
				SendMessageA( g_hWnd_static_hash[ scan_type ], WM_SETTEXT, 0, lParam );
			}
			else if ( wParam == 5 )
			{
				SendMessageA( g_hWnd_static_count[ scan_type ], WM_SETTEXT, 0, lParam );
			}
			else
			{
				g_kill_scan = true;

				// Reset text information.
				SendMessage( g_hWnd_hashing[ 0 ], WM_SETTEXT, 0, 0 );
				SendMessage( g_hWnd_hashing[ 1 ], WM_SETTEXT, 0, 0 );
				SendMessageA( g_hWnd_static_hash[ 0 ], WM_SETTEXT, 0, 0 );
				SendMessageA( g_hWnd_static_hash[ 1 ], WM_SETTEXT, 0, 0 );
				SendMessageA( g_hWnd_static_count[ 0 ], WM_SETTEXT, 0, 0 );
				SendMessageA( g_hWnd_static_count[ 1 ], WM_SETTEXT, 0, 0 );
				SendMessageA( g_hWnd_btn_scan, WM_SETTEXT, 0, ( LPARAM )"Scan" );
				EnableWindow( g_hWnd_btn_scan, ( SendMessage( g_hWnd_path[ tab_index ], WM_GETTEXTLENGTH, 0, 0 ) >= 3 ) ? TRUE : FALSE );

				SendMessage( g_hWnd_tab, TCM_SETCURFOCUS, 0, 0 );

				// Disable the main window.
				EnableWindow( g_hWnd_main, FALSE );

				// Set the window above all other windows.
				SetForegroundWindow( hWnd );
				SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
				ShowWindow( hWnd, SW_SHOW );
			}

			return 0;
		}
		break;

		default:
		{
			return DefWindowProc( hWnd, msg, wParam, lParam );
		}
		break;
	}
}
