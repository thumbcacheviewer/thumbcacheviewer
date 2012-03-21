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
#include <process.h>

#include "resource.h"

#define PROGRAM_CAPTION L"Thumbcache Viewer"

#define MIN_WIDTH		480
#define MIN_HEIGHT		320

#define MENU_OPEN		1001
#define MENU_SAVE_ALL	1002
#define MENU_SAVE_SEL	1003
#define MENU_EXIT		1004
#define MENU_ABOUT		1005
#define MENU_SELECT_ALL	1006
#define MENU_REMOVE_SEL	1007
#define MENU_HIDE_BLANK	1008

#define WINDOWS_VISTA	0x14
#define WINDOWS_7		0x15
#define WINDOWS_8		0x1A
#define WINDOWS_8v2		0x1C

#define MAX_ENTRIES		10240	// The maximum amount of entries we want to display. Anything above this will result in a user prompt. Should not be greater than UINT_MAX.

#define SNAP_WIDTH		10;		// The minimum distance at which our windows will attach together.

#define WM_PROPAGATE		WM_APP		// Updates the prompt window.
#define WM_DESTROY_ALT		WM_APP + 1	// Allows non-window threads to call DestroyWindow.
#define WM_CHANGE_CURSOR	WM_APP + 2	// Updates the window cursor.

// Thumbcache header information.
struct database_header
{
	char magic_identifier[ 4 ];
	unsigned int version;
	unsigned int type;	// Windows Vista & 7: 00 = 32, 01 = 96, 02 = 256, 03 = 1024, 04 = sr // Windows 8: 00 = 16, 01 = 32, 02 = 48, 03 = 96, 04 = 256, 05 = 1024, 06 = sr, 07 = wide, 08 = exif
};

// Found in everything but WINDOWS_8v2 databases.
struct database_header_entry_info
{
	unsigned int first_cache_entry;
	unsigned int available_cache_entry;
	unsigned int number_of_cache_entries;
};

// Found in WINDOWS_8v2 databases.
struct database_header_entry_info_v2
{
	unsigned int unknown;
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

// Window 8 Thumbcache entry.
struct database_cache_entry_8
{
	char magic_identifier[ 4 ];
	unsigned int cache_entry_size;
	long long entry_hash;
	unsigned int filename_length;
	unsigned int padding_size;
	unsigned int data_size;
	unsigned int width;
	unsigned int height;
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

// Holds shared variables among database entries.
struct shared_info
{
	wchar_t dbpath[ MAX_PATH ];			// Path to the database file.
	unsigned int system;				// 0x14 = Windows Vista, 0x15 = Windows 7

	unsigned long count;				// Number of directory entries.
};

// This structure holds information obtained as we read the database. It's passed as an lParam to our listview items.
struct fileinfo
{
	unsigned int offset;				// Offset in database.
	unsigned int size;					// Size of file.
	char extension;						// 0 = bmp, 1 = jpg, 2 = png
	wchar_t *filename;					// Name of the database entry.
	long long entry_hash;				// Entry hash
	long long data_checksum;			// Data checksum
	long long header_checksum;			// Header checksum

	shared_info *si;					// Shared information between items in a database.
};

// Temporary list for blank entries.
struct blank_entries_linked_list
{
	fileinfo *fi;						// Refer to the file info so we can add it to the listview.
	blank_entries_linked_list *next;
};

// Multi-file open structure.
struct pathinfo
{
	wchar_t *filepath;			// Path to the file/folder
	unsigned short offset;		// Offset to the first file.
};

// Save To structure.
struct save_param
{
	LPITEMIDLIST lpiidl;	// BrowseForFolder variable when saving files.
	bool save_all;			// Save All = true, Save Selected = false.
};

// Function prototypes
LRESULT CALLBACK MainWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

LRESULT CALLBACK ImageWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
VOID CALLBACK TimerProc( HWND hWnd, UINT msg, UINT idTimer, DWORD dwTime );

LRESULT CALLBACK PromptWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

unsigned __stdcall cleanup( void *pArguments );
unsigned __stdcall read_database( void *pArguments );
unsigned __stdcall remove_items( void *pArguments );
unsigned __stdcall show_hide_items( void *pArguments );
unsigned __stdcall save_items( void *pArguments );
bool is_close( int a, int b );
void update_menus( bool disable_all );

// These are all variables that are shared among the separate .cpp files.

// Object handles.
extern HWND g_hWnd_main;			// Handle to our main window.
extern HWND g_hWnd_image;			// Handle to our image window.
extern HWND g_hWnd_prompt;			// Handle to our prompt window.
extern HWND g_hWnd_list;			// Handle to the listview control.

extern CRITICAL_SECTION pe_cs;		// Allows various GUI processes (open, save, etc.) to be executed one at a time.
extern HANDLE prompt_mutex;			// Blocks read_database() until the g_hWnd_prompt is destroyed.

extern HFONT hFont;					// Handle to the system's message font.

extern HICON hIcon_bmp;				// Handle to the system's .bmp icon.
extern HICON hIcon_jpg;				// Handle to the system's .jpg icon.
extern HICON hIcon_png;				// Handle to the system's .png icon.

extern HMENU g_hMenu;				// Handle to our menu bar.
extern HMENU g_hMenuSub_context;	// Handle to our context menu.

extern bool cancelled_prompt;		// User cancelled the prompt.
extern unsigned int entry_begin;	// Beginning position to start reading.
extern unsigned int entry_end;		// Ending position to stop reading.

// Window variables
extern RECT last_dim;				// Keeps track of the image window's dimension before it gets minimized.

extern bool is_attached;			// Toggled when our windows are attached
extern bool skip_main;				// Prevents the main window from moving the image window if it is about to attach.

// Image variables
extern Gdiplus::Image *gdi_image;	// GDI+ image object. We need it to handle .png and .jpg images specifically.

extern POINT drag_rect;				// The current position of gdi_image in the image window.
extern POINT old_pos;				// The old position of gdi_image. Used to calculate the rate of change.

extern float scale;					// Scale of the image.

extern blank_entries_linked_list *g_be;	// A list to hold all of the blank entries.
extern bool hide_blank_entries;		// Hide blank entries.

// Thread variables
extern bool kill_thread;			// Allow for a clean shutdown.

extern bool in_thread;				// Flag to indicate that we're in a worker thread.
extern bool skip_draw;				// Prevents WM_DRAWITEM from accessing listview items while we're removing them.

#endif
