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

#ifndef _LITE_USER32_H
#define _LITE_USER32_H

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef enum MONITOR_DPI_TYPE
{
	MDT_EFFECTIVE_DPI = 0,
	MDT_ANGULAR_DPI = 1,
	MDT_RAW_DPI = 2,
	MDT_DEFAULT
};

#define USER32_STATE_SHUTDOWN	0
#define USER32_STATE_RUNNING	1

typedef UINT ( WINAPI *pGetDpiForWindow )( HWND hwnd );
typedef BOOL ( WINAPI *pSystemParametersInfoForDpi )( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi );
typedef int ( WINAPI *pGetSystemMetricsForDpi )( int nIndex, UINT dpi );

typedef HRESULT ( WINAPI *pGetDpiForMonitor )( HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT *dpiX, UINT *dpiY );

extern pGetDpiForWindow				_GetDpiForWindow;
extern pSystemParametersInfoForDpi	_SystemParametersInfoForDpi;
extern pGetSystemMetricsForDpi		_GetSystemMetricsForDpi;

extern pGetDpiForMonitor			_GetDpiForMonitor;

extern unsigned char user32_state;

bool InitializeUser32();
bool UnInitializeUser32();

UINT GetDpiForWindow( HWND hwnd );
BOOL SystemParametersInfoForDpi( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi );
int GetSystemMetricsForDpi( int nIndex, UINT dpi );

#endif
