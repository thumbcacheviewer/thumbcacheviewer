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

#include "lite_user32.h"

pGetDpiForWindow			_GetDpiForWindow;
pSystemParametersInfoForDpi	_SystemParametersInfoForDpi;
pGetSystemMetricsForDpi		_GetSystemMetricsForDpi;

pGetDpiForMonitor			_GetDpiForMonitor;

HMODULE hModule_user32 = NULL;

unsigned char user32_state = 0;	// 0 = Not running, 1 = running.

bool InitializeUser32()
{
	if ( user32_state != USER32_STATE_SHUTDOWN )
	{
		return true;
	}

	SetErrorMode( SEM_FAILCRITICALERRORS );
	hModule_user32 = LoadLibrary( L"user32.dll" );
	SetErrorMode( 0 );

	if ( hModule_user32 != NULL )
	{
		_GetDpiForWindow = ( pGetDpiForWindow )GetProcAddress( hModule_user32, "GetDpiForWindow" );
		if ( _GetDpiForWindow == NULL ) { goto ALTERNATIVE; }

		_SystemParametersInfoForDpi = ( pSystemParametersInfoForDpi )GetProcAddress( hModule_user32, "SystemParametersInfoForDpi" );
		if ( _SystemParametersInfoForDpi == NULL ) { goto ALTERNATIVE; }

		_GetSystemMetricsForDpi = ( pGetSystemMetricsForDpi )GetProcAddress( hModule_user32, "GetSystemMetricsForDpi" );
		if ( _GetSystemMetricsForDpi == NULL ) { goto ALTERNATIVE; }

		goto FINISH;
	}

ALTERNATIVE:

	if ( hModule_user32 != NULL )
	{
		FreeLibrary( hModule_user32 );
		hModule_user32 = NULL;
	}

	SetErrorMode( SEM_FAILCRITICALERRORS );
	hModule_user32 = LoadLibrary( L"Shcore.dll" );
	SetErrorMode( 0 );

	if ( hModule_user32 == NULL )
	{
		return false;
	}

	_GetDpiForMonitor = ( pGetDpiForMonitor )GetProcAddress( hModule_user32, "GetDpiForMonitor" );
	if ( _GetDpiForMonitor == NULL )
	{
		FreeLibrary( hModule_user32 );
		hModule_user32 = NULL;
		return false;
	}

FINISH:

	user32_state = USER32_STATE_RUNNING;

	return true;
}

bool UnInitializeUser32()
{
	if ( user32_state != USER32_STATE_SHUTDOWN )
	{
		user32_state = USER32_STATE_SHUTDOWN;

		return ( FreeLibrary( hModule_user32 ) == FALSE ? false : true );
	}

	return true;
}

UINT gdfw_dpi = 0;

UINT GetDpiForWindow( HWND hwnd )
{
	if ( _GetDpiForWindow )	// Per-monitor DPI support.
	{
		UINT dpi = _GetDpiForWindow( hwnd );
		if ( dpi == 0 ) { goto ALTERNATIVE; }
		return dpi;
	}
	else if ( _GetDpiForMonitor )	// If we're not on Windows 10+, then this is available on Windows 8.1.
	{
		UINT x = 0;
		UINT y;

		HMONITOR hMon = MonitorFromWindow( hwnd, MONITOR_DEFAULTTONEAREST );
		if ( _GetDpiForMonitor( hMon, MDT_EFFECTIVE_DPI, &x, &y ) == S_OK && x != 0 )
		{
			return x;
		}
	}

ALTERNATIVE:

	// No per-monitor support. Program won't scale when moving across monitors that have differnt DPI values.
	if ( gdfw_dpi == 0 )
	{
		HDC hDC = GetDC( NULL );
		gdfw_dpi = GetDeviceCaps( hDC, LOGPIXELSX );
		ReleaseDC( NULL, hDC );

		if ( gdfw_dpi == 0 )
		{
			gdfw_dpi = USER_DEFAULT_SCREEN_DPI;
		}
	}

	return gdfw_dpi;
}

BOOL SystemParametersInfoForDpi( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi )
{
	if ( _SystemParametersInfoForDpi )
	{
		return _SystemParametersInfoForDpi( uiAction, uiParam, pvParam, fWinIni, dpi );
	}
	else
	{
		return SystemParametersInfo( uiAction, uiParam, pvParam, fWinIni );
	}
}

int GetSystemMetricsForDpi( int nIndex, UINT dpi )
{
	if ( _GetSystemMetricsForDpi )
	{
		return _GetSystemMetricsForDpi( nIndex, dpi );
	}
	else
	{
		return GetSystemMetrics( nIndex );
	}
}
