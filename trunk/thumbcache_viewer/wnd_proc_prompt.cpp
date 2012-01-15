/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2012 Eric Kutcher

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

HWND g_hWnd_static_entries = NULL;

HWND g_hWnd_begin = NULL;
HWND g_hWnd_end = NULL;

LRESULT CALLBACK PromptWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch ( msg )
    {
		case WM_CREATE:
		{
			RECT rc;
			GetClientRect( hWnd, &rc );

			g_hWnd_static_entries = CreateWindow( WC_STATIC, L"", SS_CENTER | WS_CHILD | WS_VISIBLE, 5, 5, rc.right - 10, 30, hWnd, NULL, NULL, NULL );

			HWND g_hWnd_static1 = CreateWindow( WC_STATIC, L"Beginning:", WS_CHILD | WS_VISIBLE, 20, 45, 50, 15, hWnd, NULL, NULL, NULL );
			g_hWnd_begin = CreateWindowEx( WS_EX_CLIENTEDGE, WC_EDIT, NULL, ES_NUMBER | WS_CHILD | WS_VISIBLE, 20, 60, 85, 20, hWnd, NULL, NULL, NULL );
			SendMessage( g_hWnd_begin, EM_LIMITTEXT, 10, 0 );	// Limit the text to 10 characters.

			HWND g_hWnd_static2 = CreateWindow( WC_STATIC, L"End:", WS_CHILD | WS_VISIBLE, 135, 45, 50, 15, hWnd, NULL, NULL, NULL );
			g_hWnd_end = CreateWindowEx( WS_EX_CLIENTEDGE, WC_EDIT, NULL, ES_NUMBER | WS_CHILD | WS_VISIBLE, 135, 60, 85, 20, hWnd, NULL, NULL, NULL );
			SendMessage( g_hWnd_end, EM_LIMITTEXT, 10, 0 );	// Limit the text to 10 characters.

			HWND g_hWnd_ok = CreateWindow( WC_BUTTON, L"OK", WS_CHILD | WS_TABSTOP | WS_VISIBLE, ( rc.right - rc.left - ( GetSystemMetrics( SM_CXMINTRACK ) - ( 2 * GetSystemMetrics( SM_CXSIZEFRAME ) ) ) ) / 2, rc.bottom - 25, GetSystemMetrics( SM_CXMINTRACK ) - ( 2 * GetSystemMetrics( SM_CXSIZEFRAME ) ), 20, hWnd, ( HMENU )BTN_OK, NULL, NULL );

			// Make pretty font.
			SendMessage( g_hWnd_static_entries, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_static1, WM_SETFONT, ( WPARAM )hFont, 0 );
			SendMessage( g_hWnd_static2, WM_SETFONT, ( WPARAM )hFont, 0 );
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
					// Get the unsigned integer value of the text boxes.
					char value[ 11 ] = { 0 };
					SendMessageA( g_hWnd_begin, WM_GETTEXT, 11, ( LPARAM )value );
					entry_begin = strtoul( value, NULL, 10 );
					SendMessageA( g_hWnd_end, WM_GETTEXT, 11, ( LPARAM )value );
					entry_end = strtoul( value, NULL, 10 );

					// Verify that they're in an appropriate range.
					if ( entry_begin <= 0 || entry_end <= 0 )
					{
						MessageBox( g_hWnd_main, L"The beginning and ending positions must be greater than zero.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
						break;
					}
					else if ( entry_begin > entry_end )
					{
						MessageBox( g_hWnd_main, L"The beginning cannot be greater than the ending position.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
						break;
					}

					// Release the mutex.
					if ( prompt_mutex != NULL )
					{
						ReleaseSemaphore( prompt_mutex, 1, NULL );
					}

					// Reenable the main window.
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
			// The user cancelled the prompt. The database will not be loaded.
			cancelled_prompt = true;

			// Release the mutex.
			if ( prompt_mutex != NULL )
			{
				ReleaseSemaphore( prompt_mutex, 1, NULL );
			}

			// Reenable the main window.
			EnableWindow( g_hWnd_main, TRUE );

			ShowWindow( hWnd, SW_HIDE );
		}
		break;

		case WM_PROPAGATE:
		{
			// Inform the user of the total number of entries.
			wchar_t msg[ 91 ] = { 0 };
			swprintf_s( msg, 91, L"There are %lu entries in the database.\r\nPlease select an appropriate range to load.", ( unsigned int )wParam );
			SendMessage( g_hWnd_static_entries, WM_SETTEXT, 0, ( LPARAM )msg );

			wchar_t value[ 11 ] = { 0 };
			swprintf_s( value, 11, L"%lu", MAX_ENTRIES );

			// Set the beginning and ending position.
			SendMessage( g_hWnd_begin, WM_SETTEXT, 0, ( LPARAM )L"1" );
			SendMessage( g_hWnd_end, WM_SETTEXT, 0, ( LPARAM )value );

			// Disable the main window.
			EnableWindow( g_hWnd_main, FALSE );

			// Set the window above all other windows.
			SetForegroundWindow( hWnd );
			SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
			ShowWindow( hWnd, SW_SHOW );
		}
		break;

		case WM_DESTROY:
		{
			// Release the mutex.
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
