/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2015 Eric Kutcher

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

// Extensible Storage Engine library.
#pragma comment( lib, "esent.lib" )

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

#define PROGRAM_CAPTION		L"Thumbcache Viewer"
#define PROGRAM_CAPTION_A	"Thumbcache Viewer"

#define MIN_WIDTH			480
#define MIN_HEIGHT			320

#define NUM_COLUMNS			11

#define WM_PROPAGATE		WM_APP		// Updates the scan window.
#define WM_DESTROY_ALT		WM_APP + 1	// Allows non-window threads to call DestroyWindow.
#define WM_CHANGE_CURSOR	WM_APP + 2	// Updates the window cursor.
#define WM_ALERT			WM_APP + 3	// Called from threads to display a message box.

// fileinfo flags.
#define FIF_TYPE_BMP		1
#define FIF_TYPE_JPG		2
#define FIF_TYPE_PNG		4
#define FIF_VERIFIED_HEADER	8
#define FIF_BAD_HEADER		16
#define FIF_BAD_DATA		32

// Holds shared variables among database entries.
struct shared_info
{
	wchar_t dbpath[ MAX_PATH ];			// Path to the database file.
	unsigned int system;				// 0x14 = Windows Vista, 0x15 = Windows 7, 0x1A & 0x1C = Windows 8

	unsigned long count;				// Number of directory entries.
};

struct shared_extended_info
{
	wchar_t *windows_property;

	unsigned long count;				// Number of Windows properties.
};

// Information retrieved from Windows.edb.
struct extended_info
{
	shared_extended_info *sei;		// Shared information between items.
	wchar_t *property_value;		// Converted data value.
	extended_info *next;
};

// This structure holds information obtained as we read the database. It's passed as an lParam to our listview items.
struct fileinfo
{
	unsigned long long entry_hash;		// Entry hash
	unsigned long long data_checksum;	// Data checksum
	unsigned long long header_checksum;	// Header checksum
	unsigned long long v_data_checksum;	// Verified data checksum
	unsigned long long v_header_checksum;	// Verified header checksum
	unsigned long long mapped_hash;		// Entry hash or Vista's filename integer representation.
	wchar_t *filename;					// Name of the database entry.
	shared_info *si;					// Shared information between items in a database.
	extended_info *ei;					// Information retrieved from Windows.edb
	unsigned int header_offset;			// Offset of header.
	unsigned int data_offset;			// Offset of data.
	unsigned int size;					// Size of file.
	unsigned char flag;					// 1 = bmp, 2 = jpg, 4 = png, 8 = in tree, 16 = verified headers, 32 = bad header, 64 = bad data.
};

// Holds duplicate and blank entries.
struct linked_list
{
	fileinfo *fi;						// Refer to the file info so we can add it to the listview.
	linked_list *next;
};

// Multi-file open structure.
struct pathinfo
{
	wchar_t *filepath;			// Path to the file/folder
	wchar_t *output_path;		// If the user wants to save files.
	unsigned short offset;		// Offset to the first file.
	unsigned char type;			// 0 = Save thumbnails, 1 = Save CSV.
};

// Save To structure.
struct save_param
{
	wchar_t *filepath;		// Save directory.
	unsigned char type;		// 0 = full path, 1 = build directory
	bool save_all;			// Save All = true, Save Selected = false.
};

// Function prototypes
LRESULT CALLBACK MainWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK ImageWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK ScanWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK InfoWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK PropertyWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

VOID CALLBACK TimerProc( HWND hWnd, UINT msg, UINT idTimer, DWORD dwTime );

// These are all variables that are shared among the separate .cpp files.

// Object handles.
extern HWND g_hWnd_main;			// Handle to our main window.
extern HWND g_hWnd_image;			// Handle to our image window.
extern HWND g_hWnd_scan;			// Handle to our scan window.
extern HWND g_hWnd_info;			// Handle to our information window.
extern HWND g_hWnd_property;		// Handle to our property window.
extern HWND g_hWnd_list;			// Handle to the listview control.
extern HWND g_hWnd_list_info;		// Handle to the extended info listview control.
extern HWND g_hWnd_active;			// Handle to the active window. Used to handle tab stops.

extern CRITICAL_SECTION pe_cs;		// Allows various GUI processes (open, save, etc.) to be executed one at a time.

extern HFONT hFont;					// Handle to the system's message font.

extern int row_height;				// Height of our listview rows.

extern HICON hIcon_bmp;				// Handle to the system's .bmp icon.
extern HICON hIcon_jpg;				// Handle to the system's .jpg icon.
extern HICON hIcon_png;				// Handle to the system's .png icon.

extern HCURSOR wait_cursor;			// Temporary cursor while processing entries.

// Window variables
extern RECT last_dim;				// Keeps track of the image window's dimension before it gets minimized.

extern bool is_attached;			// Toggled when our windows are attached
extern bool skip_main;				// Prevents the main window from moving the image window if it is about to attach.

extern char cmd_line;				// Show the main window and message prompts.

// Image variables
extern Gdiplus::Image *gdi_image;	// GDI+ image object. We need it to handle .png and .jpg images specifically.

extern POINT drag_rect;				// The current position of gdi_image in the image window.
extern POINT old_pos;				// The old position of gdi_image. Used to calculate the rate of change.

extern float scale;					// Scale of the image.

// List variables
extern bool hide_blank_entries;		// Hide blank entries.

extern bool is_kbytes_c_offset;		// Toggle the cache entry offset text.
extern bool is_kbytes_c_size;		// Toggle the cache entry size text.
extern bool is_kbytes_d_offset;		// Toggle the data offset text.
extern bool is_kbytes_d_size;		// Toggle the data size text.
extern bool is_dc_lower;			// Toggle the data checksum text
extern bool is_hc_lower;			// Toggle the header checksum text.
extern bool is_eh_lower;			// Toggle the entry hash text.

// Thread variables
extern bool g_kill_thread;			// Allow for a clean shutdown.
extern bool g_kill_scan;			// Stop a file scan.

extern bool in_thread;				// Flag to indicate that we're in a worker thread.
extern bool skip_draw;				// Prevents WM_DRAWITEM from accessing listview items while we're removing them.

#endif
