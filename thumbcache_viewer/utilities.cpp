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
#include "read_thumbcache.h"
#include "menus.h"
#include "map_entries.h"
#include "crc64.h"

#include "lite_user32.h"

#include <stdio.h>

HANDLE g_shutdown_semaphore = NULL;		// Blocks shutdown while a worker thread is active.
bool g_kill_thread = false;				// Allow for a clean shutdown.

CRITICAL_SECTION pe_cs;					// Queues additional worker threads.
bool in_thread = false;					// Flag to indicate that we're in a worker thread.
bool skip_draw = false;					// Prevents WM_DRAWITEM from accessing listview items while we're removing them.

LINKED_LIST *g_be = NULL;				// A list to hold all of the blank entries.

dllrbt_tree *g_file_info_tree = NULL;	// Red-black tree of FILE_INFO structures.

void Processing_Window( bool enable )
{
	if ( enable )
	{
		SetWindowTextA( g_hWnd_main, "Thumbcache Viewer - Please wait..." );	// Update the window title.
		EnableWindow( g_hWnd_list, FALSE );										// Prevent any interaction with the listview while we're processing.
		SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, TRUE, 0 );					// SetCursor only works from the main thread. Set it to an arrow with hourglass.
		UpdateMenus( UM_DISABLE );												// Disable all processing menu items.
	}
	else
	{
		UpdateMenus( UM_ENABLE );								// Enable all processing menu items.
		SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, FALSE, 0 );	// Reset the cursor.
		EnableWindow( g_hWnd_list, TRUE );						// Allow the listview to be interactive. Also forces a refresh to update the item count column.
		SetFocus( g_hWnd_list );								// Give focus back to the listview to allow shortcut keys.
		SetWindowTextA( g_hWnd_main, PROGRAM_CAPTION_A );		// Reset the window title.
	}
}

int dllrbt_compare( void *a, void *b )
{
	if ( a > b )
	{
		return 1;
	}

	if ( a < b )
	{
		return -1;
	}

	return 0;
}

wchar_t *GetExtensionFromFilename( wchar_t *filename, unsigned long length )
{
	while ( length != 0 && filename[ --length ] != L'.' );

	return filename + length;
}

wchar_t *GetFilenameFromPath( wchar_t *path, unsigned long length )
{
	while ( length != 0 && path[ --length ] != L'\\' );

	if ( path[ length ] == L'\\' )
	{
		++length;
	}
	return path + length;
}

