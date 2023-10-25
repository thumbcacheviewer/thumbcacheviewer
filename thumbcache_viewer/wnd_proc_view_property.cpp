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
#include "utilities.h"

#include "lite_user32.h"

HWND g_hWnd_edit_property_value = NULL;

UINT current_dpi_property = 0;//, last_dpi_property = USER_DEFAULT_SCREEN_DPI;

HFONT hFont_property = NULL;

LRESULT CALLBACK PropertyWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch ( msg )
	{
		case WM_CREATE:
		{
			current_dpi_property = GetDpiForWindow( hWnd );
			hFont_property = UpdateFontsAndMetrics( current_dpi_property, /*last_dpi_property,*/ NULL );

			RECT rc;
			GetClientRect( hWnd, &rc );

			g_hWnd_edit_property_value = CreateWindow( WC_EDIT, NULL, ES_MULTILINE | ES_READONLY | WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL, 0, 0, rc.right, rc.bottom, hWnd, NULL, NULL, NULL );

			// Make pretty font.
			SendMessage( g_hWnd_edit_property_value, WM_SETFONT, ( WPARAM )hFont_property, 0 );

			return 0;
		}
		break;

		case WM_SIZE:
		{
			RECT rc;
			GetClientRect( hWnd, &rc );

			// Allow our controls to move in relation to the parent window.
			HDWP hdwp = BeginDeferWindowPos( 1 );
			DeferWindowPos( hdwp, g_hWnd_edit_property_value, HWND_TOP, 0, 0, rc.right, rc.bottom, 0 );
			EndDeferWindowPos( hdwp );

			return 0;
		}
		break;

		case WM_CTLCOLORSTATIC:
		{
			return ( LRESULT )( GetSysColorBrush( COLOR_WINDOW ) );
		}
		break;

		case WM_DPICHANGED:
		{
			//last_dpi_property = current_dpi_property;
			current_dpi_property = HIWORD( wParam );
			HFONT hFont = UpdateFontsAndMetrics( current_dpi_property, /*last_dpi_property,*/ NULL );
			EnumChildWindows( hWnd, EnumChildProc, ( LPARAM )hFont );
			DeleteObject( hFont_property );
			hFont_property = hFont;

			RECT *rc = ( RECT * )lParam;
			SetWindowPos( hWnd, NULL, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, SWP_NOZORDER | SWP_NOACTIVATE );

			return 0;
		}
		break;

		case WM_CLOSE:
		{
			ShowWindow( hWnd, SW_HIDE );

			SendMessage( g_hWnd_edit_property_value, WM_SETTEXT, 0, 0 );

			return 0;
		}
		break;

		case WM_DESTROY:
		{
			// Delete our font.
			DeleteObject( hFont_property );

			return 0;
		}
		break;

		case WM_PROPAGATE:
		{
			SendMessage( g_hWnd_edit_property_value, WM_SETTEXT, 0, 0 );
			if ( lParam != NULL )
			{
				EXTENDED_INFO *ei = ( EXTENDED_INFO * )lParam;

				SendMessage( g_hWnd_edit_property_value, WM_SETTEXT, 0, ( LPARAM )ei->property_value );
				SetWindowText( hWnd, ( ei->sei != NULL ? ei->sei->windows_property : L"Property Value" ) );
			}
			else
			{
				SetWindowText( hWnd, L"Property Value" );
			}

			// WM_SIZE is not sent when first shown. Make sure window positions are set in WM_CREATE.
			SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );

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
