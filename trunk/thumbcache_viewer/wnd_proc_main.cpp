/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011  Eric Kutcher

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

WNDPROC EditProc = NULL;			// Subclassed listview edit window.

// Object variables
HWND g_hWnd_list = NULL;			// Handle to the listview control.
HWND g_hWnd_edit = NULL;			// Handle to the listview edit control.

HMENU g_hMenu = NULL;				// Handle to our menu bar.
HMENU g_hMenuSub_context = NULL;	// Handle to our context menu.

// Window variables
int cx = 0;							// Current x (left) position of the main window based on the mouse.
int cy = 0;							// Current y (top) position of the main window based on the mouse.

bool first_show = true;				// First time the image window gets shown.

RECT last_pos = { 0 };				// The last position of the image window.

RECT current_edit_pos = { 0 };		// Current position of the listview edit control.

bool is_kbytes_size = true;			// Toggle the size text.
bool is_kbytes_offset = true;		// Toggle the database location text.
bool is_dc_lower = true;			// Toggle the data checksum text
bool is_hc_lower = true;			// Toggle the header checksum text.
bool is_eh_lower = true;			// Toggle the entry hash text.

bool is_attached = false;			// Toggled when our windows are attached.
bool skip_main = false;				// Prevents the main window from moving the image window if it is about to attach.

RECT last_dim = { 0 };				// Keeps track of the image window's dimension before it gets minimized.