wchar_t *GetSFGAOStr( unsigned long sfgao_flags )
{
	wchar_t *ret = NULL;
	if ( sfgao_flags == 0 )
	{
		ret = ( wchar_t * )malloc( sizeof( wchar_t ) * 5 );
		wmemcpy_s( ret, 5, L"None\0", 5 );
	}

	int size = _scwprintf( L"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
						( ( sfgao_flags & SFGAO_CANCOPY ) ? L"SFGAO_CANCOPY, " : L"" ),
						( ( sfgao_flags & SFGAO_CANMOVE ) ? L"SFGAO_CANMOVE, " : L"" ),
						( ( sfgao_flags & SFGAO_CANLINK ) ? L"SFGAO_CANLINK, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGE ) ? L"SFGAO_STORAGE, " : L"" ),
						( ( sfgao_flags & SFGAO_CANRENAME ) ? L"SFGAO_CANRENAME, " : L"" ),
						( ( sfgao_flags & SFGAO_CANDELETE ) ? L"SFGAO_CANDELETE, " : L"" ),
						( ( sfgao_flags & SFGAO_HASPROPSHEET ) ? L"SFGAO_HASPROPSHEET, " : L"" ),
						( ( sfgao_flags & SFGAO_DROPTARGET ) ? L"SFGAO_DROPTARGET, " : L"" ),
						( ( sfgao_flags & SFGAO_CAPABILITYMASK ) ? L"SFGAO_CAPABILITYMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_ENCRYPTED ) ? L"SFGAO_ENCRYPTED, " : L"" ),
						( ( sfgao_flags & SFGAO_ISSLOW ) ? L"SFGAO_ISSLOW, " : L"" ),
						( ( sfgao_flags & SFGAO_GHOSTED ) ? L"SFGAO_GHOSTED, " : L"" ),
						( ( sfgao_flags & SFGAO_LINK ) ? L"SFGAO_LINK, " : L"" ),
						( ( sfgao_flags & SFGAO_SHARE ) ? L"SFGAO_SHARE, " : L"" ),
						( ( sfgao_flags & SFGAO_READONLY ) ? L"SFGAO_READONLY, " : L"" ),
						( ( sfgao_flags & SFGAO_HIDDEN ) ? L"SFGAO_HIDDEN, " : L"" ),
						( ( sfgao_flags & SFGAO_DISPLAYATTRMASK ) ? L"SFGAO_DISPLAYATTRMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_FILESYSANCESTOR ) ? L"SFGAO_FILESYSANCESTOR, " : L"" ),
						( ( sfgao_flags & SFGAO_FOLDER ) ? L"SFGAO_FOLDER, " : L"" ),
						( ( sfgao_flags & SFGAO_FILESYSTEM ) ? L"SFGAO_FILESYSTEM, " : L"" ),
						( ( sfgao_flags & SFGAO_HASSUBFOLDER ) ? L"SFGAO_HASSUBFOLDER / SFGAO_CONTENTSMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_VALIDATE ) ? L"SFGAO_VALIDATE, " : L"" ),
						( ( sfgao_flags & SFGAO_REMOVABLE ) ? L"SFGAO_REMOVABLE, " : L"" ),
						( ( sfgao_flags & SFGAO_COMPRESSED ) ? L"SFGAO_COMPRESSED, " : L"" ),
						( ( sfgao_flags & SFGAO_BROWSABLE ) ? L"SFGAO_BROWSABLE, " : L"" ),
						( ( sfgao_flags & SFGAO_NONENUMERATED ) ? L"SFGAO_NONENUMERATED, " : L"" ),
						( ( sfgao_flags & SFGAO_NEWCONTENT ) ? L"SFGAO_NEWCONTENT, " : L"" ),
						( ( sfgao_flags & SFGAO_CANMONIKER ) ? L"SFGAO_CANMONIKER / SFGAO_HASSTORAGE / SFGAO_STREAM, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGEANCESTOR ) ? L"SFGAO_STORAGEANCESTOR, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGECAPMASK ) ? L"SFGAO_STORAGECAPMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_PKEYSFGAOMASK ) ? L"SFGAO_PKEYSFGAOMASK" : L"" ) );

	ret = ( wchar_t * )malloc( sizeof( wchar_t ) * ( size + 1 ) );

	size = swprintf_s( ret, size + 1, L"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
						( ( sfgao_flags & SFGAO_CANCOPY ) ? L"SFGAO_CANCOPY, " : L"" ),
						( ( sfgao_flags & SFGAO_CANMOVE ) ? L"SFGAO_CANMOVE, " : L"" ),
						( ( sfgao_flags & SFGAO_CANLINK ) ? L"SFGAO_CANLINK, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGE ) ? L"SFGAO_STORAGE, " : L"" ),
						( ( sfgao_flags & SFGAO_CANRENAME ) ? L"SFGAO_CANRENAME, " : L"" ),
						( ( sfgao_flags & SFGAO_CANDELETE ) ? L"SFGAO_CANDELETE, " : L"" ),
						( ( sfgao_flags & SFGAO_HASPROPSHEET ) ? L"SFGAO_HASPROPSHEET, " : L"" ),
						( ( sfgao_flags & SFGAO_DROPTARGET ) ? L"SFGAO_DROPTARGET, " : L"" ),
						( ( sfgao_flags & SFGAO_CAPABILITYMASK ) ? L"SFGAO_CAPABILITYMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_ENCRYPTED ) ? L"SFGAO_ENCRYPTED, " : L"" ),
						( ( sfgao_flags & SFGAO_ISSLOW ) ? L"SFGAO_ISSLOW, " : L"" ),
						( ( sfgao_flags & SFGAO_GHOSTED ) ? L"SFGAO_GHOSTED, " : L"" ),
						( ( sfgao_flags & SFGAO_LINK ) ? L"SFGAO_LINK, " : L"" ),
						( ( sfgao_flags & SFGAO_SHARE ) ? L"SFGAO_SHARE, " : L"" ),
						( ( sfgao_flags & SFGAO_READONLY ) ? L"SFGAO_READONLY, " : L"" ),
						( ( sfgao_flags & SFGAO_HIDDEN ) ? L"SFGAO_HIDDEN, " : L"" ),
						( ( sfgao_flags & SFGAO_DISPLAYATTRMASK ) ? L"SFGAO_DISPLAYATTRMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_FILESYSANCESTOR ) ? L"SFGAO_FILESYSANCESTOR, " : L"" ),
						( ( sfgao_flags & SFGAO_FOLDER ) ? L"SFGAO_FOLDER, " : L"" ),
						( ( sfgao_flags & SFGAO_FILESYSTEM ) ? L"SFGAO_FILESYSTEM, " : L"" ),
						( ( sfgao_flags & SFGAO_HASSUBFOLDER ) ? L"SFGAO_HASSUBFOLDER / SFGAO_CONTENTSMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_VALIDATE ) ? L"SFGAO_VALIDATE, " : L"" ),
						( ( sfgao_flags & SFGAO_REMOVABLE ) ? L"SFGAO_REMOVABLE, " : L"" ),
						( ( sfgao_flags & SFGAO_COMPRESSED ) ? L"SFGAO_COMPRESSED, " : L"" ),
						( ( sfgao_flags & SFGAO_BROWSABLE ) ? L"SFGAO_BROWSABLE, " : L"" ),
						( ( sfgao_flags & SFGAO_NONENUMERATED ) ? L"SFGAO_NONENUMERATED, " : L"" ),
						( ( sfgao_flags & SFGAO_NEWCONTENT ) ? L"SFGAO_NEWCONTENT, " : L"" ),
						( ( sfgao_flags & SFGAO_CANMONIKER ) ? L"SFGAO_CANMONIKER / SFGAO_HASSTORAGE / SFGAO_STREAM, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGEANCESTOR ) ? L"SFGAO_STORAGEANCESTOR, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGECAPMASK ) ? L"SFGAO_STORAGECAPMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_PKEYSFGAOMASK ) ? L"SFGAO_PKEYSFGAOMASK" : L"" ) );

	// Remove any trailing ", ".
	if ( size > 1 && ret[ size - 1 ] == L' ' )
	{
		ret[ size - 2 ] = L'\0';
	}

	return ret;
}

wchar_t *GetFileAttributesStr( unsigned long fa_flags )
{
	wchar_t *ret = NULL;
	if ( fa_flags == 0 )
	{
		ret = ( wchar_t * )malloc( sizeof( wchar_t ) * 5 );
		wmemcpy_s( ret, 5, L"None\0", 5 );
	}
	else
	{
		int size = _scwprintf( L"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
							( ( fa_flags & FILE_ATTRIBUTE_READONLY ) ? L"FILE_ATTRIBUTE_READONLY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_HIDDEN ) ? L"FILE_ATTRIBUTE_HIDDEN, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_SYSTEM ) ? L"FILE_ATTRIBUTE_SYSTEM, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_DIRECTORY ) ? L"FILE_ATTRIBUTE_DIRECTORY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_ARCHIVE ) ? L"FILE_ATTRIBUTE_ARCHIVE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_DEVICE ) ? L"FILE_ATTRIBUTE_DEVICE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_NORMAL ) ? L"FILE_ATTRIBUTE_NORMAL, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_TEMPORARY ) ? L"FILE_ATTRIBUTE_TEMPORARY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_SPARSE_FILE ) ? L"FILE_ATTRIBUTE_SPARSE_FILE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_REPARSE_POINT ) ? L"FILE_ATTRIBUTE_REPARSE_POINT, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_COMPRESSED ) ? L"FILE_ATTRIBUTE_COMPRESSED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_OFFLINE ) ? L"FILE_ATTRIBUTE_OFFLINE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED ) ? L"FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_ENCRYPTED ) ? L"FILE_ATTRIBUTE_ENCRYPTED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_VIRTUAL ) ? L"FILE_ATTRIBUTE_VIRTUAL" : L"" ) );

		ret = ( wchar_t * )malloc( sizeof( wchar_t ) * ( size + 1 ) );

		size = swprintf_s( ret, size + 1, L"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
							( ( fa_flags & FILE_ATTRIBUTE_READONLY ) ? L"FILE_ATTRIBUTE_READONLY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_HIDDEN ) ? L"FILE_ATTRIBUTE_HIDDEN, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_SYSTEM ) ? L"FILE_ATTRIBUTE_SYSTEM, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_DIRECTORY ) ? L"FILE_ATTRIBUTE_DIRECTORY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_ARCHIVE ) ? L"FILE_ATTRIBUTE_ARCHIVE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_DEVICE ) ? L"FILE_ATTRIBUTE_DEVICE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_NORMAL ) ? L"FILE_ATTRIBUTE_NORMAL, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_TEMPORARY ) ? L"FILE_ATTRIBUTE_TEMPORARY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_SPARSE_FILE ) ? L"FILE_ATTRIBUTE_SPARSE_FILE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_REPARSE_POINT ) ? L"FILE_ATTRIBUTE_REPARSE_POINT, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_COMPRESSED ) ? L"FILE_ATTRIBUTE_COMPRESSED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_OFFLINE ) ? L"FILE_ATTRIBUTE_OFFLINE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED ) ? L"FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_ENCRYPTED ) ? L"FILE_ATTRIBUTE_ENCRYPTED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_VIRTUAL ) ? L"FILE_ATTRIBUTE_VIRTUAL" : L"" ) );

		// Remove any trailing ", ".
		if ( size > 1 && ret[ size - 1 ] == L' ' )
		{
			ret[ size - 2 ] = L'\0';
		}
	}

	return ret;
}

