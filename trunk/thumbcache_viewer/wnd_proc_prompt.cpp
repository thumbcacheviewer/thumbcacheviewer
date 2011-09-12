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

#define BTN_OK	1000

HWND g_hWnd_begin = NULL;
HWND g_hWnd_end = NULL;

HWND g_hWnd_up_down1 = NULL;
HWND g_hWnd_up_down2 = NULL;

LRESULT CALLBACK PromptWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch ( msg )
    {
		case WM_CREATE:
		{
			RECT rc;
			GetClientRect( hWnd, &rc );

			HWND g_hWnd_static_msg = CreateWindow( WC_STATIC, L"Please select an appropriate range to load.", WS_CHILD | WS_VISIBLE, 5, 5, 470, 15, hWnd, NULL, NULL, NULL );

			HWND g_hWnd_static1 = CreateWindow( WC_STATIC, L"Beginning:", WS_CHILD | WS_VISIBLE, 5, 25, 50, 15, hWnd, NULL, NULL, NULL );
			g_hWnd_begin = CreateWindow( WC_EDIT, NULL, ES_NUMBER | WS_CHILD | WS_VISIBLE, 5, 40, 50, 20, hWnd, NULL, NULL, NULL );
			g_hWnd_up_down1 = CreateWindow( UPDOWN_CLASS, NULL, UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS | UDS_SETBUDDYINT | WS_CHILD | WS_VISIBLE, 55, 40, 20, 20, hWnd, NULL, NULL, NULL );
			SendMessage( g_hWnd_up_down1, UDM_SETBUDDY, ( WPARAM )g_hWnd_begin, 0 );
			SendMessage( g_hWnd_begin, EM_LIMITTEXT, 10, 0 );
			SendMessage( g_hWnd_up_down1, UDM_SETBASE, 10, 0 );

			HWND g_hWnd_static2 = CreateWindow( WC_STATIC, L"End:", WS_CHILD | WS_VISIBLE, 100, 25, 50, 15, hWnd, NULL, NULL, NULL );
			g_hWnd_end = CreateWindow( WC_EDIT, NULL, ES_NUMBER | WS_CHILD | WS_VISIBLE, 100, 40, 50, 20, hWnd, NULL, NULL, NULL );
			g_hWnd_up_down2 = CreateWindow( UPDOWN_CLASS, NULL, UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS | UDS_SETBUDDYINT | WS_CHILD | WS_VISIBLE, 150, 40, 20, 20, hWnd, NULL, NULL, NULL );
			SendMessage( g_hWnd_up_down2, UDM_SETBUDDY, ( WPARAM )g_hWnd_end, 0 );
			SendMessage( g_hWnd_end, EM_LIMITTEXT, 10, 0 );
			SendMessage( g_hWnd_up_down2, UDM_SETBASE, 10, 0 );

			HWND g_hWnd_ok = CreateWindow( WC_BUTTON, L"OK", WS_CHILD | WS_TABSTOP | WS_VISIBLE, ( rc.right - rc.left - ( GetSystemMetrics( SM_CXMINTRACK ) - ( 2 * GetSystemMetrics( SM_CXSIZEFRAME ) ) ) ) / 2, rc.bottom - 25, GetSystemMetrics( SM_CXMINTRACK ) - ( 2 * GetSystemMetrics( SM_CXSIZEFRAME ) ), 20, hWnd, ( HMENU )BTN_OK, NULL, NULL );

			// Make pretty font.
			SendMessage( g_hWnd_static_msg, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_static1, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_static2, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_up_down1, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_up_down2, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_begin, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_end, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_ok, WM_SETFONT, ( WPARAM )hFont, 0 );

			return 0;
		}
		break;

		case WM_COMMAND:
		{
			switch( LOWORD( wParam ) )
			{
				case BTN_OK:
				{
					char value[ 11 ] = { 0 };
					SendMessageA( g_hWnd_begin, WM_GETTEXT, 11, ( LPARAM )value );
					entry_begin = strtol( value, NULL, 10 );
					SendMessageA( g_hWnd_end, WM_GETTEXT, 11, ( LPARAM )value );
					entry_end = strtol( value, NULL, 10 );

					if ( entry_begin == 0 || entry_end == 0 )
					{
						MessageBox( g_hWnd_main, L"The ending and starting positions cannot be zero.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
						break;
					}
					else if ( entry_begin > entry_end )
					{
						MessageBox( g_hWnd_main, L"The beginning position cannot be greater than the ending position.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
						break;
					}

					if ( prompt_mutex != NULL )
					{
						ReleaseSemaphore( prompt_mutex, 1, NULL );
					}

					EnableWindow( g_hWnd_main, TRUE );

					ShowWindow( hWnd, SW_HIDE );
				}
				break;
			}

			return 0;
		}
		break;

		case WM_CLOSE:
		{
			cancelled_prompt = true;

			if ( prompt_mutex != NULL )
			{
				ReleaseSemaphore( prompt_mutex, 1, NULL );
			}

			EnableWindow( g_hWnd_main, TRUE );

			ShowWindow( hWnd, SW_HIDE );
		}
		break;

		case WM_PROPAGATE:
		{
			SendMessage( g_hWnd_up_down1, UDM_SETRANGE32, 1, ( ( unsigned int )wParam > INT_MAX ? INT_MAX : ( int )wParam ) );
			SendMessage( g_hWnd_up_down2, UDM_SETRANGE32, 1, ( ( unsigned int )wParam > INT_MAX ? INT_MAX : ( int )wParam ) );

			SendMessage( g_hWnd_up_down1, UDM_SETPOS, 0, 1 );
			SendMessage( g_hWnd_up_down2, UDM_SETPOS, 0, 2048 );

			EnableWindow( g_hWnd_main, FALSE );

			// Set the window above all other windows.
			SetForegroundWindow( hWnd );
			SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
			ShowWindow( hWnd, SW_SHOW );
		}
		break;

		case WM_DESTROY:
		{
			if ( prompt_mutex != NULL )
			{
				ReleaseSemaphore( prompt_mutex, 1, NULL );
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
