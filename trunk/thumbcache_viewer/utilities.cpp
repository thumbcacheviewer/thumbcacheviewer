/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2014 Eric Kutcher

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

// Extensible Storage Engine library.
#pragma comment( lib, "esent.lib" )

#include "globals.h"
#include "crc64.h"

#include <stdio.h>

#define JET_VERSION 0x0501
#include <esent.h>

#define FILE_TYPE_BMP	"BM"
#define FILE_TYPE_JPEG	"\xFF\xD8\xFF\xE0"
#define FILE_TYPE_PNG	"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"

#define ntohl( i ) ( ( ( ( unsigned long )( i ) & 0xFF000000 ) >> 24 ) | \
					 ( ( ( unsigned long )( i ) & 0x00FF0000 ) >> 8 ) | \
					 ( ( ( unsigned long )( i ) & 0x0000FF00 ) << 8 ) | \
					 ( ( ( unsigned long )( i ) & 0x000000FF ) << 24 ) )

#define ntohll( i ) ( ( ( __int64 )ntohl( i & 0xFFFFFFFFU ) << 32 ) | ntohl( ( __int64 )( i >> 32 ) ) )

HANDLE shutdown_semaphore = NULL;	// Blocks shutdown while a worker thread is active.
bool kill_thread = false;			// Allow for a clean shutdown.

CRITICAL_SECTION pe_cs;				// Queues additional worker threads.
bool in_thread = false;				// Flag to indicate that we're in a worker thread.
bool skip_draw = false;				// Prevents WM_DRAWITEM from accessing listview items while we're removing them.

linked_list *g_be = NULL;			// A list to hold all of the blank entries.

dllrbt_tree *fileinfo_tree = NULL;	// Red-black tree of fileinfo structures.

CLSID clsid;						// Holds a drive's Volume GUID.
unsigned int file_count = 0;		// Number of files scanned.
unsigned int match_count = 0;		// Number of files that match an entry hash.

void Processing_Window( bool enable )
{
	if ( enable == true )
	{
		SetWindowTextA( g_hWnd_main, "Thumbcache Viewer - Please wait..." );	// Update the window title.
		EnableWindow( g_hWnd_list, FALSE );										// Prevent any interaction with the listview while we're processing.
		SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, TRUE, 0 );					// SetCursor only works from the main thread. Set it to an arrow with hourglass.
	}
	else
	{
		SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, FALSE, 0 );	// Reset the cursor.
		EnableWindow( g_hWnd_list, TRUE );						// Allow the listview to be interactive. Also forces a refresh to update the item count column.
		SetFocus( g_hWnd_list );								// Give focus back to the listview to allow shortcut keys.
		SetWindowTextA( g_hWnd_main, PROGRAM_CAPTION_A );		// Reset the window title.
	}

	update_menus( enable );	// Disable all processing menu items.
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

wchar_t *get_extension_from_filename( wchar_t *filename, unsigned long length )
{
	while ( length != 0 && filename[ --length ] != L'.' );

	return filename + length;
}

wchar_t *get_filename_from_path( wchar_t *path, unsigned long length )
{
	while ( length != 0 && path[ --length ] != L'\\' );

	if ( path[ length ] == L'\\' )
	{
		++length;
	}
	return path + length;
}

void cleanup_blank_entries()
{
	// Go through the list of blank entries and free any shared info and fileinfo structures.
	linked_list *be = g_be;
	linked_list *del_be = NULL;
	while ( be != NULL )
	{
		del_be = be;
		be = be->next;

		if ( del_be->fi != NULL )
		{
			if ( del_be->fi->si != NULL )
			{
				del_be->fi->si->count--;

				if ( del_be->fi->si->count == 0 )
				{
					free( del_be->fi->si );
				}
			}

			free( del_be->fi->filename );
			free( del_be->fi );
		}

		free( del_be );
	}
}

void cleanup_fileinfo_tree()
{
	// Free the values of the fileinfo tree.
	node_type *node = dllrbt_get_head( fileinfo_tree );
	while ( node != NULL )
	{
		// Free the linked list if there is one.
		linked_list *fi_node = ( linked_list * )node->val;
		while ( fi_node != NULL )
		{
			linked_list *del_fi_node = fi_node;

			fi_node = fi_node->next;

			free( del_fi_node );
		}

		node = node->next;
	}

	// Clean up our fileinfo tree.
	dllrbt_delete_recursively( fileinfo_tree );
	fileinfo_tree = NULL;
}

void create_fileinfo_tree()
{
	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;

	fileinfo *fi = NULL;

	int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

	// Create the fileinfo tree if it doesn't exist.
	if ( fileinfo_tree == NULL )
	{
		fileinfo_tree = dllrbt_create( dllrbt_compare );
	}

	// Go through each item and add them to our tree.
	for ( int i = 0; i < item_count; ++i )
	{
		// We don't want to continue scanning if the user cancels the scan.
		if ( kill_scan == true )
		{
			break;
		}

		lvi.iItem = i;
		SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

		fi = ( fileinfo * )lvi.lParam;

		// Don't attempt to insert the fileinfo if it's already in the tree.
		if ( fi != NULL && !( fi->flag & FIF_IN_TREE ) )
		{
			// Create the node to insert into a linked list.
			linked_list *fi_node = ( linked_list * )malloc( sizeof( linked_list ) );
			fi_node->fi = fi;
			fi_node->next = NULL;

			// See if our tree has the hash to add the node to.
			linked_list *ll = ( linked_list * )dllrbt_find( fileinfo_tree, ( void * )fi->entry_hash, true );
			if ( ll == NULL )
			{
				if ( dllrbt_insert( fileinfo_tree, ( void * )fi->entry_hash, fi_node ) != DLLRBT_STATUS_OK )
				{
					free( fi_node );
				}
				else
				{
					fi->flag |= FIF_IN_TREE;
				}
			}
			else	// If a hash exits, insert the node into the linked list.
			{
				linked_list *next = ll->next;	// We'll insert the node after the head.
				fi_node->next = next;
				ll->next = fi_node;

				fi->flag |= FIF_IN_TREE;
			}
		}
	}

	file_count = 0;		// Reset the file count.
	match_count = 0;	// Reset the match count.
}

void update_scan_info( unsigned long long hash, wchar_t *filepath )
{
	// Now that we have a hash value to compare, search our fileinfo tree for the same value.
	linked_list *ll = ( linked_list * )dllrbt_find( fileinfo_tree, ( void * )hash, true );
	while ( ll != NULL )
	{
		if ( ll->fi != NULL )
		{
			++match_count;

			// Replace the hash filename with the local filename.
			free( ll->fi->filename );
			ll->fi->filename = _wcsdup( filepath );
		}

		ll = ll->next;
	}

	++file_count; 

	// Update our scan window with new scan information.
	if ( show_details == true )
	{
		SendMessage( g_hWnd_scan, WM_PROPAGATE, 3, ( LPARAM )filepath );
		char buf[ 19 ] = { 0 };
		sprintf_s( buf, 19, "0x%016llx", hash );
		SendMessageA( g_hWnd_scan, WM_PROPAGATE, 4, ( LPARAM )buf );
		sprintf_s( buf, 19, "%lu", file_count );
		SendMessageA( g_hWnd_scan, WM_PROPAGATE, 5, ( LPARAM )buf );
	}
}

