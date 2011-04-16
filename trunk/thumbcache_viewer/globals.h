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
#ifndef _GLOBALS_H
#define _GLOBALS_H

// Pretty window.
#pragma comment( linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"" )

// Include GDI+ support. We need it to draw .jpg and .png images, but we'll use it for .bmp too.
#pragma comment( lib, "gdiplus.lib" )

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <wchar.h>
#include <gdiplus.h>

#include "resource.h"

#define PROGRAM_CAPTION L"Thumbcache Viewer"

#define MIN_WIDTH		480
#define MIN_HEIGHT		320

#define FILE_TYPE_BMP	"BM"
#define FILE_TYPE_JPEG	"\xFF\xD8\xFF\xE0"
#define FILE_TYPE_PNG	"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"

#define MENU_OPEN		1001
#define MENU_SAVE_ALL	1002
#define MENU_SAVE_SEL	1003
#define MENU_EXIT		1004
#define MENU_ABOUT		1005
#define MENU_SELECT_ALL	1006
#define MENU_REMOVE_SEL	1007

#define IDT_TIMER		2001

#define WINDOWS_VISTA	0x14
#define WINDOWS_7		0x15

#define MAX_SCALE_SIZE	25.0f

#define MIN_COLUMN_SIZE	18

// Thumbcache header information.
struct database_header
{
	char magic_identifier[ 4 ];
	unsigned int version;
	unsigned int type;
	unsigned int first_cache_entry;
	unsigned int available_cache_entry;
	unsigned int number_of_cache_entries;
};

// Window 7 Thumbcache entry.
struct database_cache_entry_7
{
	char magic_identifier[ 4 ];
	unsigned int cache_entry_size;
	long long entry_hash;
	unsigned int filename_length;
	unsigned int padding_size;
	unsigned int data_size;
	unsigned int unknown;
	long long data_checksum;
	long long header_checksum;
};

// Windows Vista Thumbcache entry.
struct database_cache_entry_vista
{
	char magic_identifier[ 4 ];
	unsigned int cache_entry_size;
	long long entry_hash;
	wchar_t extension[ 4 ];
	unsigned int filename_length;
	unsigned int padding_size;
	unsigned int data_size;
	unsigned int unknown;
	long long data_checksum;
	long long header_checksum;
};

// This structure holds information obtained as we read the database. It's passed as an lparam to our listview items.
struct fileinfo
{
	unsigned int offset;				// Offset in database.
	unsigned int size;					// Size of file.
	wchar_t dbpath[ MAX_PATH + 1 ];		// Path to the database file.
	unsigned int system;				// 0x14 = Windows Vista, 0x15 = Windows 7
	char extension;						// 0 = bmp, 1 = jpg, 2 = png
	wchar_t *filename;					// Name of the database entry.
	long long entry_hash;				// Entry hash
	long long data_checksum;			// Data checksum
	long long header_checksum;			// Header checksum
};

// Function prototypes
LRESULT CALLBACK MainWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK EditSubProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

LRESULT CALLBACK ImageWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
VOID CALLBACK TimerProc( HWND hWnd, UINT msg, UINT idTimer, DWORD dwTime );

bool is_close( int a, int b );
void read_database( wchar_t &filepath );

// These are all variables that are shared among the separate .cpp files.

// Object handles.
extern HWND g_hWnd_main;			// Handle to our main window.
extern HWND g_hWnd_image;			// Handle to our image window.
extern HWND g_hWnd_list;			// Handle to the listview control.

extern HFONT hFont;					// Handle to the system's message font.

extern HICON hIcon_bmp;				// Handle to the system's .bmp icon.
extern HICON hIcon_jpg;				// Handle to the system's .jpg icon.
extern HICON hIcon_png;				// Handle to the system's .png icon.

extern HMENU g_hMenu;				// Handle to our menu bar.
extern HMENU g_hMenuSub_context;	// Handle to our context menu.

// Window variables
extern RECT last_dim;				// Keeps track of the image window's dimension before it gets minimized.

extern bool is_attached;			// Toggled when our windows are attached
extern bool skip_main;				// Prevents the main window from moving the image window if it is about to attach.

// Image variables
extern char *current_image;			// Buffer that stores our image and is used to write our files.
extern Gdiplus::Image *gdi_image;	// GDI+ image object. We need it to handle .png and .jpg images specifically.

extern POINT drag_rect;				// The current position of gdi_image in the image window.
extern POINT old_pos;				// The old position of gdi_image. Used to calculate the rate of change.

extern float scale;					// Scale of the image.

#endif
