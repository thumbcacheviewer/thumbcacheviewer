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

#ifndef UTILITIES_H
#define UTILITIES_H

#include "globals.h"
#include "dllrbt.h"

#define SNAP_WIDTH		10		// The minimum distance at which our windows will attach together.

#define is_close( a, b ) ( abs( ( a ) - ( b ) ) < SNAP_WIDTH )

#define ntohl( i ) ( ( ( ( unsigned long )( i ) & 0xFF000000 ) >> 24 ) | \
					 ( ( ( unsigned long )( i ) & 0x00FF0000 ) >> 8 ) | \
					 ( ( ( unsigned long )( i ) & 0x0000FF00 ) << 8 ) | \
					 ( ( ( unsigned long )( i ) & 0x000000FF ) << 24 ) )

#define ntohll( i ) ( ( ( __int64 )ntohl( i & 0xFFFFFFFFU ) << 32 ) | ntohl( ( __int64 )( i >> 32 ) ) )

unsigned __stdcall cleanup( void *pArguments );
unsigned __stdcall remove_items( void *pArguments );
unsigned __stdcall show_hide_items( void *pArguments );
unsigned __stdcall verify_checksums( void *pArguments );
unsigned __stdcall save_csv( void *pArguments );
unsigned __stdcall save_items( void *pArguments );
unsigned __stdcall copy_items( void *pArguments );

wchar_t *GetExtensionFromFilename( wchar_t *filename, unsigned long length );
wchar_t *GetFilenameFromPath( wchar_t *path, unsigned long length );
wchar_t *GetFileAttributesStr( unsigned long fa_flags );
wchar_t *GetSFGAOStr( unsigned long sfgao_flags );

void CleanupBlankEntries();
void CreateFileinfoTree();
void CleanupFileinfoTree();
void CleanupExtendedInfo( EXTENDED_INFO *ei );

void Processing_Window( bool enable );

extern HANDLE g_shutdown_semaphore;		// Blocks shutdown while a worker thread is active.
extern LINKED_LIST *g_be;				// A list to hold all of the blank entries.
extern dllrbt_tree *g_file_info_tree;	// Red-black tree of FILE_INFO structures.

extern FILE_INFO *g_current_fi;			// If we removed an entry and the info window is showing, then close the info window.

#endif