unsigned long long hash_data( char *data, unsigned long long hash, short length )
{
	while ( length-- > 0 )
	{
		hash = ( ( ( hash * 0x820 ) + ( *data++ & 0x00000000000000FF ) ) + ( hash >> 2 ) ) ^ hash;
	}

	return hash;
}

void hash_file( wchar_t *filepath, wchar_t *extension )
{
	// Initial hash value. This value was found in shell32.dll.
	unsigned long long hash = 0x95E729BA2C37FD21;

	// Hash Volume GUID
	hash = hash_data( ( char * )&clsid, hash, sizeof( CLSID ) );

	// Hash File ID - found in the Master File Table.
	HANDLE hFile = CreateFile( filepath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL );
	BY_HANDLE_FILE_INFORMATION bhfi;
	GetFileInformationByHandle( hFile, &bhfi );
	CloseHandle( hFile );
	unsigned long long file_id = bhfi.nFileIndexHigh;
	file_id = ( file_id << 32 ) | bhfi.nFileIndexLow;
	
	hash = hash_data( ( char * )&file_id, hash, sizeof( unsigned long long ) );

	// Hash Wide Character File Extension
	hash = hash_data( ( char * )extension, hash, wcslen( extension ) * sizeof( wchar_t ) );

	// Hash Last Modified DOS Time
	unsigned short fat_date;
	unsigned short fat_time;
	FileTimeToDosDateTime( &bhfi.ftLastWriteTime, &fat_date, &fat_time );
	unsigned int dos_time = fat_date;
	dos_time = ( dos_time << 16 ) | fat_time;

	hash = hash_data( ( char * )&dos_time, hash, sizeof( unsigned int ) );

	update_scan_info( hash, filepath );
}