// Image variables
fileinfo *current_fileinfo = NULL;	// Holds information about the currently selected image. Gets deleted in WM_DESTROY.
char *current_image = NULL;			// Buffer that stores our image and is used to write our files.
Gdiplus::Image *gdi_image = NULL;	// GDI+ image object. We need it to handle .png and .jpg images.

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
			HMENU hMenuSub_help = CreatePopupMenu();
			g_hMenuSub_context = CreatePopupMenu();

			// FILE MENU
			MENUITEMINFO mii = { NULL };
			mii.cbSize = sizeof( MENUITEMINFO );
			mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
			mii.fType = MFT_STRING;
			mii.dwTypeData = L"&Open...\tCtrl+O";
			mii.cch = 8;
			mii.wID = MENU_OPEN;
			InsertMenuItem( hMenuSub_file, 0, TRUE, &mii );

			mii.fType = MFT_SEPARATOR;
			InsertMenuItem( hMenuSub_file, 1, TRUE, &mii );

			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Save All...\tCtrl+S";
			mii.cch = 11;
			mii.wID = MENU_SAVE_ALL;
			mii.fState = MFS_DISABLED;
			InsertMenuItem( hMenuSub_file, 2, TRUE, &mii );
			mii.dwTypeData = L"Save Selected...\tCtrl+Shift+S";
			mii.cch = 16;
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
			mii.cch = 15;
			mii.wID = MENU_REMOVE_SEL;
			mii.fState = MFS_DISABLED;
			InsertMenuItem( hMenuSub_edit, 0, TRUE, &mii );

			mii.fType = MFT_SEPARATOR;
			InsertMenuItem( hMenuSub_edit, 1, TRUE, &mii );

			mii.fType = MFT_STRING;
			mii.dwTypeData = L"Select All\tCtrl+A";
			mii.cch = 10;
			mii.wID = MENU_SELECT_ALL;
			InsertMenuItem( hMenuSub_edit, 2, TRUE, &mii );

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

			mii.dwTypeData = L"&Help";
			mii.cch = 5;
			mii.hSubMenu = hMenuSub_help;
			InsertMenuItem( g_hMenu, 2, TRUE, &mii );

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
			SendMessage( g_hWnd_list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_BORDERSELECT );

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
			if ( is_attached == false )
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

				// Save our last position.
				last_pos.bottom = rc->bottom;
				last_pos.left = rc->left;
				last_pos.right = rc->right;
				last_pos.top = rc->top;
			}
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
						wchar_t filepath[ MAX_PATH + 1 ] = { 0 };
						OPENFILENAME ofn = { NULL };
						ofn.lStructSize = sizeof( OPENFILENAME );
						ofn.lpstrFilter = L"Thumbcache Database Files (*.db)\0*.db\0All Files (*.*)\0*.*\0";
						ofn.lpstrFile = filepath;
						ofn.nMaxFile = sizeof( filepath );
						ofn.lpstrTitle = L"Open a Thumbcache Database file";
						ofn.Flags = OFN_READONLY;

						// Display the Open File dialog box
						if( GetOpenFileName( &ofn ) )
						{
							read_database( *filepath );
							break;
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

						wchar_t save_directory[ MAX_PATH + 1 ] = { 0 };
						LPITEMIDLIST lpiidl = SHBrowseForFolder( &bi );
						if ( lpiidl )
						{
							// Get the directory path from the id list.
							SHGetPathFromIDList( lpiidl, save_directory );
							
							// Depending on what was selected, get the number of items we'll be saving.
							int num_items = 0;
							if ( LOWORD( wParam ) == MENU_SAVE_ALL )
							{
								num_items = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );
							}
							else
							{
								num_items = SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 );
							}

							// Retrieve the lparam value from the selected listview item.
							LVITEM lvi = { NULL };
							lvi.mask = LVIF_PARAM;
							lvi.iItem = -1;	// Set this to -1 so that the LVM_GETNEXTITEM call can go through the list correctly.

							// Go through all the items we'll be saving.
							for ( int i = 0; i < num_items; i++ )
							{
								if ( LOWORD( wParam ) == MENU_SAVE_ALL )
								{
									lvi.iItem = i;
								}
								else
								{
									lvi.iItem = SendMessage( g_hWnd_list, LVM_GETNEXTITEM, lvi.iItem, LVNI_SELECTED );
								}
								SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );
	
								// Create a buffer to read in our new bitmap.
								char *save_image = new char[ ( ( fileinfo * )lvi.lParam )->size ];

								// Attempt to open a file for reading.
								HANDLE hFile = CreateFile( ( ( fileinfo * )lvi.lParam )->dbpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
								if ( hFile != INVALID_HANDLE_VALUE )
								{
									DWORD read = 0;
									// Set our file pointer to the beginning of the database file.
									SetFilePointer( hFile, ( ( fileinfo * )lvi.lParam )->offset, 0, FILE_BEGIN );
									// Read the entire image into memory.
									ReadFile( hFile, save_image, ( ( fileinfo * )lvi.lParam )->size, &read, NULL );
									CloseHandle( hFile );

									// The fullpath isn't going to be 2 * MAX_PATH long and if it is, it won't get saved.
									wchar_t fullpath[ ( 2 * MAX_PATH ) + 2 ] = { 0 };
									swprintf( fullpath, ( 2 * MAX_PATH ) + 2, L"%s\\%s", save_directory, ( ( fileinfo * )lvi.lParam )->filename );

									// Attempt to open a file for saving.
									HANDLE hFile_save = CreateFile( fullpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
									if ( hFile_save != INVALID_HANDLE_VALUE )
									{
										// Write the buffer to our file.
										DWORD dwBytesWritten = 0;
										WriteFile( hFile_save, save_image, ( ( fileinfo * )lvi.lParam )->size, &dwBytesWritten, NULL );

										CloseHandle( hFile_save );
									}

									// See if the path was too long.
									if ( GetLastError() == ERROR_PATH_NOT_FOUND )
									{
										MessageBox( hWnd, L"One or more files could not be saved. Please check the filename and path.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
									}
								}
								// Delete our buffer.
								delete[] save_image;
							}
						}
					}
					break;

					case MENU_REMOVE_SEL:
					{
						// Hide the listview edit box if it's visible.
						if ( IsWindowVisible( g_hWnd_edit ) )
						{
							ShowWindow( g_hWnd_edit, SW_HIDE );
						}

						// Hide the image window since the selected item will be deleted.
						if ( IsWindowVisible( g_hWnd_image ) )
						{
							ShowWindow( g_hWnd_image, SW_HIDE );
						}

						// Get the first selected item.
						int selected_index = SendMessage( g_hWnd_list, LVM_GETNEXTITEM, -1, LVNI_SELECTED );
						while ( selected_index != -1 )
						{
							// We first need to get the lparam value otherwise the memory won't be freed.
							LVITEMA lvi = { NULL };
							lvi.mask = LVIF_PARAM;
							lvi.iItem = selected_index;
							SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );
							
							// Delete our filename, then fileinfo structure.
							delete[] ( ( fileinfo * )lvi.lParam )->filename;
							delete ( fileinfo * )lvi.lParam;

							// Remove the list item.
							SendMessage( g_hWnd_list, LVM_DELETEITEM, selected_index, 0 );

							// Get the next selected item in the list.
							selected_index = SendMessage( g_hWnd_list, LVM_GETNEXTITEM, -1, LVNI_SELECTED );
						}
					}
					break;

					case MENU_SELECT_ALL:
					{
						// Hide the listview edit box if it's visible.
						if ( IsWindowVisible( g_hWnd_edit ) )
						{
							ShowWindow( g_hWnd_edit, SW_HIDE );
						}

						// Set the state of all items to selected.
						LVITEM lvi = { NULL };
						lvi.mask = LVIF_STATE;
						lvi.state = LVIS_SELECTED;
						lvi.stateMask = LVIS_SELECTED;
						SendMessage( g_hWnd_list, LVM_SETITEMSTATE, -1, ( LPARAM )&lvi );
					}
					break;

					case MENU_ABOUT:
					{
						MessageBox( hWnd, L"Thumbcache Viewer is made free under the GPLv3 license.\n\nCopyright \xA9 2011 Eric Kutcher", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONINFORMATION );
					}
					break;

					case MENU_EXIT:
					{
						DestroyWindow( hWnd );
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
					// Make sure the control key is down.
					if ( GetKeyState( VK_CONTROL ) & 0x8000 )
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

				case HDN_ITEMCLICK:
				{
					if ( ( ( LPNMHEADER )lParam )->iItem == 2 )			// Change the size column info.
					{
						is_kbytes_size = !is_kbytes_size;
						InvalidateRect( g_hWnd_list, NULL, TRUE );
					}
					else if ( ( ( LPNMHEADER )lParam )->iItem == 3 )	// Change the database location column info.
					{
						is_kbytes_offset = !is_kbytes_offset;
						InvalidateRect( g_hWnd_list, NULL, TRUE );
					}
					else if ( ( ( LPNMHEADER )lParam )->iItem == 4 )	// Change the data checksum column info.
					{
						is_dc_lower = !is_dc_lower;
						InvalidateRect( g_hWnd_list, NULL, TRUE );
					}
					else if ( ( ( LPNMHEADER )lParam )->iItem == 5 )	// Change the header checksum column info.
					{
						is_hc_lower = !is_hc_lower;
						InvalidateRect( g_hWnd_list, NULL, TRUE );
					}
					else if ( ( ( LPNMHEADER )lParam )->iItem == 6 )	// Change the entry hash column info.
					{
						is_eh_lower = !is_eh_lower;
						InvalidateRect( g_hWnd_list, NULL, TRUE );
					}
				}
				break;

				case HDN_ITEMCHANGING:
				{
					// Prevents all columns from being resized to less than MIN_COLUMN_SIZE.
					if ( ( ( LPNMHEADER )lParam )->pitem->cxy <= MIN_COLUMN_SIZE )
					{
						return TRUE;
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
						// Disable the Select All menu.
						EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_DISABLED );
						EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_DISABLED );
					}
				}
				break;

				case LVN_ITEMCHANGED:
				{
					NMLISTVIEW *nmlv = ( NMLISTVIEW * )lParam;
					
					// If nothing was clicked, or the new item state is neither focused and selected, or just selected, then ignore the action completely.
					if ( nmlv->iItem == -1 || ( nmlv->uNewState != ( LVIS_FOCUSED | LVIS_SELECTED ) && nmlv->uNewState != LVIS_SELECTED ) )	
					{
						// See if the old state was selected
						if ( nmlv->uOldState == LVIS_SELECTED )
						{
							// Now see how many items remain selected
							if ( SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 ) == 0 )
							{
								// If there's no more items selected, then disable the Selection menu item.
								EnableMenuItem( g_hMenu, MENU_SAVE_SEL, MF_DISABLED );
								EnableMenuItem( g_hMenu, MENU_REMOVE_SEL, MF_DISABLED );
								EnableMenuItem( g_hMenuSub_context, MENU_SAVE_SEL, MF_DISABLED );
								EnableMenuItem( g_hMenuSub_context, MENU_REMOVE_SEL, MF_DISABLED );
							}

							// See how many items remain in the list.
							if ( SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) > 0 )
							{
								// Disable the Select All menu.
								EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_ENABLED );
								EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_ENABLED );
							}
						}
						break;
					}

					// If the number of selected equals the number of items in the list, then disable the Select All button.
					if ( SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) == SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 ) )
					{
						// Disable the Select All menu.
						EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_DISABLED );
						EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_DISABLED );
					}

					// If an item has been selected, enable our Remove Selected and Save Selected menu items.
					EnableMenuItem( g_hMenu, MENU_REMOVE_SEL, MF_ENABLED );
					EnableMenuItem( g_hMenu, MENU_SAVE_SEL, MF_ENABLED );
					EnableMenuItem( g_hMenuSub_context, MENU_REMOVE_SEL, MF_ENABLED );
					EnableMenuItem( g_hMenuSub_context, MENU_SAVE_SEL, MF_ENABLED );
					
					// Retrieve the lparam value from the selected listview item.
					LVITEM lvi = { NULL };
					lvi.mask = LVIF_PARAM;
					lvi.iItem = nmlv->iItem;
					SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

					// Delete our image buffer if it already exists.
					if ( current_image != NULL )
					{
						delete[] current_image;	
					}
					// Create a buffer to read in our new bitmap.
					current_image = new char[ ( ( fileinfo * )lvi.lParam )->size ];

					// Attempt to open a file for reading.
					HANDLE hFile = CreateFile( ( ( fileinfo * )lvi.lParam )->dbpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
					if ( hFile != INVALID_HANDLE_VALUE )
					{
						DWORD read = 0;
						// Set our file pointer to the beginning of the database file.
						SetFilePointer( hFile, ( ( fileinfo * )lvi.lParam )->offset, 0, FILE_BEGIN );
						// Read the entire image into memory.
						ReadFile( hFile, current_image, ( ( fileinfo * )lvi.lParam )->size, &read, NULL );
						CloseHandle( hFile );

						// If gdi_image exists, then delete it.
						if ( gdi_image != NULL )
						{
							delete gdi_image;
						}

						// Create a stream to store our buffer and then store the stream into a GDI+ image object.
						ULONG written = 0;
						IStream *is = NULL;
						CreateStreamOnHGlobal( NULL, TRUE, &is );
						is->Write( current_image, ( ( fileinfo * )lvi.lParam )->size, &written );
						gdi_image = new Gdiplus::Image( is );
						is->Release();

						RECT rc = { 0 };
						if ( !IsWindowVisible( g_hWnd_image ) )
						{
							// Only move the window if it's the first time showing it.
							if ( first_show == true )
							{
								// Make sure our main window isn't maximized. If it is, then the image window will be created in the center.
								if ( !IsZoomed( hWnd ) )
								{
									// Move our image window next to the main window on its right side.
									HDWP hdwp = BeginDeferWindowPos( 1 );
									
									GetWindowRect( hWnd, &rc );
									DeferWindowPos( hdwp, g_hWnd_image, HWND_TOPMOST, rc.right, rc.top, MIN_HEIGHT, MIN_HEIGHT, SWP_NOACTIVATE );
									EndDeferWindowPos( hdwp );

									is_attached = true;	// The two windows will now be lined up.
									first_show = false;	// Prevent this condition from occuring again.
								}
							}

							// This is done to keep both windows on top of other windows.
							// Set the image window position on top of all windows except topmost windows, but don't set focus to it.
							SetWindowPos( g_hWnd_image, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );
							// Set our main window on top of the image window.
							SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
							// Now that the image window is on top of everything (except the main window), set it back to non-topmost and show it.
							SetWindowPos( g_hWnd_image, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
						}

						// Set our image window's icon to match the file extension.
						if ( ( ( fileinfo * )lvi.lParam )->extension == 0 )
						{
							SendMessage( g_hWnd_image, WM_SETICON, ICON_SMALL, ( LPARAM )hIcon_bmp );
						}
						else if ( ( ( fileinfo * )lvi.lParam )->extension == 1 )
						{
							SendMessage( g_hWnd_image, WM_SETICON, ICON_SMALL, ( LPARAM )hIcon_jpg );
						}
						else if ( ( ( fileinfo * )lvi.lParam )->extension == 2 )
						{
							SendMessage( g_hWnd_image, WM_SETICON, ICON_SMALL, ( LPARAM )hIcon_png );
						}

						// Set the image window's new title.
						wchar_t new_title[ MAX_PATH + 65 ] = { 0 };
						swprintf( new_title, MAX_PATH + 65, L"%s - %dx%d", ( ( fileinfo * )lvi.lParam )->filename, gdi_image->GetWidth(), gdi_image->GetHeight() );
						SetWindowText( g_hWnd_image, new_title );

						// See if our image window is minimized and set the rectangle to its old size if it is.
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

					// Select all the text except the file extension (will be 4 characters long including the '.')
					SendMessage( g_hWnd_edit, EM_SETSEL, 0, wcslen( current_fileinfo->filename ) - 4 );

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
					unsigned int length = wcslen( pdi->item.pszText ) + 1;
					if ( length == 0 )
					{
						return FALSE;
					}

					// Delete the old filename.
					delete[] current_fileinfo->filename;
					// Create a new filename based on the editbox's text.
					wchar_t *filename = new wchar_t[ length ];
					wmemset( filename, 0, length );
					wcscpy_s( filename, length, pdi->item.pszText );

					// Modify our listview item's fileinfo lparam value.
					current_fileinfo->filename = filename;

					// Set the image window's new title.
					wchar_t new_title[ MAX_PATH + 65 ] = { 0 };
					swprintf( new_title, MAX_PATH + 65, L"%s - %dx%d", filename, gdi_image->GetWidth(), gdi_image->GetHeight() );
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
					HBRUSH color = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_HOTLIGHT ) );
					FillRect( dis->hDC, &dis->rcItem, color );
					DeleteObject( color );
					selected = true;
				}

				// Get the item's text.
				wchar_t buf[ MAX_PATH + 1 ];
				LVITEM lvi = { 0 };
				lvi.mask = LVIF_PARAM;
				lvi.iItem = dis->itemID;

				// This is the full size of the row.
				RECT last_rc = { 0 };

				// This will keep track of the current colunn's left position.
				int last_left = 0;

				// Adjust the alignment position of the text.
				int RIGHT_COLUMNS = 0;

				// Loop through all the columns
				for ( int i = 0; i <= 8; i++ )
				{
					lvi.iSubItem = i;	// Set the column
					SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );	// Get the lparam value from our item.

					RIGHT_COLUMNS = 0;

					// Save the appropriate text in our buffer for the current column.
					switch ( i )
					{
						case 0:
						{
							swprintf( buf, 9, L"%d", dis->itemID + 1 );
						}
						break;

						case 1:
						{
							swprintf( buf, MAX_PATH + 1, L"%s", ( ( fileinfo * )lvi.lParam )->filename );
						}
						break;

						case 2:
						{
							RIGHT_COLUMNS = DT_RIGHT;

							// Depending on our toggle, output the size in either kilobytes or bytes.
							if ( is_kbytes_size == true )
							{
								swprintf( buf, MAX_PATH, L"%d KB", ( ( fileinfo * )lvi.lParam )->size / 1024 );
							}
							else
							{
								swprintf( buf, MAX_PATH, L"%d B", ( ( fileinfo * )lvi.lParam )->size );
							}
						}
						break;

						case 3:
						{
							RIGHT_COLUMNS = DT_RIGHT;

							// Depending on our toggle, output the offset (db location) in either kilobytes or bytes.
							if ( is_kbytes_offset == true )
							{
								swprintf( buf, MAX_PATH, L"%d B", ( ( fileinfo * )lvi.lParam )->offset );
							}
							else
							{
								swprintf( buf, MAX_PATH, L"%d KB", ( ( fileinfo * )lvi.lParam )->offset / 1024 );
							}
						}
						break;

						case 4:
						{
							// Output the hex string in either lowercase or uppercase.
							if ( is_dc_lower == true )
							{
								swprintf( buf, 19, L"0x%08x%08x", ( ( fileinfo * )lvi.lParam )->data_checksum, ( ( fileinfo * )lvi.lParam )->data_checksum + 4 );
							}
							else
							{
								swprintf( buf, 19, L"0x%08X%08X", ( ( fileinfo * )lvi.lParam )->data_checksum, ( ( fileinfo * )lvi.lParam )->data_checksum + 4 );
							}
						}
						break;

						case 5:
						{
							// Output the hex string in either lowercase or uppercase.
							if ( is_hc_lower == true )
							{
								swprintf( buf, 19, L"0x%08x%08x", ( ( fileinfo * )lvi.lParam )->header_checksum, ( ( fileinfo * )lvi.lParam )->header_checksum + 4 );
							}
							else
							{
								swprintf( buf, 19, L"0x%08X%08X", ( ( fileinfo * )lvi.lParam )->header_checksum, ( ( fileinfo * )lvi.lParam )->header_checksum + 4 );
							}
						}
						break;

						case 6:
						{
							// Output the hex string in either lowercase or uppercase.
							if ( is_eh_lower == true )
							{
								swprintf( buf, 19, L"0x%08x%08x", ( ( fileinfo * )lvi.lParam )->entry_hash, ( ( fileinfo * )lvi.lParam )->entry_hash + 4 );
							}
							else
							{
								swprintf( buf, 19, L"0x%08X%08X", ( ( fileinfo * )lvi.lParam )->entry_hash, ( ( fileinfo * )lvi.lParam )->entry_hash + 4 );
							}
						}
						break;

						case 7:
						{
							if ( ( ( fileinfo * )lvi.lParam )->system == WINDOWS_7 )
							{
								swprintf( buf, 10, L"Windows 7" );
							}
							else
							{
								swprintf( buf, 14, L"Windows Vista" );
							}
						}
						break;

						case 8:
						{
							swprintf( buf, MAX_PATH + 1, L"%s", ( ( fileinfo * )lvi.lParam )->dbpath );
						}
						break;
					}

					// Get the dimensions of the listview column
					LVCOLUMN lvc = { 0 };
					lvc.mask = LVCF_WIDTH;
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
						// Black text.
						SetTextColor( hdcMem, RGB( 0x00, 0x00, 0x00 ) );
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
			DestroyWindow( hWnd );
			return 0;
		}
		break;

		case WM_DESTROY:
		{
			// Get the number of items in the listview.
			int num_items = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );
			if ( num_items > 0 )
			{
				// Go through each item, and delete their lparam values. current_fileinfo will get deleted here.
				for ( int i = 0; i < num_items; i++ )
				{
					LVITEMA lvi = { NULL };
					lvi.mask = LVIF_PARAM;
					lvi.iItem = i;
					SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

					// First delete the filename pointer.
					delete[] ( ( fileinfo * )lvi.lParam )->filename;
					// Then delete the fileinfo structure.
					delete ( fileinfo * )lvi.lParam;
				}
			}
			// We may not have initialized these, but if they exist, delete them.
			if ( current_image != NULL )
			{
				delete[] current_image;
			}

			if ( gdi_image != NULL )
			{
				delete gdi_image;
			}

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
