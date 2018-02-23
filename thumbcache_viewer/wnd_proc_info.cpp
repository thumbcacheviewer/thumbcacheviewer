/*
	thumbcache_viewer will extract thumbnail images from thumbcache database files.
	Copyright (C) 2011-2018 Eric Kutcher

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
#include "utilities.h"
#include "menus.h"

#include <stdio.h>

#define BTN_OK	1000

HWND g_hWnd_static_mapped_hash = NULL;
HWND g_hWnd_list_info = NULL;
HWND g_hWnd_btn_close = NULL;

fileinfo *g_current_fi = NULL;

int CALLBACK InfoCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	extended_info *ei1 = ( ( extended_info * )lParam1 );
	extended_info *ei2 = ( ( extended_info * )lParam2 );

	unsigned char index = 0;

	// We added 2 to the lParamSort value in order to distinguish between items we want to sort up, and items we want to sort down.
	// Saves us from having to pass some arbitrary struct pointer.
	if ( lParamSort >= 2 )	// Up
	{
		index = ( unsigned char )( lParamSort % 2 );
	}
	else
	{
		index = ( unsigned char )lParamSort;

		ei1 = ( ( extended_info * )lParam2 );
		ei2 = ( ( extended_info * )lParam1 );
	}
		
	switch ( index )
	{
		case 0:
		{
			if ( ei1->sei == NULL && ei2->sei == NULL ) { return 0; }
			else if ( ei1->sei != NULL && ei2->sei == NULL ) { return 1; }
			else if ( ei1->sei == NULL && ei2->sei != NULL ) { return -1; }
			return _wcsicmp( ei1->sei->windows_property, ei2->sei->windows_property );
		}
		break;

		case 1:
		{
			return _wcsicmp( ( ei1->property_value != NULL ? ei1->property_value : L"" ), ( ei2->property_value != NULL ? ei2->property_value : L"" ) );
		}
		break;

		default:
		{
			return 0;
		}
		break;
	}	
}

LRESULT CALLBACK InfoWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch ( msg )
    {
		case WM_CREATE:
		{
			RECT rc;
			GetClientRect( hWnd, &rc );

			HWND g_hWnd_static1 = CreateWindowA( WC_STATICA, "Mapped Hash:", WS_CHILD | WS_VISIBLE, 20, 20, rc.right - 310, 15, hWnd, NULL, NULL, NULL );
			g_hWnd_static_mapped_hash = CreateWindow( WC_STATIC, NULL, WS_CHILD | WS_VISIBLE, rc.right - 290, 20, rc.right - 195, 15, hWnd, NULL, NULL, NULL );

			g_hWnd_list_info = CreateWindowEx( WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL, LVS_REPORT | LVS_OWNERDRAWFIXED | WS_CHILDWINDOW | WS_VISIBLE, 20, 40, rc.right - 40, rc.bottom - 90, hWnd, NULL, NULL, NULL );
			SendMessage( g_hWnd_list_info, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES );

			// Initialize our listview columns
			LVCOLUMNA lvc = { NULL }; 
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT; 
			lvc.fmt = LVCFMT_CENTER;
			lvc.pszText = "Windows Property";
			lvc.cx = 200;
			SendMessageA( g_hWnd_list_info, LVM_INSERTCOLUMNA, 0, ( LPARAM )&lvc );

			lvc.fmt = LVCFMT_LEFT;
			lvc.pszText = "Property Value";
			lvc.cx = 200;
			SendMessageA( g_hWnd_list_info, LVM_INSERTCOLUMNA, 1, ( LPARAM )&lvc );

			g_hWnd_btn_close = CreateWindowA( WC_BUTTONA, "Close", BS_DEFPUSHBUTTON | WS_CHILD | WS_TABSTOP | WS_VISIBLE, ( ( rc.right - rc.left ) / 2 ) - 40, rc.bottom - 32, 80, 23, hWnd, ( HMENU )BTN_OK, NULL, NULL );

			// Make pretty font.
			SendMessage( g_hWnd_static1, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_static_mapped_hash, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_list_info, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_btn_close, WM_SETFONT, ( WPARAM )hFont, 0 );

			return 0;
		}
		break;

		case WM_COMMAND:
		{
			switch( LOWORD( wParam ) )
			{
				case IDOK:
				case BTN_OK:
				{
					SendMessage( hWnd, WM_CLOSE, 0, 0 );
				}
				break;

				case MENU_PROP_VALUE:
				{
					LVITEM lvi = { NULL };
					lvi.mask = LVIF_PARAM;
					lvi.iItem = ( int )SendMessage( g_hWnd_list_info, LVM_GETNEXTITEM, -1, LVNI_FOCUSED | LVNI_SELECTED );

					if ( lvi.iItem != -1 )
					{
						SendMessage( g_hWnd_list_info, LVM_GETITEM, 0, ( LPARAM )&lvi );

						SendMessage( g_hWnd_property, WM_PROPAGATE, 0, lvi.lParam );
					}
				}
				break;

				case MENU_COPY_SEL:
				{
					CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &copy_items, ( void * )1, 0, NULL ) );
				}
				break;

				case MENU_SELECT_ALL:
				{
					// Set the state of all items to selected.
					LVITEM lvi = { NULL };
					lvi.mask = LVIF_STATE;
					lvi.state = LVIS_SELECTED;
					lvi.stateMask = LVIS_SELECTED;
					SendMessage( g_hWnd_list_info, LVM_SETITEMSTATE, -1, ( LPARAM )&lvi );
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
				case LVN_KEYDOWN:
				{
					NMLISTVIEW *nmlv = ( NMLISTVIEW * )lParam;

					// Make sure the control key is down and that we're not already in a worker thread. Prevents threads from queuing in case the user falls asleep on their keyboard.
					if ( GetKeyState( VK_CONTROL ) & 0x8000 && !in_thread )
					{
						// Determine which key was pressed.
						switch ( ( ( LPNMLVKEYDOWN )lParam )->wVKey )
						{
							case 'A':	// Select all items if Ctrl + A is down and there are items in the list.
							{
								if ( SendMessage( nmlv->hdr.hwndFrom, LVM_GETITEMCOUNT, 0, 0 ) > 0 )
								{
									SendMessage( hWnd, WM_COMMAND, MENU_SELECT_ALL, 0 );
								}
							}
							break;

							case 'C':	// Copy selected items if Ctrl + C is down and there are selected items in the list.
							{
								if ( SendMessage( nmlv->hdr.hwndFrom, LVM_GETSELECTEDCOUNT, 0, 0 ) > 0 )
								{
									SendMessage( hWnd, WM_COMMAND, MENU_COPY_SEL, 0 );
								}
							}
							break;

							case 'V':	// View property value.
							{
								if ( SendMessage( nmlv->hdr.hwndFrom, LVM_GETSELECTEDCOUNT, 0, 0 ) > 0 )
								{
									SendMessage( hWnd, WM_COMMAND, MENU_PROP_VALUE, 0 );
								}
							}
							break;
						}
					}
				}
				break;

				case LVN_COLUMNCLICK:
				{
					NMLISTVIEW *nmlv = ( NMLISTVIEW * )lParam;

					LVCOLUMN lvc = { NULL };
					lvc.mask = LVCF_FMT;
					SendMessage( g_hWnd_list_info, LVM_GETCOLUMN, nmlv->iSubItem, ( LPARAM )&lvc );
					
					if ( HDF_SORTUP & lvc.fmt )	// Column is sorted upward.
					{
						// Sort down
						lvc.fmt = lvc.fmt & ( ~HDF_SORTUP ) | HDF_SORTDOWN;
						SendMessage( g_hWnd_list_info, LVM_SETCOLUMN, ( WPARAM )nmlv->iSubItem, ( LPARAM )&lvc );

						SendMessage( g_hWnd_list_info, LVM_SORTITEMS, nmlv->iSubItem, ( LPARAM )( PFNLVCOMPARE )InfoCompareFunc );
					}
					else if ( HDF_SORTDOWN & lvc.fmt )	// Column is sorted downward.
					{
						// Sort up
						lvc.fmt = lvc.fmt & ( ~HDF_SORTDOWN ) | HDF_SORTUP;
						SendMessage( g_hWnd_list_info, LVM_SETCOLUMN, nmlv->iSubItem, ( LPARAM )&lvc );

						SendMessage( g_hWnd_list_info, LVM_SORTITEMS, nmlv->iSubItem + 2, ( LPARAM )( PFNLVCOMPARE )InfoCompareFunc );
					}
					else	// Column has no sorting set.
					{
						// Remove the sort format for all columns.
						for ( int i = 0; i < 2; i++ )
						{
							// Get the current format
							SendMessage( g_hWnd_list_info, LVM_GETCOLUMN, i, ( LPARAM )&lvc );
							// Remove sort up and sort down
							lvc.fmt = lvc.fmt & ( ~HDF_SORTUP ) & ( ~HDF_SORTDOWN );
							SendMessage( g_hWnd_list_info, LVM_SETCOLUMN, i, ( LPARAM )&lvc );
						}

						// Read current the format from the clicked column
						SendMessage( g_hWnd_list_info, LVM_GETCOLUMN, nmlv->iSubItem, ( LPARAM )&lvc );
						// Sort down to start.
						lvc.fmt = lvc.fmt | HDF_SORTDOWN;
						SendMessage( g_hWnd_list_info, LVM_SETCOLUMN, nmlv->iSubItem, ( LPARAM )&lvc );

						SendMessage( g_hWnd_list_info, LVM_SORTITEMS, nmlv->iSubItem, ( LPARAM )( PFNLVCOMPARE )InfoCompareFunc );
					}
				}
				break;

				case NM_RCLICK:
				{
					NMITEMACTIVATE *nmitem = ( NMITEMACTIVATE *)lParam;

					long type = nmitem->iItem != -1 ? MF_ENABLED : MF_DISABLED;
					EnableMenuItem( g_hMenuSub_ei_context, MENU_COPY_SEL, type );
					EnableMenuItem( g_hMenuSub_ei_context, MENU_PROP_VALUE, type );

					int item_count = ( int )SendMessage( nmitem->hdr.hwndFrom, LVM_GETITEMCOUNT, 0, 0 );
					EnableMenuItem( g_hMenuSub_ei_context, MENU_SELECT_ALL, ( item_count > 0 && SendMessage( nmitem->hdr.hwndFrom, LVM_GETSELECTEDCOUNT, 0, 0 ) < item_count ? MF_ENABLED : MF_DISABLED ) );

					// Show our edit context menu as a popup.
					POINT p;
					GetCursorPos( &p ) ;
					TrackPopupMenu( g_hMenuSub_ei_context, 0, p.x, p.y, 0, hWnd, NULL );
				}
				break;

				case NM_DBLCLK:
				{
					NMITEMACTIVATE *nmitem = ( NMITEMACTIVATE *)lParam;

					if ( nmitem->iItem != -1 )
					{
						SendMessage( hWnd, WM_COMMAND, MENU_PROP_VALUE, 0 );
					}
				}
				break;
			}
			return FALSE;
		}
		break;

		case WM_MEASUREITEM:
		{
			// Set the row height of the list view.
			if ( ( ( LPMEASUREITEMSTRUCT )lParam )->CtlType = ODT_LISTVIEW )
			{
				( ( LPMEASUREITEMSTRUCT )lParam )->itemHeight = row_height;
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

		case WM_CTLCOLORSTATIC:
		{
			return ( LRESULT )( GetSysColorBrush( COLOR_WINDOW ) );
		}
		break;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint( hWnd, &ps );

			RECT client_rc, frame_rc;
			GetClientRect( hWnd, &client_rc );

			// Create a memory buffer to draw to.
			HDC hdcMem = CreateCompatibleDC( hDC );

			HBITMAP hbm = CreateCompatibleBitmap( hDC, client_rc.right - client_rc.left, client_rc.bottom - client_rc.top );
			HBITMAP ohbm = ( HBITMAP )SelectObject( hdcMem, hbm );
			DeleteObject( ohbm );
			DeleteObject( hbm );

			// Fill the background.
			HBRUSH color = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_MENU ) );
			FillRect( hdcMem, &client_rc, color );
			DeleteObject( color );

			frame_rc = client_rc;
			frame_rc.left += 10;
			frame_rc.right -= 10;
			frame_rc.top += 10;
			frame_rc.bottom -= 40;

			// Fill the frame.
			color = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_WINDOW ) );
			FillRect( hdcMem, &frame_rc, color );
			DeleteObject( color );

			// Draw the frame's border.
			DrawEdge( hdcMem, &frame_rc, EDGE_ETCHED, BF_RECT );

			// Draw our memory buffer to the main device context.
			BitBlt( hDC, client_rc.left, client_rc.top, client_rc.right, client_rc.bottom, hdcMem, 0, 0, SRCCOPY );

			DeleteDC( hdcMem );
			EndPaint( hWnd, &ps );

			return 0;
		}
		break;

		case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *dis = ( DRAWITEMSTRUCT * )lParam;
      
			// The item we want to draw is our listview.
			if ( dis->CtlType == ODT_LISTVIEW && dis->itemData != NULL )
			{
				extended_info *ei = ( extended_info * )dis->itemData;

				// Alternate item color's background.
				if ( dis->itemID % 2 )	// Even rows will have a light grey background.
				{
					HBRUSH color = CreateSolidBrush( ( COLORREF )RGB( 0xF7, 0xF7, 0xF7 ) );
					FillRect( dis->hDC, &dis->rcItem, color );
					DeleteObject( color );
				}

				// Set the selected item's color.
				bool selected = false;
				if ( dis->itemState & ( ODS_FOCUS || ODS_SELECTED ) )
				{
					HBRUSH color = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_HIGHLIGHT ) );
					FillRect( dis->hDC, &dis->rcItem, color );
					DeleteObject( color );
					selected = true;
				}

				wchar_t *buf = L"";

				// This is the full size of the row.
				RECT last_rc;

				// This will keep track of the current colunn's left position.
				int last_left = 0;

				LVCOLUMN lvc = { 0 };
				lvc.mask = LVCF_WIDTH;

				// Loop through all the columns
				for ( int i = 0; i < 2; ++i )
				{
					// Save the appropriate text in our buffer for the current column.
					switch ( i )
					{
						case 0:
						{
							buf = ( ei->sei->windows_property != NULL ? ei->sei->windows_property : L"" );
						}
						break;

						case 1:
						{
							buf = ( ei->property_value != NULL ? ei->property_value : L"" );
						}
						break;
					}

					// Get the dimensions of the listview column
					SendMessage( dis->hwndItem, LVM_GETCOLUMN, i, ( LPARAM )&lvc );

					last_rc = dis->rcItem;

					// This will adjust the text to fit nicely into the rectangle.
					last_rc.left = 5 + last_left;
					last_rc.right = lvc.cx + last_left - 5;

					// Save the last left position of our column.
					last_left += lvc.cx;

					// Save the height and width of this region.
					int width = last_rc.right - last_rc.left;
					if ( width <= 0 )
					{
						continue;
					}

					int height = last_rc.bottom - last_rc.top;

					// Normal text position.
					RECT rc;
					rc.top = 0;
					rc.left = 0;
					rc.right = width;
					rc.bottom = height;

					// Shadow text position.
					//RECT rc2 = rc;
					//rc2.left += 1;
					//rc2.top += 1;
					//rc2.right += 1;
					//rc2.bottom += 1;

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
					if ( selected )
					{
						// Fill the background.
						HBRUSH color = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_HIGHLIGHT ) );
						FillRect( hdcMem, &rc, color );
						DeleteObject( color );

						// Shadow color - black.
						//SetTextColor( hdcMem, RGB( 0x00, 0x00, 0x00 ) );
						//DrawText( hdcMem, buf, -1, &rc2, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS );

						// White text.
						SetTextColor( hdcMem, RGB( 0xFF, 0xFF, 0xFF ) );
						DrawText( hdcMem, buf, -1, &rc, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS );

						BitBlt( dis->hDC, dis->rcItem.left + last_rc.left, last_rc.top, width, height, hdcMem, 0, 0, SRCCOPY );
					}
					else	// Draw normal text.
					{
						// Fill the background.
						HBRUSH color = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_WINDOW ) );
						FillRect( hdcMem, &rc, color );
						DeleteObject( color );

						// Shadow color - light grey.
						//SetTextColor( hdcMem, RGB( 0xE0, 0xE0, 0xE0 ) );
						//DrawText( hdcMem, buf, -1, &rc2, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS );

						// Show red text if our checksums don't match and black for everything else.
						SetTextColor( hdcMem, RGB( 0x00, 0x00, 0x00 ) );
						DrawText( hdcMem, buf, -1, &rc, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS );

						BitBlt( dis->hDC, dis->rcItem.left + last_rc.left, last_rc.top, width, height, hdcMem, 0, 0, SRCAND );
					}

					// Delete our back buffer.
					DeleteDC( hdcMem );
				}
			}
			return TRUE;
		}
		break;

		case WM_SIZE:
		{
			RECT rc;
			GetClientRect( hWnd, &rc );

			// Allow our controls to move in relation to the parent window.
			HDWP hdwp = BeginDeferWindowPos( 2 );
			DeferWindowPos( hdwp, g_hWnd_list_info, HWND_TOP, 20, 40, rc.right - 40, rc.bottom - 90, SWP_NOZORDER );
			DeferWindowPos( hdwp, g_hWnd_btn_close, HWND_TOP, ( ( rc.right - rc.left ) / 2 ) - 40, rc.bottom - 32, 80, 23, SWP_NOZORDER );
			EndDeferWindowPos( hdwp );

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
			SendMessage( g_hWnd_property, WM_CLOSE, 0, 0 );
			ShowWindow( hWnd, SW_HIDE );

			g_current_fi = NULL;	// Reset.

			SendMessageA( g_hWnd_static_mapped_hash, WM_SETTEXT, 0, 0 );
			SendMessage( g_hWnd_list_info, LVM_DELETEALLITEMS, 0, 0 );

			return 0;
		}
		break;

		case WM_ALERT:
		{
			MessageBoxA( hWnd, ( LPCSTR )lParam, PROGRAM_CAPTION_A, MB_APPLMODAL | ( wParam == 1 ? MB_ICONINFORMATION : MB_ICONWARNING ) | MB_SETFOREGROUND );
		}
		break;

		case WM_PROPAGATE:
		{
			if ( wParam == 0 )
			{
				SendMessage( g_hWnd_list_info, LVM_DELETEALLITEMS, 0, 0 );
				SendMessageA( g_hWnd_static_mapped_hash, WM_SETTEXT, 0, 0 );

				// Remove the column formatting.
				LVCOLUMN lvc = { NULL };
				lvc.mask = LVCF_FMT;
				SendMessage( g_hWnd_list_info, LVM_SETCOLUMN, 0, ( LPARAM )&lvc );
				SendMessage( g_hWnd_list_info, LVM_SETCOLUMN, 1, ( LPARAM )&lvc );

				if ( lParam != NULL )
				{
					fileinfo *fi = ( fileinfo * )lParam;

					g_current_fi = fi;

					char chash[ 17 ] = { 0 };
					sprintf_s( chash, 17, "%016llx", fi->mapped_hash );
					SendMessageA( g_hWnd_static_mapped_hash, WM_SETTEXT, 0, ( LPARAM )chash );

					// Insert a row into our listview.
					LVITEM lvi = { NULL };
					lvi.mask = LVIF_PARAM;
					lvi.iSubItem = 0;

					extended_info *t_ei = fi->ei;
					while ( t_ei != NULL )
					{
						lvi.iItem = ( int )SendMessage( g_hWnd_list_info, LVM_GETITEMCOUNT, 0, 0 );
						lvi.lParam = ( LPARAM )t_ei;
						SendMessage( g_hWnd_list_info, LVM_INSERTITEM, 0, ( LPARAM )&lvi );

						t_ei = t_ei->next;
					}
				}

				// WM_SIZE is not sent when first shown. Make sure window positions are set in WM_CREATE.
				SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
			}
			else if ( wParam == 1 )
			{
				// Insert a row into our listview.
				LVITEM lvi = { NULL };
				lvi.mask = LVIF_PARAM;
				lvi.iItem = ( int )SendMessage( g_hWnd_list_info, LVM_GETITEMCOUNT, 0, 0 );
				lvi.iSubItem = 0;
				lvi.lParam = lParam;
				SendMessage( g_hWnd_list_info, LVM_INSERTITEM, 0, ( LPARAM )&lvi );
			}
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