void CleanupExtendedInfo( EXTENDED_INFO *ei )
{
	EXTENDED_INFO *d_ei = NULL;
	while ( ei != NULL )
	{
		d_ei = ei;
		ei = ei->next;

		if ( d_ei->sei != NULL )
		{
			--( d_ei->sei->count );

			// Remove our shared information from the linked list if there's no more items.
			if ( d_ei->sei->count == 0 )
			{
				free( d_ei->sei->windows_property );
				free( d_ei->sei );
			}
		}

		free( d_ei->property_value );
		free( d_ei );
	}
}

void CleanupBlankEntries()
{
	// Go through the list of blank entries and free any shared info and FILE_INFO structures.
	LINKED_LIST *be = g_be;
	LINKED_LIST *del_be = NULL;
	while ( be != NULL )
	{
		del_be = be;
		be = be->next;

		if ( del_be->fi != NULL )
		{
			if ( del_be->fi->si != NULL )
			{
				--( del_be->fi->si->count );

				if ( del_be->fi->si->count == 0 )
				{
					free( del_be->fi->si );
				}
			}

			CleanupExtendedInfo( del_be->fi->ei );

			free( del_be->fi->filename );
			free( del_be->fi );
		}

		free( del_be );
	}
}

void CleanupFileinfoTree()
{
	// Free the values of the file info tree.
	node_type *node = dllrbt_get_head( g_file_info_tree );
	while ( node != NULL )
	{
		// Free the linked list if there is one.
		LINKED_LIST *fi_node = ( LINKED_LIST * )node->val;
		while ( fi_node != NULL )
		{
			LINKED_LIST *del_fi_node = fi_node;

			fi_node = fi_node->next;

			free( del_fi_node );
		}

		node = node->next;
	}

	// Clean up our file info tree.
	dllrbt_delete_recursively( g_file_info_tree );
	g_file_info_tree = NULL;
}

void CreateFileinfoTree()
{
	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;

	FILE_INFO *fi = NULL;

	int item_count = ( int )SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

	// Create the file info tree if it doesn't exist.
	if ( g_file_info_tree == NULL )
	{
		g_file_info_tree = dllrbt_create( dllrbt_compare );
	}

	// Go through each item and add them to our tree.
	for ( lvi.iItem = 0; lvi.iItem < item_count; ++lvi.iItem )
	{
		// We don't want to continue scanning if the user cancels the scan.
		if ( g_kill_scan )
		{
			break;
		}

		SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

		fi = ( FILE_INFO * )lvi.lParam;

		// Don't attempt to insert the FILE_INFO if it's already in the tree.
		if ( fi != NULL )
		{
			// Create the node to insert into a linked list.
			LINKED_LIST *fi_node = ( LINKED_LIST * )malloc( sizeof( LINKED_LIST ) );
			fi_node->fi = fi;
			fi_node->next = NULL;

			// In Windows Vista, the hash in the filename may not be the same as the cache entry hash.
			// The cache entry hash algorithm is different from Windows 7 as well.
			// Use the filename hash instead.
			if ( fi->si != NULL && fi->si->system == WINDOWS_VISTA )
			{
				if ( fi->filename != NULL )
				{
					int filename_length = ( int )wcslen( fi->filename );

					wchar_t *filename = NULL;
					wchar_t *filename_end = wcschr( fi->filename, L'.' );	// Check for an extension.
					if ( filename_end != NULL )
					{
						if ( filename_end - fi->filename <= 16 )	// Make sure it's at most 16 digits.
						{
							filename = ( wchar_t * )malloc( sizeof( wchar_t ) * ( ( filename_end - fi->filename ) + 1 ) );
							wmemcpy_s( filename, ( filename_end - fi->filename ) + 1, fi->filename, filename_end - fi->filename );
							filename[ filename_end - fi->filename ] = 0; // Sanity.
						}
					}
					else if ( filename_end == NULL && filename_length <= 16 && filename_length >= 0 )	// Make sure it's at most 16 digits.
					{
						filename = _wcsdup( fi->filename );
					}

					if ( filename != NULL )
					{
						fi->mapped_hash = _wcstoui64( filename, NULL, 16 );
						free( filename );
					}
				}
			}

			// See if our tree has the hash to add the node to.
			LINKED_LIST *ll = ( LINKED_LIST * )dllrbt_find( g_file_info_tree, ( void * )fi->mapped_hash, true );
			if ( ll == NULL )
			{
				if ( dllrbt_insert( g_file_info_tree, ( void * )fi->mapped_hash, fi_node ) != DLLRBT_STATUS_OK )
				{
					free( fi_node );
				}
			}
			else	// If a hash exits, insert the node into the linked list.
			{
				LINKED_LIST *next = ll->next;	// We'll insert the node after the head.
				fi_node->next = next;
				ll->next = fi_node;
			}
		}
	}
}