void traverse_directory( wchar_t *path )
{
	// We don't want to continue scanning if the user cancels the scan.
	if ( kill_scan == true )
	{
		return;
	}

	// Set the file path to search for all files/folders in the current directory.
	wchar_t filepath[ MAX_PATH ];
	swprintf_s( filepath, MAX_PATH, L"%s\\*", path );

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFileEx( ( LPCWSTR )filepath, FindExInfoStandard, &FindFileData, FindExSearchNameMatch, NULL, 0 );
	if ( hFind != INVALID_HANDLE_VALUE ) 
	{
		do
		{
			if ( kill_scan == true )
			{
				break;	// We need to close the find file handle.
			}

			wchar_t next_path[ MAX_PATH ];

			// See if the file is a directory.
			if ( ( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
			{
				// Go through all directories except "." and ".." (current and parent)
				if ( ( wcscmp( FindFileData.cFileName, L"." ) != 0 ) && ( wcscmp( FindFileData.cFileName, L".." ) != 0 ) )
				{
					// Move to the next directory.
					swprintf_s( next_path, MAX_PATH, L"%s\\%s", path, FindFileData.cFileName );

					traverse_directory( next_path );

					// Only hash folders if enabled.
					if ( include_folders == true )
					{
						hash_file( next_path, L"" );
					}
				}
			}
			else
			{
				// See if the file's extension is in our filter. Go to the next file if it's not.
				wchar_t *ext = get_extension_from_filename( FindFileData.cFileName, wcslen( FindFileData.cFileName ) );
				if ( extension_filter[ 0 ] != 0 )
				{
					// Do a case-insensitive substring search for the extension.
					int ext_length = wcslen( ext );
					wchar_t *temp_ext = ( wchar_t * )malloc( sizeof( wchar_t ) * ( ext_length + 3 ) );
					for ( int i = 0; i < ext_length; ++i )
					{
						temp_ext[ i + 1 ] = towlower( ext[ i ] );
					}
					temp_ext[ 0 ] = L'|';				// Append the delimiter to the beginning of the string.
					temp_ext[ ext_length + 1 ] = L'|';	// Append the delimiter to the end of the string.
					temp_ext[ ext_length + 2 ] = L'\0';

					if ( wcsstr( extension_filter, temp_ext ) == NULL )
					{
						free( temp_ext );
						continue;
					}

					free( temp_ext );
				}

				swprintf_s( next_path, MAX_PATH, L"%s\\%s", path, FindFileData.cFileName );

				hash_file( next_path, ext );
			}
		}
		while ( FindNextFile( hFind, &FindFileData ) != 0 );	// Go to the next file.

		FindClose( hFind );	// Close the find file handle.
	}
}

// The Microsoft Jet Database Engine seems to have a lot of annoying quirks/compatibility issues.
// The directory scanner is a nice compliment should this function not work 100%.
// Ideally, the database being scanned should be done with the same esent.dll that was used to create it.
// If there are issues with the database, make a copy and use esentutl.exe to fix it.
void traverse_ese_database()
{
	JET_INSTANCE instance = JET_instanceNil;
	JET_SESID sesid = JET_sesidNil;
	JET_DBID dbid = 0;
	JET_TABLEID tableid = JET_tableidNil;
	JET_ERR err = JET_errSuccess;

	JET_COLUMNDEF thumbnail_cache_id_column = { 0 }, item_path_display_column = { 0 }, file_extension_column = { 0 }, file_attributes_column = { 0 };

	JET_RETRIEVECOLUMN rc[ 4 ] = { 0 };

	char *ascii_filepath = NULL;

	unsigned long long thumbnail_cache_id = 0;
	char *item_path_display = NULL;
	char *file_extension = NULL;
	DWORD file_attributes = 0;

	unsigned long long hash = 0;

	unsigned long ulUpdate = 0, cbPageSize = 0;

	char *partial_header = NULL;
	DWORD read = 0;

	char error[ 1024 ] = { 0 };
	int error_offset = 0;
	unsigned char error_state = 0;

	// JetGetDatabaseFileInfo for Windows Vista doesn't seem to like weird page sizes so we'll get it manually.
	HANDLE hFile = CreateFile( g_filepath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		partial_header = ( char * )malloc( sizeof( char ) * 512 );

		ReadFile( hFile, partial_header, sizeof( char ) * 512, &read, NULL );

		CloseHandle( hFile );
	}

	// Make sure we got enough of the header and it has the magic identifier (0x89ABCDEF) for an ESE database.
	if ( read < 512 || partial_header == NULL || memcmp( partial_header + 4, "\xEF\xCD\xAB\x89", sizeof( unsigned long ) ) != 0 )
	{
		MessageBoxA( g_hWnd_scan, "The file is not an ESE database or has been corrupted.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
		goto CLEANUP;
	}

	memcpy_s( &ulUpdate, sizeof( unsigned long ), partial_header + 0xE8, sizeof( unsigned long ) );		// Revision number
	memcpy_s( &cbPageSize, sizeof( unsigned long ), partial_header + 0xEC, sizeof( unsigned long ) );	// Page size

	// The following Jet functions don't have Unicode support on XP (our minimum compatibility) or below.
	// Putting the database in the root directory (along with a Unicode filename) would get around the issue.
	int filepath_length = WideCharToMultiByte( CP_ACP, 0, g_filepath, -1, NULL, 0, NULL, NULL );
	ascii_filepath = ( char * )malloc( sizeof( char ) * filepath_length ); // Size includes the null character.
	WideCharToMultiByte( CP_ACP, 0, g_filepath, -1, ascii_filepath, filepath_length, NULL, NULL );

	// Disable event logging.
	err = JetSetSystemParameter( NULL, JET_sesidNil, JET_paramNoInformationEvent, true, NULL ); if ( err != JET_errSuccess ) { goto CLEANUP; }
	// Don't generate recovery files.
	err = JetSetSystemParameter( NULL, JET_sesidNil, JET_paramRecovery, NULL, "Off" ); if ( err != JET_errSuccess ) { goto CLEANUP; }
	// Don't generate any temporary tables.
	err = JetSetSystemParameter( NULL, JET_sesidNil, JET_paramMaxTemporaryTables, 0, NULL ); if ( err != JET_errSuccess ) { goto CLEANUP; }

	// 2KB, 16KB, and 32KB page sizes were added to Windows 7 (0x11) and above.
	if ( ( err = JetSetSystemParameter( NULL, JET_sesidNil, JET_paramDatabasePageSize, cbPageSize, NULL ) ) != JET_errSuccess ) 
	{
		// The database engine doesn't like our page size. It's probably from Windows Vista (0x0C) or below.
		if ( err == JET_errInvalidParameter && ulUpdate >= 0x11 ) { error_offset = sprintf_s( error, 1024, "The Microsoft Jet database engine is not supported for this version of database.\r\n\r\nPlease run the program with esent.dll from Windows 7 or higher.\r\n\r\n" ); }
		goto CLEANUP;
	}

	err = JetCreateInstance( &instance, PROGRAM_CAPTION_A ); if ( err != JET_errSuccess ) { goto CLEANUP; }
	err = JetInit( &instance ); if ( err != JET_errSuccess ) { goto CLEANUP; }
	err = JetBeginSession( instance, &sesid, 0, 0 ); if ( err != JET_errSuccess ) { goto CLEANUP; }

	if ( ( err = JetAttachDatabase( sesid, ascii_filepath, JET_bitDbReadOnly ) ) != JET_errSuccess )
	{
		error_state = 2;	// Don't detach database.
		if ( err == JET_errDatabaseDirtyShutdown ) { error_offset = sprintf_s( error, 1024, "Please run esentutl.exe to recover or repair the database.\r\n\r\n" ); }
		else if ( err == JET_errDatabaseInvalidPath || err == JET_errDiskIO || err == JET_errInvalidPath || err == JET_errInvalidSystemPath ) { error_offset = sprintf_s( error, 1024, "The database could not be loaded from its current location.\r\n\r\nTry moving the database into the root directory and ensure that there are no Unicode characters in the path.\r\n\r\n" ); }
		else if ( err == JET_errReadVerifyFailure ) { error_offset = sprintf_s( error, 1024, "The Microsoft Jet database engine may not be supported for this version of database.\r\n\r\nPlease run the program with esent.dll from Windows Vista or higher.\r\n\r\n" ); }	// I see this with XP esent.dll.
		goto CLEANUP;
	}

	if ( ( err = JetOpenDatabase( sesid, ascii_filepath, NULL, &dbid, JET_bitDbReadOnly ) ) != JET_errSuccess )
	{
		error_state = 1;	// Don't close database.
		goto CLEANUP;
	}

	if ( ( err = JetOpenTable( sesid, dbid, "SystemIndex_0A", NULL, 0, JET_bitTableReadOnly, &tableid ) ) != JET_errSuccess )
	{
		if ( err == JET_errObjectNotFound ) { error_offset = sprintf_s( error, 1024, "The SystemIndex_0A table was not found.\r\n\r\n" ); }
		goto CLEANUP;
	}

	if ( ( err = JetGetTableColumnInfo( sesid, tableid, "System_ThumbnailCacheId", &thumbnail_cache_id_column, sizeof( thumbnail_cache_id_column ), JET_ColInfo ) ) != JET_errSuccess )
	{
		if ( err == JET_errColumnNotFound ) { error_offset = sprintf_s( error, 1024, "The System_ThumbnailCacheId column was not found.\r\n\r\n" ); }
		goto CLEANUP;
	}

	if ( ( err = JetGetTableColumnInfo( sesid, tableid, "System_ItemPathDisplay", &item_path_display_column, sizeof( item_path_display_column ), JET_ColInfo ) ) != JET_errSuccess )
	{
		if ( err == JET_errColumnNotFound ) { error_offset = sprintf_s( error, 1024, "The System_ItemPathDisplay column was not found.\r\n\r\n" ); }
		goto CLEANUP;
	}

	if ( ( err = JetGetTableColumnInfo( sesid, tableid, "System_FileExtension", &file_extension_column, sizeof( file_extension_column ), JET_ColInfo ) ) != JET_errSuccess )
	{
		if ( err == JET_errColumnNotFound ) { error_offset = sprintf_s( error, 1024, "The System_FileExtension column was not found.\r\n\r\n" ); }
		goto CLEANUP;
	}

	if ( ( err = JetGetTableColumnInfo( sesid, tableid, "System_FileAttributes", &file_attributes_column, sizeof( file_attributes_column ), JET_ColInfo ) ) != JET_errSuccess )
	{
		if ( err == JET_errColumnNotFound ) { error_offset = sprintf_s( error, 1024, "The System_FileAttributes column was not found.\r\n\r\n" ); }
		goto CLEANUP;
	}

	// Ensure that the values we receive are of the correct size.
	if ( thumbnail_cache_id_column.cbMax != sizeof( unsigned long long ) || file_attributes_column.cbMax != sizeof( DWORD ) || item_path_display_column.cbMax == 0 ) { goto CLEANUP; }

	item_path_display = ( char * )malloc( sizeof( char ) * ( item_path_display_column.cbMax + 2 ) );
	memset( item_path_display, 0, sizeof( char ) * ( item_path_display_column.cbMax + 2 ) );

	file_extension = ( char * )malloc( sizeof( char ) * ( file_extension_column.cbMax + 2 ) );
	memset( file_extension, 0, sizeof( char ) * ( file_extension_column.cbMax + 2 ) );

	// Set up the column info we want to retrieve.
	rc[ 0 ].columnid = thumbnail_cache_id_column.columnid;
	rc[ 0 ].pvData = ( void * )&thumbnail_cache_id;
	rc[ 0 ].cbData = thumbnail_cache_id_column.cbMax;
	rc[ 0 ].itagSequence = 1;

	rc[ 1 ].columnid = item_path_display_column.columnid;
	rc[ 1 ].pvData = ( void * )item_path_display;
	rc[ 1 ].cbData = item_path_display_column.cbMax;
	rc[ 1 ].itagSequence = 1;

	rc[ 2 ].columnid = file_extension_column.columnid;
	rc[ 2 ].pvData = ( void * )file_extension;
	rc[ 2 ].cbData = file_extension_column.cbMax;
	rc[ 2 ].itagSequence = 1;

	rc[ 3 ].columnid = file_attributes_column.columnid;
	rc[ 3 ].pvData = ( void * )&file_attributes;
	rc[ 3 ].cbData = file_attributes_column.cbMax;
	rc[ 3 ].itagSequence = 1;

	if ( JetMove( sesid, tableid, JET_MoveFirst, 0 ) != JET_errSuccess ) { goto CLEANUP; }

	while ( true )
	{
		// We don't want to continue scanning if the user cancels the scan.
		if ( kill_scan == true )
		{
			break;
		}

		// Retrieve the 4 column values.
		if ( JetRetrieveColumns( sesid, tableid, rc, 4 ) != JET_errSuccess )
		{
			break;
		}

		// The file path should be an unterminated Unicode string.
		item_path_display[ rc[ 1 ].cbActual ] = 0;
		item_path_display[ rc[ 1 ].cbActual + 1 ] = 0;

		// The file extension should be an unterminated Unicode string.
		file_extension[ rc[ 2 ].cbActual ] = 0;
		file_extension[ rc[ 2 ].cbActual + 1 ] = 0;

		// Swap the byte order of the hash.
		hash = ntohll( thumbnail_cache_id );

		// See if the entry is a folder.
		if ( ( file_attributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
		{
			if ( include_folders == false )
			{
				if ( JetMove( sesid, tableid, JET_MoveNext, 0 ) != JET_errSuccess )
				{
					break;
				}

				continue;
			}
		}
		else
		{
			// See if the file's extension is in our filter. Go to the next entry if it's not.
			wchar_t *ext = ( wchar_t * )file_extension;
			if ( extension_filter[ 0 ] != 0 )
			{
				// Do a case-insensitive substring search for the extension.
				int ext_length = wcslen( ext );
				wchar_t *temp_ext = ( wchar_t * )malloc( sizeof( wchar_t ) * ( ext_length + 3 ) );
				for ( int i = 0; i < ext_length; ++i )
				{
					temp_ext[ i + 1 ] = towlower( ext[ i ] );
				}
				temp_ext[ 0 ] = L'|';				// Append the delimiter to the beginning of the string.
				temp_ext[ ext_length + 1 ] = L'|';	// Append the delimiter to the end of the string.
				temp_ext[ ext_length + 2 ] = L'\0';

				if ( wcsstr( extension_filter, temp_ext ) == NULL )
				{
					free( temp_ext );

					// Move to the next record (column row).
					if ( JetMove( sesid, tableid, JET_MoveNext, 0 ) != JET_errSuccess )
					{
						break;
					}

					continue;
				}

				free( temp_ext );
			}
		}

		update_scan_info( hash, ( wchar_t * )item_path_display );

		// Move to the next record (column row).
		if ( JetMove( sesid, tableid, JET_MoveNext, 0 ) != JET_errSuccess )
		{
			break;
		}
	}

CLEANUP:

	if ( sesid != JET_sesidNil )
	{
		if ( tableid != JET_tableidNil )
		{
			JetCloseTable( sesid, tableid );
		}

		if ( error_state < 2 )
		{
			if ( error_state < 1 )
			{
				JetCloseDatabase( sesid, dbid, JET_bitNil );
			}

			JetDetachDatabase( sesid, ascii_filepath );	// If ascii_filepath is NULL, then all databases are detached from the session.
		}

		JetEndSession( sesid, 0 );
	}

	if ( instance != JET_instanceNil )
	{
		JetTerm( instance );
	}

	free( item_path_display );
	free( file_extension );
	free( ascii_filepath );
	free( partial_header );

	if ( err != JET_errSuccess )
	{
		JET_API_PTR error_value = err;
		// It would be nice to know how big this buffer is supposed to be. It seems to silently fail if it's not big enough...thankfully.
		if ( JetGetSystemParameter( NULL, JET_sesidNil, JET_paramErrorToString, &error_value, error + error_offset, sizeof( error ) - error_offset ) != JET_errBufferTooSmall )
		{
			char *search = strchr( error + error_offset, ',' );
			if ( search != NULL )
			{
				*search = ':';
			}
		}

		MessageBoxA( g_hWnd_scan, error, PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
	}
}

unsigned __stdcall scan_files( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	SetWindowTextA( g_hWnd_scan, "Map File Paths to Cache Entry Hashes - Please wait..." );	// Update the window title.
	SendMessage( g_hWnd_scan, WM_CHANGE_CURSOR, TRUE, 0 );	// SetCursor only works from the main thread. Set it to an arrow with hourglass.

	// 0 = scan directories, 1 = scan ese database
	unsigned char scan_type = ( unsigned char )pArguments;

	// Disable scan button, enable cancel button.
	SendMessage( g_hWnd_scan, WM_PROPAGATE, 1, 0 );

	create_fileinfo_tree();

	if ( scan_type == 0 )
	{
		// File path will be at least 2 characters. Copy our drive to get the volume GUID.
		wchar_t drive[ 4 ] = { 0 };
		wchar_t volume_guid[ 50 ] = { 0 };

		wmemcpy_s( drive, 4, g_filepath, 2 );
		drive[ 2 ] = L'\\';	// Ensure the drive ends with "\".
		drive[ 3 ] = L'\0';

		// Get the volume GUID first.
		if ( GetVolumeNameForVolumeMountPoint( drive, volume_guid, 50 ) == TRUE )
		{
			volume_guid[ 48 ] = L'\0';
			CLSIDFromString( ( LPOLESTR )( volume_guid + 10 ), &clsid );

			traverse_directory( g_filepath );
		}
		else
		{
			MessageBoxA( g_hWnd_scan, "Volume name could not be found.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
		}
	}
	else
	{
		traverse_ese_database();
	}

	InvalidateRect( g_hWnd_list, NULL, TRUE );

	// Update the details.
	if ( show_details == false )
	{
		char msg[ 11 ] = { 0 };
		sprintf_s( msg, 11, "%lu", file_count );
		SendMessageA( g_hWnd_scan, WM_PROPAGATE, 5, ( LPARAM )msg );
	}

	// Reset button and text.
	SendMessage( g_hWnd_scan, WM_PROPAGATE, 2, 0 );

	if ( match_count > 0 )
	{
		char msg[ 30 ] = { 0 };
		sprintf_s( msg, 30, "%d file%s mapped.", match_count, ( match_count > 1 ? "s were" : " was" ) );
		MessageBoxA( g_hWnd_scan, msg, PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONINFORMATION );
	}
	else
	{
		MessageBoxA( g_hWnd_scan, "No files were mapped.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONINFORMATION );
	}

	SendMessage( g_hWnd_scan, WM_CHANGE_CURSOR, FALSE, 0 );	// Reset the cursor.
	SetWindowTextA( g_hWnd_scan, "Map File Paths to Cache Entry Hashes" );	// Reset the window title.

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

bool scan_memory( HANDLE hFile, unsigned int &offset )
{
	// Allocate a 32 kilobyte chunk of memory to scan. This value is arbitrary.
	char *buf = ( char * )malloc( sizeof( char ) * 32768 );
	char *scan = NULL;
	DWORD read = 0;

	while ( true )
	{
		// Begin reading through the database.
		ReadFile( hFile, buf, sizeof( char ) * 32768, &read, NULL );
		if ( read <= 4 )
		{
			free( buf );
			return false;
		}

		// Binary string search. Look for the magic identifier.
		scan = buf;
		while ( read-- > 4 && memcmp( scan++, "CMMM", 4 ) != 0 )
		{
			++offset;
		}

		// If it's not found, then we'll keep scanning.
		if ( read < 4 )
		{
			// Adjust the offset back 4 characters (in case we truncated the magic identifier when reading).
			SetFilePointer( hFile, offset, NULL, FILE_BEGIN );
			// Keep scanning.
			continue;
		}

		break;
	}

	free( buf );
	return true;
}

// This will allow our main thread to continue while secondary threads finish their processing.
unsigned __stdcall cleanup( void *pArguments )
{
	// This semaphore will be released when the thread gets killed.
	shutdown_semaphore = CreateSemaphore( NULL, 0, 1, NULL );

	kill_thread = true;	// Causes our secondary threads to cease processing and release the semaphore.

	// Wait for any active threads to complete. 5 second timeout in case we miss the release.
	WaitForSingleObject( shutdown_semaphore, 5000 );
	CloseHandle( shutdown_semaphore );
	shutdown_semaphore = NULL;

	// DestroyWindow won't work on a window from a different thread. So we'll send a message to trigger it.
	SendMessage( g_hWnd_main, WM_DESTROY_ALT, 0, 0 );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall remove_items( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;
	
	skip_draw = true;	// Prevent the listview from drawing while freeing lParam values.

	Processing_Window( true );

	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;

	fileinfo *fi = NULL;

	int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );
	int sel_count = SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 );

	// See if we've selected all the items. We can clear the list much faster this way.
	if ( item_count == sel_count )
	{
		// Go through each item, and free their lParam values. current_fileinfo will get deleted here.
		for ( int i = 0; i < item_count; ++i )
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
			{
				break;
			}

			// We first need to get the lParam value otherwise the memory won't be freed.
			lvi.iItem = i;
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			fi = ( fileinfo * )lvi.lParam;

			if ( fi != NULL )
			{
				if ( fi->si != NULL )
				{
					fi->si->count--;

					// Remove our shared information from the linked list if there's no more items for this database.
					if ( fi->si->count == 0 )
					{
						free( fi->si );
					}
				}

				// Free our filename, then fileinfo structure.
				free( fi->filename );
				free( fi );
			}
		}

		cleanup_fileinfo_tree();

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
			lvi.iItem = index_array[ sel_count - 1 - i ] = SendMessage( g_hWnd_list, LVM_GETNEXTITEM, lvi.iItem, LVNI_SELECTED );
		}

		for ( int i = 0; i < sel_count; i++ )
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
			{
				break;
			}

			// We first need to get the lParam value otherwise the memory won't be freed.
			lvi.iItem = index_array[ i ];
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			fi = ( fileinfo * )lvi.lParam;

			if ( fi != NULL )
			{
				// Remove the fileinfo from the fileinfo tree if it exists in it.
				if ( fi->flag & FIF_IN_TREE )
				{
					// First find the fileinfo to remove from the fileinfo tree.
					dllrbt_iterator *itr = dllrbt_find( fileinfo_tree, ( void * )fi->entry_hash, false );
					if ( itr != NULL )
					{
						// Head of the linked list.
						linked_list *ll = ( linked_list * )( ( node_type * )itr )->val;

						// Go through each linked list node and remove the one with fi.
						linked_list *current_node = ll;
						linked_list *last_node = NULL;
						while ( current_node != NULL )
						{
							if ( current_node->fi == fi )
							{
								if ( last_node == NULL )	// Remove head. (current_node == ll)
								{
									ll = current_node->next;
								}
								else
								{
									last_node->next = current_node->next;
								}

								free( current_node );

								if ( ll != NULL && ll->fi != NULL )
								{
									// Reset the head in the tree.
									( ( node_type * )itr )->val = ( void * )ll;
									( ( node_type * )itr )->key = ( void * )ll->fi->entry_hash;
								}

								break;
							}

							last_node = current_node;
							current_node = current_node->next;
						}

						// If the head of the linked list is NULL, then we can remove the linked list from the fileinfo tree.
						if ( ll == NULL )
						{
							dllrbt_remove( fileinfo_tree, itr );	// Remove the node from the tree. The tree will rebalance itself.
						}
					}
				}

				if ( fi->si != NULL )
				{
					fi->si->count--;

					// Remove our shared information from the linked list if there's no more items for this database.
					if ( fi->si->count == 0 )
					{
						free( fi->si );
					}
				}
				
				// Free our filename, then fileinfo structure.
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
	if ( shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( shutdown_semaphore, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall show_hide_items( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	Processing_Window( true );

	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;

	int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

	if ( hide_blank_entries == false )	// Display the blank entries.
	{
		// This will reinsert the blank entry at the end of the listview.
		linked_list *be = g_be;
		linked_list *del_be = NULL;
		g_be = NULL;
		while ( be != NULL )
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
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
		for ( int i = item_count - 1; i >= 0; --i )
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
			{
				break;
			}

			lvi.iItem = i;
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			// If the list item is blank, then add it to the blank entry linked list.
			if ( lvi.lParam != NULL && ( ( fileinfo * )lvi.lParam )->size == 0 )
			{
				linked_list *be = ( linked_list * )malloc( sizeof( linked_list ) );
				be->fi = ( fileinfo * )lvi.lParam;
				be->next = g_be;

				g_be = be;

				// Remove the list item.
				SendMessage( g_hWnd_list, LVM_DELETEITEM, i, 0 );
			}
		}
	}

	Processing_Window( false );

	// Release the semaphore if we're killing the thread.
	if ( shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( shutdown_semaphore, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall verify_checksums( void *pArguments )
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

	fileinfo *fi = NULL;

	int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

	// Go through each item to compare values.
	for ( int i = 0; i < item_count; ++i )
	{
		// Stop processing and exit the thread.
		if ( kill_thread == true )
		{
			break;
		}

		lvi.iItem = i;
		SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

		fi = ( fileinfo * )lvi.lParam;
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
	if ( shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( shutdown_semaphore, 1, NULL );
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
			int save_items = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

			// Retrieve the lParam value from the selected listview item.
			LVITEM lvi = { NULL };
			lvi.mask = LVIF_PARAM;

			fileinfo *fi = NULL;

			// Go through all the items we'll be saving.
			for ( int i = 0; i < save_items; ++i )
			{
				// Stop processing and exit the thread.
				if ( kill_thread == true )
				{
					break;
				}

				lvi.iItem = i;
				SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

				fi = ( fileinfo * )lvi.lParam;
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

					case WINDOWS_VISTA:
					{
						system_string = "Windows Vista";
					}
					break;

					default:
					{
						system_string = "Unknown";
					}
					break;
				}

				// See if the next entry can fit in the buffer. If it can't, then we dump the buffer.
				if ( write_buf_offset + filename_length + dbpath_length + ( 10 * 4 ) + ( 20 * 5 ) + 13 + 31 > size )
				{
					// Dump the buffer.
					WriteFile( hFile, write_buf, write_buf_offset, &write, NULL );
					write_buf_offset = 0;
				}

				write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, "\r\n\"%s\",%lu,%lu,%lu,%lu,0x%016llx",
											   utf8_filename,
											   fi->header_offset, fi->size + ( fi->data_offset - fi->header_offset ),
											   fi->data_offset, fi->size,
											   fi->data_checksum );
											  
				if ( fi->v_data_checksum != fi->data_checksum )
				{
					write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, " : 0x%016llx",
											   fi->v_data_checksum );
				}

				write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, ",0x%016llx",
											   fi->header_checksum );

				if ( fi->v_header_checksum != fi->header_checksum )
				{
					write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, " : 0x%016llx",
											   fi->v_header_checksum );
				}

				write_buf_offset += sprintf_s( write_buf + write_buf_offset, size - write_buf_offset, ",0x%016llx,%s,\"%s\"",
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
	if ( shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( shutdown_semaphore, 1, NULL );
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

	save_param *save_type = ( save_param * )pArguments;
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
		int save_items = ( save_type->save_all == true ? SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) : SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 ) );

		// Retrieve the lParam value from the selected listview item.
		LVITEM lvi = { NULL };
		lvi.mask = LVIF_PARAM;
		lvi.iItem = -1;	// Set this to -1 so that the LVM_GETNEXTITEM call can go through the list correctly.

		fileinfo *fi = NULL;

		// Go through all the items we'll be saving.
		for ( int i = 0; i < save_items; ++i )
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
			{
				break;
			}

			lvi.iItem = ( save_type->save_all == true ? i : SendMessage( g_hWnd_list, LVM_GETNEXTITEM, lvi.iItem, LVNI_SELECTED ) );
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			fi = ( fileinfo * )lvi.lParam;
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

					// Directory + backslash + filename + extension + NULL character = ( 2 * MAX_PATH ) + 6
					wchar_t fullpath[ ( 2 * MAX_PATH ) + 6 ] = { 0 };

					wchar_t *filename = get_filename_from_path( fi->filename, wcslen( fi->filename ) );

					if ( fi->flag & FIF_TYPE_BMP )
					{
						wchar_t *ext = get_extension_from_filename( filename, wcslen( filename ) );
						// The extension in the filename might not be the actual type. So we'll append .bmp to the end of it.
						if ( _wcsicmp( ext, L".bmp" ) == 0 )
						{
							swprintf_s( fullpath, ( 2 * MAX_PATH ) + 6, L"%s\\%.259s", save_directory, filename );
						}
						else
						{
							swprintf_s( fullpath, ( 2 * MAX_PATH ) + 6, L"%s\\%.259s.bmp", save_directory, filename );
						}
					}
					else if ( fi->flag & FIF_TYPE_JPG )
					{
						wchar_t *ext = get_extension_from_filename( filename, wcslen( filename ) );
						// The extension in the filename might not be the actual type. So we'll append .jpg to the end of it.
						if ( _wcsicmp( ext, L".jpg" ) == 0 || _wcsicmp( ext, L".jpeg" ) == 0 )
						{
							swprintf_s( fullpath, ( 2 * MAX_PATH ) + 6, L"%s\\%.259s", save_directory, filename );
						}
						else
						{
							swprintf_s( fullpath, ( 2 * MAX_PATH ) + 6, L"%s\\%.259s.jpg", save_directory, filename );
						}
					}
					else if ( fi->flag & FIF_TYPE_PNG )
					{
						wchar_t *ext = get_extension_from_filename( filename, wcslen( filename ) );
						// The extension in the filename might not be the actual type. So we'll append .png to the end of it.
						if ( _wcsicmp( ext, L".png" ) == 0 )
						{
							swprintf_s( fullpath, ( 2 * MAX_PATH ) + 6, L"%s\\%.259s", save_directory, filename );
						}
						else
						{
							swprintf_s( fullpath, ( 2 * MAX_PATH ) + 6, L"%s\\%.259s.png", save_directory, filename );
						}
					}
					else
					{
						swprintf_s( fullpath, ( 2 * MAX_PATH ) + 6, L"%s\\%.259s", save_directory, filename );
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
	if ( shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( shutdown_semaphore, 1, NULL );
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

unsigned __stdcall read_database( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	// Protects our global variables.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	Processing_Window( true );

	pathinfo *pi = ( pathinfo * )pArguments;
	if ( pi != NULL && pi->filepath != NULL )
	{
		int fname_length = 0;
		wchar_t *fname = pi->filepath + pi->offset;

		int filepath_length = wcslen( pi->filepath ) + 1;	// Include NULL character.
		
		bool construct_filepath = ( filepath_length > pi->offset && cmd_line == 0 ? false : true );

		wchar_t *filepath = NULL;

		// We're going to open each file in the path info.
		do
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
			{
				break;
			}

			// Construct the filepath for each file.
			if ( construct_filepath == true )
			{
				fname_length = wcslen( fname ) + 1;	// Include '\' character or NULL character

				if ( cmd_line != 0 )
				{
					filepath = ( wchar_t * )malloc( sizeof( wchar_t ) * fname_length );
					wcscpy_s( filepath, fname_length, fname );
				}
				else
				{
					filepath = ( wchar_t * )malloc( sizeof( wchar_t ) * ( filepath_length + fname_length ) );
					swprintf_s( filepath, filepath_length + fname_length, L"%s\\%s", pi->filepath, fname );
				}

				// Move to the next file name.
				fname += fname_length;
			}
			else	// Copy the filepath.
			{
				filepath = ( wchar_t * )malloc( sizeof( wchar_t ) * filepath_length );
				wcscpy_s( filepath, filepath_length, pi->filepath );
			}

			// Attempt to open our database file.
			HANDLE hFile = CreateFile( filepath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
			if ( hFile != INVALID_HANDLE_VALUE )
			{
				shared_info *si = NULL;
				DWORD read = 0;
				int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );	// We don't need to call this for each item.

				database_header dh = { 0 };
				ReadFile( hFile, &dh, sizeof( database_header ), &read, NULL );

				// Make sure it's a thumbcache database and the stucture was filled correctly.
				if ( memcmp( dh.magic_identifier, "CMMM", 4 ) != 0 || read != sizeof( database_header ) )
				{
					CloseHandle( hFile );
					free( filepath );

					if ( cmd_line != 2 ){ MessageBoxA( g_hWnd_main, "The file is not a thumbcache database.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING ); }

					continue;
				}

				// Set the file pointer to the first possible cache entry. (Should be at an offset equal to the size of the header)
				// current_position will keep track our our file pointer position before setting the file pointer. (ReadFile sets it as well)
				unsigned int current_position = ( dh.version != WINDOWS_8v2 ? 24 : 28 );

				bool next_file = false;	// Go to the next database file.
				unsigned int header_offset = 0;

				// Go through our database and attempt to extract each cache entry.
				while ( true )
				{
					// Stop processing and exit the thread.
					if ( kill_thread == true )
					{
						free( filepath );

						next_file = true;
						break;
					}

					// Set the file pointer to the end of the last cache entry.
					current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );
					if ( current_position == INVALID_SET_FILE_POINTER )
					{
						free( filepath );

						if ( cmd_line != 2 ){ MessageBoxA( g_hWnd_main, "Invalid cache entry.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING ); }

						next_file = true;
						break;
					}

					void *database_cache_entry = NULL;
					// Determine the type of database we're working with and store its content in the correct structure.
					if ( dh.version == WINDOWS_7 )
					{
						database_cache_entry = ( database_cache_entry_7 * )malloc( sizeof( database_cache_entry_7 ) );
						ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_7 ), &read, NULL );
						
						// Make sure it's a thumbcache database and the stucture was filled correctly.
						if ( read != sizeof( database_cache_entry_7 ) )
						{
							// EOF reached.
							free( database_cache_entry );
							free( filepath );

							next_file = true;
							break;
						}
						else if ( memcmp( ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
						{
							free( database_cache_entry );

							// Walk back to the end of the last cache entry.
							current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );

							// If we found the beginning of the entry, attempt to read it again.
							if ( scan_memory( hFile, current_position ) == true )
							{
								continue;
							}

							free( filepath );

							next_file = true;
							break;
						}
					}
					else if ( dh.version == WINDOWS_VISTA )
					{
						database_cache_entry = ( database_cache_entry_vista * )malloc( sizeof( database_cache_entry_vista ) );
						ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_vista ), &read, NULL );
						// Make sure it's a thumbcache database and the stucture was filled correctly.
						if ( read != sizeof( database_cache_entry_vista ) )
						{
							// EOF reached.
							free( database_cache_entry );
							free( filepath );

							next_file = true;
							break;
						}
						else if ( memcmp( ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
						{
							free( database_cache_entry );

							// Walk back to the end of the last cache entry.
							current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );

							// If we found the beginning of the entry, attempt to read it again.
							if ( scan_memory( hFile, current_position ) == true )
							{
								continue;
							}

							free( filepath );

							next_file = true;
							break;
						}
					}
					else if ( dh.version == WINDOWS_8 || dh.version == WINDOWS_8v2 || dh.version == WINDOWS_8v3 || dh.version == WINDOWS_8_1 )
					{
						database_cache_entry = ( database_cache_entry_8 * )malloc( sizeof( database_cache_entry_8 ) );
						ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_8 ), &read, NULL );
						
						// Make sure it's a thumbcache database and the stucture was filled correctly.
						if ( read != sizeof( database_cache_entry_8 ) )
						{
							// EOF reached.
							free( database_cache_entry );
							free( filepath );

							next_file = true;
							break;
						}
						else if ( memcmp( ( ( database_cache_entry_8 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
						{
							free( database_cache_entry );

							// Walk back to the end of the last cache entry.
							current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );

							// If we found the beginning of the entry, attempt to read it again.
							if ( scan_memory( hFile, current_position ) == true )
							{
								continue;
							}

							free( filepath );

							next_file = true;
							break;
						}
					}
					else	// If this is true, then the file isn't from Vista, 7, or 8 and not supported by this program.
					{
						free( filepath );

						if ( cmd_line != 2 ){ MessageBoxA( g_hWnd_main, "The file is not supported by this program.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING ); }

						next_file = true;
						break;
					}

					// I think this signifies the end of a valid database and everything beyond this is data that's been overwritten.
					if ( ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->entry_hash : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->entry_hash : ( ( database_cache_entry_8 * )database_cache_entry )->entry_hash ) ) == 0 )
					{
						// Skip the header of this entry. If the next position is invalid (which it probably will be), we'll end up scanning.
						current_position += read;
						// Free each database entry that we've skipped over.
						free( database_cache_entry );

						continue;
					}

					header_offset = current_position;

					// Size of the cache entry.
					unsigned int cache_entry_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->cache_entry_size : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->cache_entry_size : ( ( database_cache_entry_8 * )database_cache_entry )->cache_entry_size ) );

					current_position += cache_entry_size;

					// Filename length should be the total number of bytes (excluding the NULL character) that the UTF-16 filename takes up. A realistic limit should be twice the size of MAX_PATH.
					unsigned int filename_length = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->filename_length : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->filename_length : ( ( database_cache_entry_8 * )database_cache_entry )->filename_length ) );

					// Since the database can store CLSIDs that extend beyond MAX_PATH, we'll have to set a larger truncation length. A length of 32767 would probably never be seen. 
					unsigned int filename_truncate_length = min( filename_length, ( sizeof( wchar_t ) * SHRT_MAX ) );
					
					// UTF-16 filename. Allocate the filename length plus 6 for the unicode extension and null character. This will get deleted before MainWndProc is destroyed. See WM_DESTROY in MainWndProc.
					wchar_t *filename = ( wchar_t * )malloc( filename_truncate_length + ( sizeof( wchar_t ) * 6 ) );
					memset( filename, 0, filename_truncate_length + ( sizeof( wchar_t ) * 6 ) );
					ReadFile( hFile, filename, filename_truncate_length, &read, NULL );
					if ( read == 0 )
					{
						free( filename );
						free( database_cache_entry );
						free( filepath );
						
						if ( cmd_line != 2 )
						{
							char msg[ 49 ] = { 0 };
							sprintf_s( msg, 49, "Invalid cache entry located at %lu bytes.", current_position );
							MessageBoxA( g_hWnd_main, msg, PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
						}

						next_file = true;
						break;
					}

					unsigned int file_position = 0;

					// Adjust our file pointer if we truncated the filename. This really shouldn't happen unless someone tampered with the database, or it became corrupt.
					if ( filename_length > filename_truncate_length )
					{
						// Offset the file pointer and see if we've moved beyond the EOF.
						file_position = SetFilePointer( hFile, filename_length - filename_truncate_length, 0, FILE_CURRENT );
						if ( file_position == INVALID_SET_FILE_POINTER )
						{
							free( filename );
							free( database_cache_entry );
							free( filepath );

							if ( cmd_line != 2 )
							{
								char msg[ 49 ] = { 0 };
								sprintf_s( msg, 49, "Invalid cache entry located at %lu bytes.", current_position );
								MessageBoxA( g_hWnd_main, msg, PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
							}
							
							next_file = true;
							break;
						}
					}

					// Padding before the data entry.
					unsigned int padding_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->padding_size : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->padding_size : ( ( database_cache_entry_8 * )database_cache_entry )->padding_size ) );

					// This will set our file pointer to the beginning of the data entry.
					file_position = SetFilePointer( hFile, padding_size, 0, FILE_CURRENT );
					if ( file_position == INVALID_SET_FILE_POINTER )
					{
						free( filename );
						free( database_cache_entry );
						free( filepath );

						if ( cmd_line != 2 )
						{
							char msg[ 49 ] = { 0 };
							sprintf_s( msg, 49, "Invalid cache entry located at %lu bytes.", current_position );
							MessageBoxA( g_hWnd_main, msg, PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
						}

						next_file = true;
						break;
					}

					// Size of our image.
					unsigned int data_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_size : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->data_size : ( ( database_cache_entry_8 * )database_cache_entry )->data_size ) );

					// Create a new info structure to send to the listview item's lParam value.
					fileinfo *fi = ( fileinfo * )malloc( sizeof( fileinfo ) );
					fi->flag = 0;
					fi->header_offset = header_offset;
					fi->data_offset = file_position;
					fi->size = data_size;

					fi->entry_hash = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->entry_hash : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->entry_hash : ( ( database_cache_entry_8 * )database_cache_entry )->entry_hash ) );
					fi->data_checksum = fi->v_data_checksum = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_checksum : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->data_checksum : ( ( database_cache_entry_8 * )database_cache_entry )->data_checksum ) );
					fi->header_checksum = fi->v_header_checksum = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->header_checksum : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->header_checksum : ( ( database_cache_entry_8 * )database_cache_entry )->header_checksum ) );

					// Read any data that exists and get its file extension.
					if ( data_size != 0 )
					{
						// Retrieve the data content header. Our longest identifier is 8 bytes.
						char *buf = ( char * )malloc( sizeof( char ) * 8 );
						ReadFile( hFile, buf, 8, &read, NULL );
						if ( read == 0 )
						{
							free( buf );
							free( fi );
							free( filename );
							free( database_cache_entry );
							free( filepath );

							if ( cmd_line != 2 )
							{
								char msg[ 49 ] = { 0 };
								sprintf_s( msg, 49, "Invalid cache entry located at %lu bytes.", current_position );
								MessageBoxA( g_hWnd_main, msg, PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING );
							}

							next_file = true;
							break;
						}

						// Detect the file extension and copy it into the filename string.
						if ( memcmp( buf, FILE_TYPE_BMP, 2 ) == 0 )			// First 3 bytes
						{
							wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".bmp", 4 );
							fi->flag = FIF_TYPE_BMP;
						}
						else if ( memcmp( buf, FILE_TYPE_JPEG, 4 ) == 0 )	// First 4 bytes
						{
							wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".jpg", 4 );
							fi->flag = FIF_TYPE_JPG;
						}
						else if ( memcmp( buf, FILE_TYPE_PNG, 8 ) == 0 )	// First 8 bytes
						{
							wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".png", 4 );
							fi->flag = FIF_TYPE_PNG;
						}
						else if ( dh.version == WINDOWS_VISTA && ( ( database_cache_entry_vista * )database_cache_entry )->extension[ 0 ] != NULL )	// If it's a Windows Vista thumbcache file and we can't detect the extension, then use the one given.
						{
							wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 1, L".", 1 );
							wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ) + 1, 4, ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 );
						}

						// Free our data buffer.
						free( buf );

						// This will set our file pointer to the end of the data entry.
						SetFilePointer( hFile, data_size - 8, 0, FILE_CURRENT );
					}
					else	// No data exists.
					{
						// Windows Vista thumbcache files should include the extension.
						if ( dh.version == WINDOWS_VISTA && ( ( database_cache_entry_vista * )database_cache_entry )->extension[ 0 ] != NULL )
						{
							wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 1, L".", 1 );
							wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ) + 1, 4, ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 );
						}
					}

					fi->filename = filename;	// Gets deleted during shutdown.

					// Do this only once for each database, and only if we have an entry to add to the listview.
					if ( si == NULL )
					{
						// This information is shared between entries within the database.
						si = ( shared_info * )malloc( sizeof( shared_info ) );
						si->count = 0;
						si->system = dh.version;
						wcscpy_s( si->dbpath, MAX_PATH, filepath );
					}

					// Increase the number of items for our shared information.
					si->count++;

					// The operating system and database location is shared among each entry for the database. This will reduce the amount of memory used.
					fi->si = si;

					// Add blank entries to our blank entries linked list.
					if ( hide_blank_entries == true && fi->size == 0 )
					{
						linked_list *be = ( linked_list * )malloc( sizeof( linked_list ) );
						be->fi = fi;
						be->next = g_be;

						g_be = be;
					}
					else
					{
						// Insert a row into our listview.
						LVITEM lvi = { NULL };
						lvi.mask = LVIF_PARAM; // Our listview items will display the text contained the lParam value.
						lvi.iItem = item_count++;
						lvi.iSubItem = 0;
						lvi.lParam = ( LPARAM )fi;
						SendMessage( g_hWnd_list, LVM_INSERTITEM, 0, ( LPARAM )&lvi );
					}

					// Free our database cache entry.
					free( database_cache_entry );
				}
				// Close the input file.
				CloseHandle( hFile );

				if ( next_file == true )
				{
					continue;
				}
			}
			else
			{
				// If this occurs, then there's something wrong with the user's system.
				if ( cmd_line != 2 ){ MessageBoxA( g_hWnd_main, "The database file failed to open.", PROGRAM_CAPTION_A, MB_APPLMODAL | MB_ICONWARNING ); }
			}

			// Free the old filepath.
			free( filepath );
		}
		while ( construct_filepath == true && *fname != L'\0' );

		// Save the files or a CSV if the user specified an output directory through the command-line.
		if ( pi->output_path != NULL )
		{
			if ( pi->type == 0 )	// Save thumbnail images.
			{
				save_param *save_type = ( save_param * )malloc( sizeof( save_param ) );
				save_type->type = 1;	// Build directory. It may not exist.
				save_type->save_all = true;
				save_type->filepath = pi->output_path;

				// save_type is freed in the save_items thread.
				CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &save_items, ( void * )save_type, 0, NULL ) );
			}
			else	// Save CSV.
			{
				// output_path is freed in save_csv.
				CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &save_csv, ( void * )pi->output_path, 0, NULL ) );
			}
		}

		free( pi->filepath );
	}
	else if ( pi != NULL )	// filepath == NULL
	{
		free( pi->output_path );	// Assume output_path is set.
	}

	free( pi );

	Processing_Window( false );

	// Release the semaphore if we're killing the thread.
	if ( shutdown_semaphore != NULL )
	{
		ReleaseSemaphore( shutdown_semaphore, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}