// This will allow our main thread to continue while secondary threads finish their processing.
unsigned __stdcall cleanup( void * /*pArguments*/ )
{
	// This semaphore will be released when the thread gets killed.
	g_shutdown_semaphore = CreateSemaphore( NULL, 0, 1, NULL );

	g_kill_thread = true;	// Causes our secondary threads to cease processing and release the semaphore.

	// Wait for any active threads to complete. 5 second timeout in case we miss the release.
	WaitForSingleObject( g_shutdown_semaphore, 5000 );
	CloseHandle( g_shutdown_semaphore );
	g_shutdown_semaphore = NULL;

	// DestroyWindow won't work on a window from a different thread. So we'll send a message to trigger it.
	SendMessage( g_hWnd_main, WM_DESTROY_ALT, 0, 0 );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall copy_items( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	Processing_Window( true );

	char type = ( char )pArguments;	// 0 = main list, 1 = extended info list

	HWND hWnd = ( type == 1 ? g_hWnd_list_info : g_hWnd_list );

	LVITEM lvi = { 0 };
	lvi.mask = LVIF_PARAM;
	lvi.iItem = -1;	// Set this to -1 so that the LVM_GETNEXTITEM call can go through the list correctly.

	int item_count = ( int )SendMessage( hWnd, LVM_GETITEMCOUNT, 0, 0 );
	int sel_count = ( int )SendMessage( hWnd, LVM_GETSELECTEDCOUNT, 0, 0 );
	
	bool copy_all = false;
	if ( item_count == sel_count )
	{
		copy_all = true;
	}
	else
	{
		item_count = sel_count;
	}

	unsigned int buffer_size = 8192;
	unsigned int buffer_offset = 0;
	wchar_t *copy_buffer = ( wchar_t * )malloc( sizeof( wchar_t ) * buffer_size );	// Allocate 8 kilobytes.

	int value_length = 0;

	wchar_t tbuf[ MAX_PATH ];
	wchar_t *buf = tbuf;

	bool add_newline = false;
	bool add_tab = false;

	char column_start = ( type == 1 ? 0 : 1 );
	char column_end = ( type == 1 ? 2 : NUM_COLUMNS );

	FILE_INFO *fi = NULL;
	EXTENDED_INFO *ei = NULL;

	// Go through each item, and copy their lParam values.
	for ( int i = 0; i < item_count; ++i )
	{
		// Stop processing and exit the thread.
		if ( g_kill_thread )
		{
			break;
		}

		if ( copy_all )
		{
			lvi.iItem = i;
		}
		else
		{
			lvi.iItem = ( int )SendMessage( hWnd, LVM_GETNEXTITEM, lvi.iItem, LVNI_SELECTED );
		}

		SendMessage( hWnd, LVM_GETITEM, 0, ( LPARAM )&lvi );

		if ( type == 1 )
		{
			ei = ( EXTENDED_INFO * )lvi.lParam;

			if ( ei == NULL || ( ei != NULL && ei->sei == NULL ) )
			{
				continue;
			}
		}
		else 
		{
			fi = ( FILE_INFO * )lvi.lParam;

			if ( fi == NULL || ( fi != NULL && fi->si == NULL ) )
			{
				continue;
			}
		}

		add_newline = add_tab = false;

		for ( int j = column_start; j < column_end; ++j )
		{
			switch ( j )
			{
				case 0:
				{
					buf = ei->sei->windows_property;
					value_length = ( buf != NULL ? ( int )wcslen( buf ) : 0 );
				}
				break;

				case 1:
				{
					buf = ( type == 1 ? ei->property_value : fi->filename );
					value_length = ( buf != NULL ? ( int )wcslen( buf ) : 0 );
				}
				break;

				case 2:
				{
					buf = tbuf;	// Reset the buffer pointer.

					// Depending on our toggle, output the offset (db location) in either kilobytes or bytes.
					value_length = swprintf_s( buf, MAX_PATH, ( is_kbytes_c_offset ? L"%d B" : L"%d KB" ), ( is_kbytes_c_offset ? fi->header_offset : fi->header_offset / 1024 ) );
				}
				break;

				case 3:
				{
					unsigned int cache_entry_size = fi->size + ( fi->data_offset - fi->header_offset );

					// Depending on our toggle, output the size in either kilobytes or bytes.
					value_length = swprintf_s( buf, MAX_PATH, ( is_kbytes_c_size ? L"%d KB" : L"%d B" ), ( is_kbytes_c_size ? cache_entry_size / 1024 : cache_entry_size ) );
				}
				break;

				case 4:
				{
					// Depending on our toggle, output the offset (db location) in either kilobytes or bytes.
					value_length = swprintf_s( buf, MAX_PATH, ( is_kbytes_d_offset ? L"%d B" : L"%d KB" ), ( is_kbytes_d_offset ? fi->data_offset : fi->data_offset / 1024 ) );
				}
				break;

				case 5:
				{
					// Depending on our toggle, output the size in either kilobytes or bytes.
					value_length = swprintf_s( buf, MAX_PATH, ( is_kbytes_d_size ? L"%d KB" : L"%d B" ), ( is_kbytes_d_size ? fi->size / 1024 : fi->size ) );
				}
				break;

				case 6:
				{
					// Output the hex string in either lowercase or uppercase.
					value_length = swprintf_s( buf, MAX_PATH, ( is_dc_lower ? L"%016llx" : L"%016llX" ), fi->data_checksum );

					if ( fi->v_data_checksum != fi->data_checksum )
					{
						value_length = swprintf_s( buf + value_length, MAX_PATH - value_length, ( is_dc_lower ? L" : %016llx" : L" : %016llX" ), fi->v_data_checksum );
					}
				}
				break;

				case 7:
				{
					// Output the hex string in either lowercase or uppercase.
					value_length = swprintf_s( buf, MAX_PATH, ( is_hc_lower ? L"%016llx" : L"%016llX" ), fi->header_checksum );

					if ( fi->v_header_checksum != fi->header_checksum )
					{
						value_length = swprintf_s( buf + value_length, MAX_PATH - value_length, ( is_hc_lower ? L" : %016llx" : L" : %016llX" ), fi->v_header_checksum );
					}
				}
				break;

				case 8:
				{
					// Output the hex string in either lowercase or uppercase.
					value_length = swprintf_s( buf, MAX_PATH, ( is_eh_lower ? L"%016llx" : L"%016llX" ), fi->entry_hash );
				}
				break;

				case 9:
				{
					switch ( fi->si->system )
					{
						case WINDOWS_VISTA:
						{
							buf = L"Windows Vista";
							value_length = 13;
						}
						break;

						case WINDOWS_7:
						{
							buf = L"Windows 7";
							value_length = 9;
						}
						break;

						case WINDOWS_8:
						case WINDOWS_8v2:
						case WINDOWS_8v3:
						{
							buf = L"Windows 8";
							value_length = 9;
						}
						break;

						case WINDOWS_8_1:
						{
							buf = L"Windows 8.1";
							value_length = 11;
						}
						break;

						case WINDOWS_10:
						{
							buf = L"Windows 10";
							value_length = 10;
						}
						break;

						default:
						{
							buf = L"Unknown";
							value_length = 7;
						}
						break;
					}
				}
				break;

				case 10:
				{
					buf = fi->si->dbpath;
					value_length = ( int )wcslen( buf );
				}
				break;
			}

			if ( buf == NULL || ( buf != NULL && buf[ 0 ] == NULL ) )
			{
				if ( ( ( type != 1 && j == 1 ) || ( type == 1 && j == 0 ) ) )
				{
					add_tab = false;
				}

				continue;
			}

			if ( ( ( type != 1 && j > 1 ) || ( type == 1 && j > 0 ) ) && add_tab )
			{
				*( copy_buffer + buffer_offset ) = L'\t';
				++buffer_offset;
			}

			add_tab = true;

			while ( buffer_offset + value_length + 3 >= buffer_size )	// Add +3 for \t and \r\n
			{
				buffer_size += 8192;
				wchar_t *realloc_buffer = ( wchar_t * )realloc( copy_buffer, sizeof( wchar_t ) * buffer_size );
				if ( realloc_buffer == NULL )
				{
					goto CLEANUP;
				}

				copy_buffer = realloc_buffer;
			}
			wmemcpy_s( copy_buffer + buffer_offset, buffer_size - buffer_offset, buf, value_length );
			buffer_offset += value_length;

			add_newline = true;
		}

		if ( i < item_count - 1 && add_newline )	// Add newlines for every item except the last.
		{
			*( copy_buffer + buffer_offset ) = L'\r';
			++buffer_offset;
			*( copy_buffer + buffer_offset ) = L'\n';
			++buffer_offset;
		}
		else if ( ( i == item_count - 1 && !add_newline ) && buffer_offset >= 2 )	// If add_newline is false for the last item, then a newline character is in the buffer.
		{
			buffer_offset -= 2;	// Ignore the last newline in the buffer.
		}
	}

	if ( OpenClipboard( hWnd ) )
	{
		EmptyClipboard();

		DWORD len = buffer_offset;

		// Allocate a global memory object for the text.
		HGLOBAL hglbCopy = GlobalAlloc( GMEM_MOVEABLE, sizeof( wchar_t ) * ( len + 1 ) );
		if ( hglbCopy != NULL )
		{
			// Lock the handle and copy the text to the buffer. lptstrCopy doesn't get freed.
			wchar_t *lptstrCopy = ( wchar_t * )GlobalLock( hglbCopy );
			if ( lptstrCopy != NULL )
			{
				wmemcpy_s( lptstrCopy, len + 1, copy_buffer, len );
				lptstrCopy[ len ] = 0; // Sanity
			}

			GlobalUnlock( hglbCopy );

			if ( SetClipboardData( CF_UNICODETEXT, hglbCopy ) == NULL )
			{
				GlobalFree( hglbCopy );	// Only free this Global memory if SetClipboardData fails.
			}

			CloseClipboard();
		}
	}

CLEANUP:

	free( copy_buffer );

	Processing_Window( false );

	// Release the semaphore if we're killing the thread.
	if ( g_shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( g_shutdown_semaphore, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall remove_items( void * /*pArguments*/ )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;
	
	skip_draw = true;	// Prevent the listview from drawing while freeing lParam values.

	Processing_Window( true );

	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;

	FILE_INFO *fi = NULL;

	int item_count = ( int )SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );
	int sel_count = ( int )SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 );

	// See if we've selected all the items. We can clear the list much faster this way.
	if ( item_count == sel_count )
	{
		// Go through each item, and free their lParam values. current_file_info will get deleted here.
		for ( lvi.iItem = 0; lvi.iItem < item_count; ++lvi.iItem )
		{
			// Stop processing and exit the thread.
			if ( g_kill_thread )
			{
				break;
			}

			// We first need to get the lParam value otherwise the memory won't be freed.
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			fi = ( FILE_INFO * )lvi.lParam;

			if ( fi != NULL )
			{
				if ( fi->si != NULL )
				{
					--( fi->si->count );

					// Remove our shared information from the linked list if there's no more items for this database.
					if ( fi->si->count == 0 )
					{
						free( fi->si );
					}
				}

				// Close the info window if we're removing its file info.
				if ( fi == g_current_fi )
				{
					SendMessage( g_hWnd_info, WM_CLOSE, 0, 0 );
				}
				CleanupExtendedInfo( fi->ei );

				// Free our filename, then FILE_INFO structure.
				free( fi->filename );
				free( fi );
			}
		}

		SendMessage( g_hWnd_list, LVM_DELETEALLITEMS, 0, 0 );
	}
	else	// Otherwise, we're going to have to go through each selection one at a time. (SLOOOOOW) Start from the end and work our way to the beginning.
	{
		// Scroll to the first item.
		// This will reduce the time it takes to remove a large selection of items.
		// When we delete the item from the end of the listview, the control won't force a paint refresh (since the item's not likely to be visible)
		SendMessage( g_hWnd_list, LVM_ENSUREVISIBLE, 0, FALSE );

		int *index_array = ( int * )malloc( sizeof( int ) * sel_count );

		lvi.iItem = -1;	// Set this to -1 so that the LVM_GETNEXTITEM call can go through the list correctly.

		// Create an index list of selected items (in reverse order).
		for ( int i = 0; i < sel_count; i++ )
		{
			lvi.iItem = index_array[ sel_count - 1 - i ] = ( int )SendMessage( g_hWnd_list, LVM_GETNEXTITEM, lvi.iItem, LVNI_SELECTED );
		}

		for ( int i = 0; i < sel_count; i++ )
		{
			// Stop processing and exit the thread.
			if ( g_kill_thread )
			{
				break;
			}

			// We first need to get the lParam value otherwise the memory won't be freed.
			lvi.iItem = index_array[ i ];
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			fi = ( FILE_INFO * )lvi.lParam;

			if ( fi != NULL )
			{
				if ( fi->si != NULL )
				{
					--( fi->si->count );

					// Remove our shared information from the linked list if there's no more items for this database.
					if ( fi->si->count == 0 )
					{
						free( fi->si );
					}
				}

				if ( fi == g_current_fi )
				{
					SendMessage( g_hWnd_info, WM_CLOSE, 0, 0 );
				}
				CleanupExtendedInfo( fi->ei );
				
				// Free our filename, then FILE_INFO structure.
				free( fi->filename );
				free( fi );
			}

			// Remove the list item.
			SendMessage( g_hWnd_list, LVM_DELETEITEM, index_array[ i ], 0 );
		}

		free( index_array );
	}

	skip_draw = false;	// Allow drawing again.

	Processing_Window( false );

	// Release the semaphore if we're killing the thread.
	if ( g_shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( g_shutdown_semaphore, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall show_hide_items( void * /*pArguments*/ )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	Processing_Window( true );

	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;

	int item_count = ( int )SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

	if ( !hide_blank_entries )	// Display the blank entries.
	{
		// This will reinsert the blank entry at the end of the listview.
		LINKED_LIST *be = g_be;
		LINKED_LIST *del_be = NULL;
		g_be = NULL;
		while ( be != NULL )
		{
			// Stop processing and exit the thread.
			if ( g_kill_thread )
			{
				g_be = be;	// Reset the global blank entries list to free in WM_DESTORY.

				break;
			}
			del_be = be;

			// Insert a row into our listview.
			lvi.iItem = item_count++;
			lvi.iSubItem = 0;
			lvi.lParam = ( LPARAM )be->fi;
			SendMessage( g_hWnd_list, LVM_INSERTITEM, 0, ( LPARAM )&lvi );

			be = be->next;
			free( del_be );	// Remove the entry from the linked list. We do this for easy managment in case the user decides to remove an item from the listview.
		}
	}
	else	// Hide the blank entries.
	{
		// Scroll to the first item.
		// This will reduce the time it takes to remove a large selection of items.
		// When we delete the item from the end of the listview, the control won't force a paint refresh (since the item's not likely to be visible)
		SendMessage( g_hWnd_list, LVM_ENSUREVISIBLE, 0, FALSE );

		// Start from the end and work our way to the beginning.
		for ( lvi.iItem = item_count - 1; lvi.iItem >= 0; --lvi.iItem )
		{
			// Stop processing and exit the thread.
			if ( g_kill_thread )
			{
				break;
			}

			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			// If the list item is blank, then add it to the blank entry linked list.
			if ( lvi.lParam != NULL && ( ( FILE_INFO * )lvi.lParam )->size == 0 )
			{
				LINKED_LIST *be = ( LINKED_LIST * )malloc( sizeof( LINKED_LIST ) );
				be->fi = ( FILE_INFO * )lvi.lParam;
				be->next = g_be;

				g_be = be;

				// Remove the list item.
				SendMessage( g_hWnd_list, LVM_DELETEITEM, lvi.iItem, 0 );
			}
		}
	}

	Processing_Window( false );

	// Release the semaphore if we're killing the thread.
	if ( g_shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( g_shutdown_semaphore, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall verify_checksums( void * /*pArguments*/ )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	Processing_Window( true );

	// Create our buffers to hash.
	char *header_buffer = NULL;
	char *data_buffer = NULL;
	char *tmp_data = NULL;	// Used to offset the data buffer.

	unsigned int bad_header = 0;
	unsigned int bad_data = 0;

	unsigned int header_size = 0;

	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;

	FILE_INFO *fi = NULL;

	int item_count = ( int )SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

	// Go through each item to compare values.
	for ( lvi.iItem = 0; lvi.iItem < item_count; ++lvi.iItem )
	{
		// Stop processing and exit the thread.
		if ( g_kill_thread )
		{
			break;
		}

		SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

		fi = ( FILE_INFO * )lvi.lParam;
		if ( fi == NULL || ( fi != NULL && fi->si == NULL ) )
		{
			continue;
		}

		// Skip entries that we've already verified, but count bad checksums.
		if ( fi->flag >= FIF_VERIFIED_HEADER )
		{
			if ( fi->flag & FIF_BAD_HEADER )
			{
				++bad_header;
			}

			if ( fi->flag & FIF_BAD_DATA )
			{
				++bad_data;
			}

			continue;
		}

		// Get the header size of the current entry.
		header_size = ( fi->si->system == WINDOWS_7 ? sizeof( database_cache_entry_7 ) : ( fi->si->system == WINDOWS_VISTA ? sizeof( database_cache_entry_vista ) : sizeof( database_cache_entry_8 ) ) ) - sizeof( unsigned long long );

		// Attempt to open a file for reading.
		HANDLE hFile = CreateFile( fi->si->dbpath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		if ( hFile != INVALID_HANDLE_VALUE )
		{
			fi->flag |= FIF_VERIFIED_HEADER;	// Entry has been verified

			header_buffer = ( char * )malloc( sizeof( char ) * header_size );

			DWORD read = 0;
			// Set our file pointer to the beginning of the database file.
			SetFilePointer( hFile, fi->header_offset, 0, FILE_BEGIN );
			// Read the header into memory.
			ReadFile( hFile, header_buffer, header_size, &read, NULL );

			data_buffer = ( char * )malloc( sizeof( char ) * fi->size );
			tmp_data = data_buffer;

			// Set our file pointer to the beginning of the database file.
			SetFilePointer( hFile, fi->data_offset, 0, FILE_BEGIN );
			// Read the entire image into memory.
			ReadFile( hFile, data_buffer, fi->size, &read, NULL );
			CloseHandle( hFile );

			// The header checksum uses an initial CRC of -1
			fi->v_header_checksum = crc64( header_buffer, header_size, 0xFFFFFFFFFFFFFFFF );
			if ( fi->v_header_checksum != fi->header_checksum )
			{
				fi->flag |= FIF_BAD_HEADER;	// Header checksum is invalid.
				++bad_header;
			}

			free( header_buffer );

			// If the data is larger than 1024 bytes, then we're going to generate two CRCs and xor them together.
			if ( read > 1024 )
			{
				// The first checksum uses an initial CRC of 0. We read the first 1024 bytes.
				unsigned long long first_crc = crc64( tmp_data, 1024, 0x0000000000000000 );
				tmp_data += 1024;

				// Break the remaining data into 400 byte chunks.
				read -= 1024;
				int chunks = read / 400;

				// The second CRC also uses an initial CRC of 0.
				unsigned long long second_crc = 0x0000000000000000;

				// For each of these chunks, we hash the first 4 bytes.
				for ( int i = 0; i < chunks; ++i )
				{
					second_crc = crc64( tmp_data, 4, second_crc );
					tmp_data += 400;	// Move to the next chunk.
				}

				// See how many bytes we have left.
				int remaining = read % 400;
				if ( remaining > 0 )
				{
					// If we have more than 4 bytes left to hash, then set it to 4.
					if ( remaining > 4 )
					{
						remaining = 4;
					}
					second_crc = crc64( tmp_data, remaining, second_crc );
				}

				// xor the two CRCs to generate the final data checksum.
				fi->v_data_checksum = ( first_crc ^ second_crc );
			}
			else	// Data is less than or equal to 1024 bytes
			{
				// The header checksum uses an initial CRC of 0
				fi->v_data_checksum = crc64( tmp_data, read, 0x0000000000000000 );
			}

			if ( fi->v_data_checksum != fi->data_checksum )
			{
				fi->flag |= FIF_BAD_DATA;	// Data checksum is invalid.
				++bad_data;
			}

			free( data_buffer );
		}
	}

	if ( bad_header == 0 && bad_data == 0 )
	{
		MessageBoxA( g_hWnd_main, "All checksums are valid.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONINFORMATION );
	}
	else
	{
		if ( bad_header > 0 )
		{
			char msg[ 51 ] = { 0 };
			sprintf_s( msg, 51, "%d mismatched header checksum%s found.", bad_header, ( bad_header > 1 ? "s were" : " was" ) );
			MessageBoxA( g_hWnd_main, msg, PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
		}

		if ( bad_data > 0 )
		{
			char msg[ 49 ] = { 0 };
			sprintf_s( msg, 49, "%d mismatched data checksum%s found.", bad_data, ( bad_data > 1 ? "s were" : " was" ) );
			MessageBoxA( g_hWnd_main, msg, PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
		}
	}

	Processing_Window( false );

	// Release the semaphore if we're killing the thread.
	if ( g_shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( g_shutdown_semaphore, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

// Allocates a new string if characters need escaping. Otherwise, it returns NULL.
char *escape_csv( const char *string )
{
	char *escaped_string = NULL;
	char *q = NULL;
	const char *p = NULL;
	int c = 0;

	if ( string == NULL )
	{
		return NULL;
	}

	// Get the character count and offset it for any quotes.
	for ( c = 0, p = string; *p != NULL; ++p ) 
	{
		if ( *p != '\"' )
		{
			++c;
		}
		else
		{
			c += 2;
		}
	}

	// If the string has no special characters to escape, then return NULL.
	if ( c <= ( p - string ) )
	{
		return NULL;
	}

	q = escaped_string = ( char * )malloc( sizeof( char ) * ( c + 1 ) );

	for ( p = string; *p != NULL; ++p ) 
	{
		if ( *p != '\"' )
		{
			*q = *p;
			++q;
		}
		else
		{
			*q++ = '\"';
			*q++ = '\"';
		}
	}

	*q = 0;	// Sanity.

	return escaped_string;
}

unsigned __stdcall save_csv( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	Processing_Window( true );

	wchar_t *filepath = ( wchar_t * )pArguments;
	if ( filepath != NULL )
	{
		// Open our config file if it exists.
		HANDLE hFile = CreateFile( filepath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		if ( hFile != INVALID_HANDLE_VALUE )
		{
			int size = ( 32768 + 1 );
			DWORD write = 0;
			int write_buf_offset = 0;
			char *system_string = NULL;

			char *write_buf = ( char * )malloc( sizeof( char ) * size );

			// Write the UTF-8 BOM and CSV column titles.
			WriteFile( hFile, "\xEF\xBB\xBF" "Filename,Cache Entry Offset (bytes),Cache Entry Size (bytes),Data Offset (bytes),Data Size (bytes),Data Checksum,Header Checksum,Cache Entry Hash,System,Location", 164, &write, NULL );

			// Get the number of items we'll be saving.
			int save_items = ( int )SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

			// Retrieve the lParam value from the selected listview item.
			LVITEM lvi = { NULL };
			lvi.mask = LVIF_PARAM;

			FILE_INFO *fi = NULL;

			// Go through all the items we'll be saving.
			for ( lvi.iItem = 0; lvi.iItem < save_items; ++lvi.iItem )
			{
				// Stop processing and exit the thread.
				if ( g_kill_thread )
				{
					break;
				}

				SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

				fi = ( FILE_INFO * )lvi.lParam;
				if ( fi == NULL || ( fi != NULL && fi->si == NULL ) )
				{
					continue;
				}

				int filename_length = WideCharToMultiByte( CP_UTF8, 0, ( fi->filename != NULL ? fi->filename : L"" ), -1, NULL, 0, NULL, NULL );
				char *utf8_filename = ( char * )malloc( sizeof( char ) * filename_length ); // Size includes the null character.
				filename_length = WideCharToMultiByte( CP_UTF8, 0, ( fi->filename != NULL ? fi->filename : L"" ), -1, utf8_filename, filename_length, NULL, NULL ) - 1;

				// The filename comes from the database entry and it could have unsupported characters.
				char *escaped_filename = escape_csv( utf8_filename );
				if ( escaped_filename != NULL )
				{
					free( utf8_filename );
					utf8_filename = escaped_filename;
				}

				int dbpath_length = WideCharToMultiByte( CP_UTF8, 0, fi->si->dbpath, -1, NULL, 0, NULL, NULL );
				char *utf8_dbpath = ( char * )malloc( sizeof( char ) * dbpath_length ); // Size includes the null character.
				dbpath_length = WideCharToMultiByte( CP_UTF8, 0, fi->si->dbpath, -1, utf8_dbpath, dbpath_length, NULL, NULL ) - 1;

				switch ( fi->si->system )
				{
					case WINDOWS_VISTA:
					{
						system_string = "Windows Vista";
					}
					break;

					case WINDOWS_7:
					{
						system_string = "Windows 7";
					}
					break;

					case WINDOWS_8:
					case WINDOWS_8v2:
					case WINDOWS_8v3:
					{
						system_string = "Windows 8";
					}
					break;

					case WINDOWS_8_1:
					{
						system_string = "Windows 8.1";
					}
					break;

					case WINDOWS_10:
					{
						system_string = "Windows 10";
					}
					break;

					default:
					{
						system_string = "Unknown";
					}
					break;
				}

				// See if the next entry can fit in the buffer. If it can't, then we dump the buffer.
				if ( write_buf_offset + filename_length + dbpath_length + ( 10 * 4 ) + ( 20 * 5 ) + 13 + 21 + 1 > size )
				{
					// Dump the buffer.
					WriteFile( hFile, write_buf, write_buf_offset, &write, NULL );
					write_buf_offset = 0;
				}

				write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, "\r\n\"%s\",%lu,%lu,%lu,%lu,%016llx",
											   utf8_filename,
											   fi->header_offset, fi->size + ( fi->data_offset - fi->header_offset ),
											   fi->data_offset, fi->size,
											   fi->data_checksum );

				if ( fi->v_data_checksum != fi->data_checksum )
				{
					write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, " : %016llx",
											   fi->v_data_checksum );
				}

				write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, ",%016llx",
											   fi->header_checksum );

				if ( fi->v_header_checksum != fi->header_checksum )
				{
					write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, " : %016llx",
											   fi->v_header_checksum );
				}

				write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, ",%016llx,%s,\"%s\"",
											   fi->entry_hash,
											   system_string,
											   utf8_dbpath );

				free( utf8_filename );
				free( utf8_dbpath );
			}

			// If there's anything remaining in the buffer, then write it to the file.
			if ( write_buf_offset > 0 )
			{
				WriteFile( hFile, write_buf, write_buf_offset, &write, NULL );
			}

			free( write_buf );

			CloseHandle( hFile );
		}

		free( filepath );
	}

	Processing_Window( false );

	// Release the semaphore if we're killing the thread.
	if ( g_shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( g_shutdown_semaphore, 1, NULL );
	}
	else if ( cmd_line == 2 )	// Exit the program if we're done saving.
	{
		// DestroyWindow won't work on a window from a different thread. So we'll send a message to trigger it.
		SendMessage( g_hWnd_main, WM_DESTROY_ALT, 0, 0 );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall save_items( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	Processing_Window( true );

	SAVE_INFO *save_type = ( SAVE_INFO * )pArguments;
	if ( save_type != NULL )
	{
		wchar_t save_directory[ MAX_PATH ] = { 0 };
		if ( save_type->filepath == NULL )
		{
			GetCurrentDirectory( MAX_PATH, save_directory );
		}
		else if ( save_type->type == 1 )
		{
			// Create and set the directory that we'll be outputting files to.
			if ( GetFileAttributes( save_type->filepath ) == INVALID_FILE_ATTRIBUTES )
			{
				CreateDirectory( save_type->filepath, NULL );
			}

			// Get the full path if the input was relative.
			GetFullPathName( save_type->filepath, MAX_PATH, save_directory, NULL );
		}
		else
		{
			wcsncpy_s( save_directory, MAX_PATH, save_type->filepath, MAX_PATH - 1 );
		}

		// Depending on what was selected, get the number of items we'll be saving.
		int save_items = ( save_type->save_all ? ( int )SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) : ( int )SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 ) );

		// Retrieve the lParam value from the selected listview item.
		LVITEM lvi = { NULL };
		lvi.mask = LVIF_PARAM;
		lvi.iItem = -1;	// Set this to -1 so that the LVM_GETNEXTITEM call can go through the list correctly.

		FILE_INFO *fi = NULL;

		// Go through all the items we'll be saving.
		for ( int i = 0; i < save_items; ++i )
		{
			// Stop processing and exit the thread.
			if ( g_kill_thread )
			{
				break;
			}

			lvi.iItem = ( save_type->save_all ? i : ( int )SendMessage( g_hWnd_list, LVM_GETNEXTITEM, lvi.iItem, LVNI_SELECTED ) );
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			fi = ( FILE_INFO * )lvi.lParam;
			if ( fi == NULL || ( fi != NULL && ( fi->filename == NULL || fi->si == NULL ) ) )
			{
				continue;
			}

			// Skip 0 byte files.
			if ( fi->size != 0 )
			{
				// Create a buffer to read in our new bitmap.
				char *save_image = ( char * )malloc( sizeof( char ) * fi->size );

				// Attempt to open a file for reading.
				HANDLE hFile = CreateFile( fi->si->dbpath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
				if ( hFile != INVALID_HANDLE_VALUE )
				{
					DWORD read = 0;
					// Set our file pointer to the beginning of the database file.
					SetFilePointer( hFile, fi->data_offset, 0, FILE_BEGIN );
					// Read the entire image into memory.
					ReadFile( hFile, save_image, fi->size, &read, NULL );
					CloseHandle( hFile );

					// Directory + backslash + filename + extension + NULL character = ( MAX_PATH * 2 ) + 6
					wchar_t fullpath[ ( MAX_PATH * 2 ) + 6 ] = { 0 };

					wchar_t *filename = GetFilenameFromPath( fi->filename, ( unsigned long )wcslen( fi->filename ) );

					// Replace any invalid filename characters with an underscore "_".
					wchar_t escaped_filename[ MAX_PATH ] = { 0 };
					unsigned int escaped_filename_length = 0; 
					while ( filename != NULL && *filename != NULL && escaped_filename_length < MAX_PATH )
					{
						if ( *filename == L'\\' ||
							 *filename == L'/' ||
							 *filename == L':' ||
							 *filename == L'*' ||
							 *filename == L'?' ||
							 *filename == L'\"' ||
							 *filename == L'<' ||
							 *filename == L'>' ||
							 *filename == L'|' )
						{
							escaped_filename[ escaped_filename_length ] = L'_';
						}
						else
						{
							escaped_filename[ escaped_filename_length ] = *filename;
						}

						++escaped_filename_length;
						++filename;
					}
					escaped_filename[ escaped_filename_length ] = 0;	// Sanity.

					if ( fi->flag & FIF_TYPE_BMP )
					{
						wchar_t *ext = GetExtensionFromFilename( escaped_filename, escaped_filename_length );
						// The extension in the filename might not be the actual type. So we'll append .bmp to the end of it.
						if ( _wcsicmp( ext, L".bmp" ) == 0 )
						{
							swprintf_s( fullpath, ( MAX_PATH * 2 ) + 6, L"%.259s\\%.259s", save_directory, escaped_filename );
						}
						else
						{
							swprintf_s( fullpath, ( MAX_PATH * 2 ) + 6, L"%.259s\\%.259s.bmp", save_directory, escaped_filename );
						}
					}
					else if ( fi->flag & FIF_TYPE_JPG )
					{
						wchar_t *ext = GetExtensionFromFilename( escaped_filename, escaped_filename_length );
						// The extension in the filename might not be the actual type. So we'll append .jpg to the end of it.
						if ( _wcsicmp( ext, L".jpg" ) == 0 || _wcsicmp( ext, L".jpeg" ) == 0 )
						{
							swprintf_s( fullpath, ( MAX_PATH * 2 ) + 6, L"%.259s\\%.259s", save_directory, escaped_filename );
						}
						else
						{
							swprintf_s( fullpath, ( MAX_PATH * 2 ) + 6, L"%.259s\\%.259s.jpg", save_directory, escaped_filename );
						}
					}
					else if ( fi->flag & FIF_TYPE_PNG )
					{
						wchar_t *ext = GetExtensionFromFilename( escaped_filename, escaped_filename_length );
						// The extension in the filename might not be the actual type. So we'll append .png to the end of it.
						if ( _wcsicmp( ext, L".png" ) == 0 )
						{
							swprintf_s( fullpath, ( MAX_PATH * 2 ) + 6, L"%.259s\\%.259s", save_directory, escaped_filename );
						}
						else
						{
							swprintf_s( fullpath, ( MAX_PATH * 2 ) + 6, L"%.259s\\%.259s.png", save_directory, escaped_filename );
						}
					}
					else
					{
						swprintf_s( fullpath, ( MAX_PATH * 2 ) + 6, L"%.259s\\%.259s", save_directory, escaped_filename );
					}

					// Attempt to open a file for saving.
					HANDLE hFile_save = CreateFile( fullpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
					if ( hFile_save != INVALID_HANDLE_VALUE )
					{
						// Write the buffer to our file. Only write what we've read.
						DWORD dwBytesWritten = 0;
						WriteFile( hFile_save, save_image, read, &dwBytesWritten, NULL );

						CloseHandle( hFile_save );
					}

					// See if the path was too long.
					if ( GetLastError() == ERROR_PATH_NOT_FOUND )
					{
						if ( cmd_line != 2 ){ MessageBoxA( g_hWnd_main, "One or more files could not be saved. Please check the filename and path.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING ); }
					}
				}
				// Free our buffer.
				free( save_image );
			}
		}

		free( save_type->filepath );
		free( save_type );
	}

	Processing_Window( false );

	// Release the semaphore if we're killing the thread.
	if ( g_shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( g_shutdown_semaphore, 1, NULL );
	}
	else if ( cmd_line == 2 )	// Exit the program if we're done saving.
	{
		// DestroyWindow won't work on a window from a different thread. So we'll send a message to trigger it.
		SendMessage( g_hWnd_main, WM_DESTROY_ALT, 0, 0 );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

BOOL CALLBACK EnumChildProc( HWND hWnd, LPARAM lParam )
{
	HFONT hFont = ( HFONT )lParam;

	SendMessage( hWnd, WM_SETFONT, ( WPARAM )hFont, 0 );

	return TRUE;
}

HFONT UpdateFontsAndMetrics( UINT current_dpi_update, /*UINT last_dpi_update,*/ int *row_height )
{
	// Get the default message system font.
	NONCLIENTMETRICS ncm = { NULL };
	ncm.cbSize = sizeof( NONCLIENTMETRICS );
	SystemParametersInfoForDpi( SPI_GETNONCLIENTMETRICS, sizeof( NONCLIENTMETRICS ), &ncm, 0, current_dpi_update );

	// Set our global font to the LOGFONT value obtained from the system.
	HFONT hFont = CreateFontIndirect( &ncm.lfMessageFont );

	if ( row_height != NULL )
	{
		// Get the row height for our listview control.
		TEXTMETRIC tm;
		HDC hDC = GetDC( NULL );
		HFONT ohf = ( HFONT )SelectObject( hDC, hFont );
		GetTextMetricsW( hDC, &tm );
		SelectObject( hDC, ohf );	// Reset old font.
		ReleaseDC( NULL, hDC );

		*row_height = tm.tmHeight + tm.tmExternalLeading + _SCALE_( 5, dpi_update );

		int icon_height = GetSystemMetricsForDpi( SM_CYSMICON, current_dpi_update ) + _SCALE_( 2, dpi_update );
		if ( *row_height < icon_height )
		{
			*row_height = icon_height;
		}
	}

	return hFont;
}
