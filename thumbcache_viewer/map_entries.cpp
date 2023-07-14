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

#include "map_entries.h"
#include "globals.h"
#include "utilities.h"

#include "lite_mssrch.h"
#include "lite_msscb.h"
#include "lite_sqlite3.h"

#include "read_esedb.h"
#include "read_sqlitedb.h"

#include <stdio.h>

wchar_t g_filepath[ MAX_PATH ] = { 0 };				// Path to the files and folders to scan.
wchar_t g_extension_filter[ MAX_PATH + 2 ] = { 0 };	// A list of extensions to filter from a file scan.

bool g_include_folders = false;						// Include folders in a file scan.
bool g_retrieve_extended_information = false;		// Retrieve additional columns from Windows.edb
bool g_show_details = false;						// Show details in the scan window.

bool g_kill_scan = true;							// Stop a file scan.

bool is_win_7_or_higher = true;						// Windows Vista uses a different file hashing algorithm.
bool is_win_8_1_or_higher = true;					// Windows 8.1 uses a different file hashing algorithm.

CLSID clsid;										// Holds a drive's Volume GUID.
unsigned int g_file_count = 0;						// Number of files scanned.
unsigned int g_match_count = 0;						// Number of files that match an entry hash.

#define _WIN32_WINNT_WIN7		0x0601
//#define _WIN32_WINNT_WIN8		0x0602
#define _WIN32_WINNT_WINBLUE	0x0603

BOOL IsWindowsVersionOrGreater( WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor )
{
	OSVERSIONINFOEXW osvi = { sizeof( osvi ), 0, 0, 0, 0, { 0 }, 0, 0 };
	DWORDLONG const dwlConditionMask = VerSetConditionMask(
		VerSetConditionMask(
		VerSetConditionMask(
		0, VER_MAJORVERSION, VER_GREATER_EQUAL ),
		   VER_MINORVERSION, VER_GREATER_EQUAL ),
		   VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );

	osvi.dwMajorVersion = wMajorVersion;
	osvi.dwMinorVersion = wMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;

	return VerifyVersionInfoW( &osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask ) != FALSE;
}

BOOL IsWindows7OrGreater()
{
    return IsWindowsVersionOrGreater( HIBYTE( _WIN32_WINNT_WIN7 ), LOBYTE( _WIN32_WINNT_WIN7 ), 0 );
}

/*BOOL IsWindows8OrGreater()
{
	return IsWindowsVersionOrGreater( HIBYTE( _WIN32_WINNT_WIN8 ), LOBYTE( _WIN32_WINNT_WIN8 ), 0 );
}*/

BOOL IsWindows8Point1OrGreater()
{
	return IsWindowsVersionOrGreater( HIBYTE( _WIN32_WINNT_WINBLUE ), LOBYTE( _WIN32_WINNT_WINBLUE ), 0 );
}

void UpdateWindowInfo( unsigned long long hash, wchar_t *filepath )
{
	++g_file_count; 

	// Update our scan window with new scan information.
	if ( g_show_details )
	{
		SendMessage( g_hWnd_scan, WM_PROPAGATE, 3, ( LPARAM )filepath );
		char buf[ 17 ] = { 0 };
		sprintf_s( buf, 17, "%016llx", hash );
		SendMessageA( g_hWnd_scan, WM_PROPAGATE, 4, ( LPARAM )buf );
		sprintf_s( buf, 17, "%lu", g_file_count );
		SendMessageA( g_hWnd_scan, WM_PROPAGATE, 5, ( LPARAM )buf );
	}
}

void UpdateFileinfo( unsigned long long hash, wchar_t *filepath )
{
	// Now that we have a hash value to compare, search our file info tree for the same value.
	LINKED_LIST *ll = ( LINKED_LIST * )dllrbt_find( g_file_info_tree, ( void * )hash, true );

	// Retrieve the column information once so we don't have to call this for duplicate entries in the loop below.
	bool got_columns = false;
	EXTENDED_INFO *ei = NULL;
	if ( g_retrieve_extended_information )
	{
		if ( ll != NULL && ll->fi != NULL )
		{
			if ( ll->fi->ei == NULL )
			{
				// Retrieve all the records associated with the matching System_ThumbnailCacheId.
				if ( ( g_err = JetRetrieveColumns( g_sesid, g_tableid_0A, g_rc_array, g_column_count ) ) == JET_errSuccess )
				{
					got_columns = true;
				}
			}
			else
			{
				// Save this to copy to any linked list nodes that have NULL extended info.
				ei = ll->fi->ei;
			}
		}
	}

	while ( ll != NULL )
	{
		if ( ll->fi != NULL )
		{
			++g_match_count;

			// Replace the hash filename with the local filename.
			wchar_t *del_filename = ll->fi->filename;
			ll->fi->filename = _wcsdup( filepath );
			free( del_filename );

			if ( got_columns )
			{
				// Convert the record values to strings so we can print them out.
				ConvertValues( &( ll->fi->ei ) );
			}
			else if ( ll->fi->ei == NULL )
			{
				// If we have extended information, but didn't retrieve any columns, then copy it to our file info linked list nodes.
				EXTENDED_INFO *l_ei = NULL;
				EXTENDED_INFO *t_ei = ei;
				while ( t_ei != NULL )
				{
					if ( t_ei->sei != NULL )
					{
						++( t_ei->sei->count );

						EXTENDED_INFO *c_ei = ( EXTENDED_INFO * )malloc( sizeof( EXTENDED_INFO ) );
						c_ei->sei = t_ei->sei;
						c_ei->property_value = _wcsdup( t_ei->property_value );
						c_ei->next = NULL;

						if ( l_ei != NULL )
						{
							l_ei->next = c_ei;
						}
						else
						{
							ll->fi->ei = c_ei;
						}

						l_ei = c_ei;
					}

					t_ei = t_ei->next;
				}
			}
		}

		ll = ll->next;
	}
}

unsigned long long HashData( char *data, unsigned long long hash, short length )
{
	while ( length-- > 0 )
	{
		hash ^= ( ( ( hash * 0x820 ) + ( *data++ & 0x00000000000000FF ) ) + ( hash >> 2 ) );
	}

	return hash;
}

void HashFile( wchar_t *filepath, wchar_t *extension )
{
	// Initial hash value. This value was found in shell32.dll.
	unsigned long long hash = 0x95E729BA2C37FD21;

	HANDLE hFile = CreateFile( filepath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		// Hash Volume GUID
		hash = HashData( ( char * )&clsid, hash, sizeof( CLSID ) );

		// Hash File ID - found in the Master File Table.
		BY_HANDLE_FILE_INFORMATION bhfi;
		GetFileInformationByHandle( hFile, &bhfi );
		CloseHandle( hFile );
		unsigned long long file_id = bhfi.nFileIndexHigh;
		file_id = ( file_id << 32 ) | bhfi.nFileIndexLow;

		hash = HashData( ( char * )&file_id, hash, sizeof( unsigned long long ) );

		// Windows Vista doesn't hash the file extension or modified DOS time.
		if ( is_win_7_or_higher )
		{
			// Hash Wide Character File Extension
			hash = HashData( ( char * )extension, hash, ( short )( wcslen( extension ) * sizeof( wchar_t ) ) );

			// Hash Last Modified DOS Time
			unsigned short fat_date;
			unsigned short fat_time;
			FileTimeToDosDateTime( &bhfi.ftLastWriteTime, &fat_date, &fat_time );
			unsigned int dos_time = fat_date;
			dos_time = ( dos_time << 16 ) | fat_time;

			hash = HashData( ( char * )&dos_time, hash, sizeof( unsigned int ) );

			// Windows 8.1 calculates the precision loss between the converted write time and original write time.
			if ( is_win_8_1_or_higher )
			{
				// Convert the DOS time back into a FILETIME.
				FILETIME converted_write_time;
				DosDateTimeToFileTime( fat_date, fat_time, &converted_write_time );

				// We only need to hash the low order int.
				unsigned int precision_loss = converted_write_time.dwLowDateTime - bhfi.ftLastWriteTime.dwLowDateTime;

				// Hash if there's any precision loss.
				if ( precision_loss != 0 )
				{
					hash = HashData( ( char * )&precision_loss, hash, sizeof( unsigned int ) );
				}
			}
		}

		UpdateFileinfo( hash, filepath );
		UpdateWindowInfo( hash, filepath );
	}
}

void TraverseDirectory( wchar_t *path )
{
	// We don't want to continue scanning if the user cancels the scan.
	if ( g_kill_scan )
	{
		return;
	}

	// Set the file path to search for all files/folders in the current directory.
	wchar_t filepath[ ( MAX_PATH * 2 ) + 2 ];
	swprintf_s( filepath, ( MAX_PATH * 2 ) + 2, L"%.259s\\*", path );

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFileEx( ( LPCWSTR )filepath, FindExInfoStandard, &FindFileData, FindExSearchNameMatch, NULL, 0 );
	if ( hFind != INVALID_HANDLE_VALUE ) 
	{
		do
		{
			if ( g_kill_scan )
			{
				break;	// We need to close the find file handle.
			}

			// See if the file is a directory.
			if ( ( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
			{
				// Go through all directories except "." and ".." (current and parent)
				if ( ( wcscmp( FindFileData.cFileName, L"." ) != 0 ) && ( wcscmp( FindFileData.cFileName, L".." ) != 0 ) )
				{
					// Move to the next directory. Limit the path length to MAX_PATH.
					if ( swprintf_s( filepath, ( MAX_PATH * 2 ) + 2, L"%.259s\\%.259s", path, FindFileData.cFileName ) < MAX_PATH )
					{
						TraverseDirectory( filepath );

						// Only hash folders if enabled.
						if ( g_include_folders )
						{
							HashFile( filepath, L"" );
						}
					}
				}
			}
			else
			{
				// See if the file's extension is in our filter. Go to the next file if it's not.
				wchar_t *ext = GetExtensionFromFilename( FindFileData.cFileName, ( unsigned long )wcslen( FindFileData.cFileName ) );
				if ( g_extension_filter[ 0 ] != 0 )
				{
					// Do a case-insensitive substring search for the extension.
					int ext_length = ( int )wcslen( ext );
					wchar_t *temp_ext = ( wchar_t * )malloc( sizeof( wchar_t ) * ( ext_length + 3 ) );
					for ( int i = 0; i < ext_length; ++i )
					{
						temp_ext[ i + 1 ] = towlower( ext[ i ] );
					}
					temp_ext[ 0 ] = L'|';				// Append the delimiter to the beginning of the string.
					temp_ext[ ext_length + 1 ] = L'|';	// Append the delimiter to the end of the string.
					temp_ext[ ext_length + 2 ] = L'\0';

					if ( wcsstr( g_extension_filter, temp_ext ) == NULL )
					{
						free( temp_ext );
						continue;
					}

					free( temp_ext );
				}

				if ( swprintf_s( filepath, ( MAX_PATH * 2 ) + 2, L"%.259s\\%.259s", path, FindFileData.cFileName ) >= MAX_PATH && FindFileData.cAlternateFileName[ 0 ] != 0 )
				{
					// See if the 8.3 filename can fit.
					swprintf_s( filepath, ( MAX_PATH * 2 ) + 2, L"%.259s\\%.259s", path, FindFileData.cAlternateFileName );
				}

				HashFile( filepath, ext );
			}
		}
		while ( FindNextFile( hFind, &FindFileData ) != 0 );	// Go to the next file.

		FindClose( hFind );	// Close the find file handle.
	}
}

void TraverseSQLiteDatabase( wchar_t *database_filepath )
{
	char *sql_err_msg = NULL;
	LVITEM lvi;

	int filepath_length = WideCharToMultiByte( CP_ACP, 0, database_filepath, -1, NULL, 0, NULL, NULL );
	char *ascii_filepath = ( char * )malloc( sizeof( char ) * filepath_length ); // Size includes the null character.
	WideCharToMultiByte( CP_ACP, 0, database_filepath, -1, ascii_filepath, filepath_length, NULL, NULL );

	int sql_rc = sqlite3_open_v2( ascii_filepath, &g_sql_db, SQLITE_OPEN_READONLY, NULL );
	if ( sql_rc )
	{
		// sqlite3_errmsg16( g_sql_db );
		SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )"The SQLite database could not be opened." );

		goto CLEANUP;
	}

	sql_rc = sqlite3_exec( g_sql_db, "SELECT Id, UniqueKey FROM SystemIndex_1_PropertyStore_Metadata", BuildPropertyTreeCallback, NULL, &sql_err_msg );
	if ( sql_rc != SQLITE_OK )
	{
		goto CLEANUP;
	}

	char query[ 512 ];
	memcpy_s( query, 512,
		"SELECT WorkId, Id, UniqueKey, Value, VariantType FROM SystemIndex_1_PropertyStore " \
		"JOIN SystemIndex_1_PropertyStore_Metadata ON SystemIndex_1_PropertyStore_Metadata.Id = SystemIndex_1_PropertyStore.ColumnId " \
		"WHERE WorkId IN ( SELECT WorkId FROM SystemIndex_1_PropertyStore WHERE Value = (X\'", 288 );

	// Get the number of items in the listview.
	int item_count = ( int )SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

	memset( &lvi, 0, sizeof( LVITEM ) );
	lvi.mask = LVIF_PARAM;

	FILE_INFO *fi = NULL;

	for ( lvi.iItem = 0; lvi.iItem < item_count; ++lvi.iItem )
	{
		// We don't want to continue scanning if the user cancels the scan.
		if ( g_kill_scan )
		{
			break;
		}

		SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

		fi = ( FILE_INFO * )lvi.lParam;
		if ( fi != NULL )
		{
			unsigned long long hash = ntohll( fi->entry_hash );
			// Hex value must be padded.
			// Id 438 =  14F-System_FileAttributes
			// Id 434 = 4392-System_FileExtension
			// Id  33 = 4447-System_ItemPathDisplay
			sprintf_s( query + 288, 512 - 288, "%016llx\') ) AND (Id = 438 OR Id = 434 OR Id = 33)", hash );

			PROPERTY_INFO_STATUS pis;
			pis.file_attributes = 0;
			pis.file_extension = NULL;
			pis.item_path_display = NULL;

			// Get values to filter results.
			sql_rc = sqlite3_exec( g_sql_db, query, CheckPropertyInfoCallback, ( void * )&pis, &sql_err_msg );
			if ( sql_rc != SQLITE_OK )
			{
				free( pis.file_extension );
				free( pis.item_path_display );

				break;
			}

			UpdateWindowInfo( fi->entry_hash, pis.item_path_display );

			// See if the entry is a folder.
			if ( ( pis.file_attributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 && !g_include_folders )
			{
				free( pis.file_extension );
				free( pis.item_path_display );

				continue;
			}
			else
			{
				// See if the file's extension is in our filter. Go to the next entry if it's not.
				if ( pis.file_extension != NULL && g_extension_filter[ 0 ] != 0 )
				{
					// Do a case-insensitive substring search for the extension.
					int ext_length = ( int )wcslen( pis.file_extension );
					wchar_t *temp_ext = ( wchar_t * )malloc( sizeof( wchar_t ) * ( ext_length + 3 ) );
					for ( int i = 0; i < ext_length; ++i )
					{
						temp_ext[ i + 1 ] = towlower( pis.file_extension[ i ] );
					}
					temp_ext[ 0 ] = L'|';				// Append the delimiter to the beginning of the string.
					temp_ext[ ext_length + 1 ] = L'|';	// Append the delimiter to the end of the string.
					temp_ext[ ext_length + 2 ] = L'\0';

					if ( wcsstr( g_extension_filter, temp_ext ) == NULL )
					{
						free( temp_ext );
						free( pis.file_extension );
						free( pis.item_path_display );

						continue;
					}

					free( temp_ext );
				}
			}

			free( pis.file_extension );

			if ( pis.item_path_display != NULL )
			{
				++g_match_count;

				// Replace the hash filename with the local filename.
				wchar_t *del_filename = fi->filename;
				fi->filename = pis.item_path_display;
				free( del_filename );

				pis.item_path_display = NULL;

				if ( g_retrieve_extended_information && fi->ei == NULL )
				{
					query[ 308 ] = 0; // Preform the same query, but without the last AND statement.

					// Get all the values associated with the hash. fi->ei is filled out in the callback.
					sql_rc = sqlite3_exec( g_sql_db, query, CreatePropertyInfoCallback, ( void * )fi, &sql_err_msg );
					if ( sql_rc != SQLITE_OK )
					{
						break;
					}
				}
			}

			free( pis.item_path_display );
		}
	}

CLEANUP:

	free( ascii_filepath );

	if ( sql_err_msg != NULL )
	{
		sprintf_s( g_error, ERROR_BUFFER_SIZE, "SQLite: %s", sql_err_msg );

		SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )g_error );

		sqlite3_free( sql_err_msg );
	}

	CleanupSQLiteInfo();
}

// The Microsoft Jet Database Engine seems to have a lot of annoying quirks/compatibility issues.
// The directory scanner is a nice compliment should this function not work 100%.
// Ideally, the database being scanned should be done with the same esent.dll that was used to create it.
// If there are issues with the database, make a copy and use esentutl.exe to fix it.
void TraverseESEDatabase( wchar_t *database_filepath, unsigned long revision, unsigned long page_size )
{
	JET_RETRIEVECOLUMN rc[ 4 ] = { 0 };

	unsigned long long thumbnail_cache_id = 0;
	unsigned char *item_path_display = NULL;
	unsigned char *file_extension = NULL;
	unsigned long file_attributes = 0;

	// Initialize the Jet database session and get column information for later retrieval. SystemIndex_0A and SystemIndex_0P will be opened on success.
	if ( ( g_err = InitESEDBInfo( database_filepath, revision, page_size ) ) != JET_errSuccess || ( g_err = ( g_revision < 0x14 ? GetColumnInfo() : GetColumnInfoWin8() ) ) != JET_errSuccess ) { goto CLEANUP; }

	// These are set in get_column_info. Make sure they exist.
	if ( g_thumbnail_cache_id == NULL )		{ if ( g_err == JET_errColumnNotFound ) { SetErrorMessage( "The System_ThumbnailCacheId column was not found." ); } goto CLEANUP; }
	else if ( g_item_path_display == NULL )	{ if ( g_err == JET_errColumnNotFound ) { SetErrorMessage( "The System_ItemPathDisplay column was not found." ); } goto CLEANUP; }
	else if ( g_file_attributes == NULL )	{ if ( g_err == JET_errColumnNotFound ) { SetErrorMessage( "The System_FileAttributes column was not found." ); } goto CLEANUP; }
	else if ( g_file_extension == NULL )	{ if ( g_err == JET_errColumnNotFound ) { SetErrorMessage( "The System_FileExtension column was not found." ); } goto CLEANUP; }

	// Ensure that the values we retrieve are of the correct size.
	if ( g_thumbnail_cache_id->max_size != sizeof( unsigned long long ) ||
		 g_file_attributes->max_size != sizeof( unsigned long ) ||
		 g_item_path_display->max_size == 0 ||
		 g_file_extension->max_size == 0 )
	{ g_err = JET_errInvalidColumnType; goto CLEANUP; }

	item_path_display = ( unsigned char * )malloc( sizeof( unsigned char ) * ( g_item_path_display->max_size + 2 ) ); // Add 2 bytes for a L"\0" to be added.
	memset( item_path_display, 0, sizeof( unsigned char ) * ( g_item_path_display->max_size + 2 ) );

	file_extension = ( unsigned char * )malloc( sizeof( unsigned char ) * ( g_file_extension->max_size + 2 ) ); // Add 2 bytes for a L"\0" to be added.
	memset( file_extension, 0, sizeof( unsigned char ) * ( g_file_extension->max_size + 2 ) );

	// Set up the column info we want to retrieve.
	rc[ 0 ].columnid = g_thumbnail_cache_id->column_id;
	rc[ 0 ].pvData = ( void * )&thumbnail_cache_id;
	rc[ 0 ].cbData = g_thumbnail_cache_id->max_size;
	rc[ 0 ].itagSequence = 1;

	rc[ 1 ].columnid = g_item_path_display->column_id;
	rc[ 1 ].pvData = ( void * )item_path_display;
	rc[ 1 ].cbData = g_item_path_display->max_size;
	rc[ 1 ].itagSequence = 1;

	rc[ 2 ].columnid = g_file_extension->column_id;
	rc[ 2 ].pvData = ( void * )file_extension;
	rc[ 2 ].cbData = g_file_extension->max_size;
	rc[ 2 ].itagSequence = 1;

	rc[ 3 ].columnid = g_file_attributes->column_id;
	rc[ 3 ].pvData = ( void * )&file_attributes;
	rc[ 3 ].cbData = g_file_attributes->max_size;
	rc[ 3 ].itagSequence = 1;

	if ( ( g_err = JetMove( g_sesid, g_tableid_0A, JET_MoveFirst, JET_bitNil ) ) != JET_errSuccess ) { goto CLEANUP; }

	// Windows 8+ (database revision >= 0x14) doesn't use compression.
	// mssrch.dll is for Windows 7. msscb.dll is for Windows Vista. Load whichever one we can.
	// These dlls will allow us to uncompress certain column's data/text.
	if ( g_revision < 0x14 && mssrch_state == MSSRCH_STATE_SHUTDOWN && msscb_state == MSSRCH_STATE_SHUTDOWN )
	{
		// We only need one to load successfully.
		if ( !InitializeMsSrch() && !InitializeMsSCB() )
		{
			SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )"The modules mssrch.dll and msscb.dll failed to load.\r\n\r\nCompressed data will not be read." );
		}
	}

	if ( g_retrieve_extended_information )
	{
		// Initialize g_rc_array to hold all of the record information we'll retrieve.
		BuildRetrieveColumnArray();
	}

	for ( ;; )
	{
		// We don't want to continue scanning if the user cancels the scan.
		if ( g_kill_scan )
		{
			break;
		}

		// Retrieve the 4 column values.
		if ( ( g_err = JetRetrieveColumns( g_sesid, g_tableid_0A, rc, 4 ) ) != JET_errSuccess )
		{
			break;
		}

		bool set_file_info = true;

		// See if the entry is a folder.
		if ( ( file_attributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
		{
			if ( !g_include_folders )
			{
				set_file_info = false;
				goto NEXT_ITEM;
			}
		}
		else
		{
			// The file extension should be an unterminated Unicode string.
			file_extension[ rc[ 2 ].cbActual ] = 0;
			file_extension[ rc[ 2 ].cbActual + 1 ] = 0;

			wchar_t *uc_file_extension = NULL;
			if ( g_file_extension->JetCompress )
			{
				uc_file_extension = UncompressValue( file_extension, rc[ 2 ].cbActual );
			}

			// See if the file's extension is in our filter. Go to the next entry if it's not.
			wchar_t *ext = ( uc_file_extension == NULL ? ( wchar_t * )file_extension : uc_file_extension );
			if ( g_extension_filter[ 0 ] != 0 )
			{
				// Do a case-insensitive substring search for the extension.
				int ext_length = ( int )wcslen( ext );
				wchar_t *temp_ext = ( wchar_t * )malloc( sizeof( wchar_t ) * ( ext_length + 3 ) );
				for ( int i = 0; i < ext_length; ++i )
				{
					temp_ext[ i + 1 ] = towlower( ext[ i ] );
				}
				temp_ext[ 0 ] = L'|';				// Append the delimiter to the beginning of the string.
				temp_ext[ ext_length + 1 ] = L'|';	// Append the delimiter to the end of the string.
				temp_ext[ ext_length + 2 ] = L'\0';

				if ( wcsstr( g_extension_filter, temp_ext ) == NULL )
				{
					free( temp_ext );
					free( uc_file_extension );

					set_file_info = false;
					goto NEXT_ITEM;
				}

				free( temp_ext );
			}

			free( uc_file_extension );
		}

NEXT_ITEM:

		// Swap the byte order of the hash. For XP and 7
		if ( g_use_big_endian )
		{
			thumbnail_cache_id = ntohll( thumbnail_cache_id );
		}

		// The file path should be an unterminated Unicode string. Add L"\0" to the end.
		item_path_display[ rc[ 1 ].cbActual ] = 0;
		item_path_display[ rc[ 1 ].cbActual + 1 ] = 0;

		wchar_t *uc_item_path_display = NULL;
		if ( g_item_path_display->JetCompress )
		{
			uc_item_path_display = UncompressValue( item_path_display, rc[ 1 ].cbActual );
		}

		wchar_t *filepath = ( uc_item_path_display == NULL ? ( wchar_t * )item_path_display : uc_item_path_display );

		if ( set_file_info )
		{
			UpdateFileinfo( thumbnail_cache_id, filepath );
		}
		UpdateWindowInfo( thumbnail_cache_id, filepath );

		free( uc_item_path_display );

		// Move to the next record (column row).
		if ( JetMove( g_sesid, g_tableid_0A, JET_MoveNext, JET_bitNil ) != JET_errSuccess )
		{
			break;
		}
	}

CLEANUP:

	free( item_path_display );
	free( file_extension );

	// Process any error that occurred.
	HandleESEDBError();

	// Cleanup and reset all values associated with processing the database.
	CleanupESEDBInfo();
}

// Tests the file's header to figure out whether it's an ESE or SQLite database.
void TraverseDatabase( wchar_t *database_filepath )
{
	HANDLE hFile = CreateFile( database_filepath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		DWORD read = 0;
		char partial_header[ 256 ];

		BOOL ret = ReadFile( hFile, partial_header, sizeof( char ) * 256, &read, NULL );

		CloseHandle( hFile );

		if ( ret != FALSE && read >= 256 )
		{
			if ( memcmp( partial_header, "SQLite format 3\0", 16 ) == 0 )
			{
				if ( InitializeSQLite3() )
				{
					TraverseSQLiteDatabase( database_filepath );
				}
				else
				{
					SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )"The module sqlite3.dll failed to load.\r\n\r\nThe DLL can be downloaded from: www.sqlite.org" );
				}
			}
			else if ( memcmp( partial_header + 4, "\xEF\xCD\xAB\x89", sizeof( unsigned long ) ) == 0 )	// Make sure we got enough of the header and it has the magic identifier (0x89ABCDEF) for an ESE database.
			{
				unsigned long revision = 0, page_size = 0;

				memcpy_s( &revision, sizeof( unsigned long ), partial_header + 0xE8, sizeof( unsigned long ) );		// Revision number
				memcpy_s( &page_size, sizeof( unsigned long ), partial_header + 0xEC, sizeof( unsigned long ) );	// Page size

				TraverseESEDatabase( database_filepath, revision, page_size );
			}
			else
			{
				SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )"The selected file is not an ESE or SQLite database." );
			}
		}
		else
		{
			SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )"The selected file could not be read or is an invalid database." );
		}
	}
	else
	{
		SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )"The selected file could not be opened." );
	}
}

unsigned __stdcall MapEntries( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	SetWindowTextA( g_hWnd_scan, "Map File Paths to Cache Entry Hashes - Please wait..." );	// Update the window title.
	SendMessage( g_hWnd_scan, WM_CHANGE_CURSOR, TRUE, 0 );	// SetCursor only works from the main thread. Set it to an arrow with hourglass.

	// 0 = scan directories, 1 = scan ese database
	unsigned char scan_type = ( unsigned char )pArguments;

	// Disable scan button, enable cancel button.
	SendMessage( g_hWnd_scan, WM_PROPAGATE, 1, 0 );

	CreateFileinfoTree();

	g_file_count = 0;	// Reset the file count.
	g_match_count = 0;	// Reset the match count.

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

			// Assume anything below Windows 7 is running on Windows Vista.
			is_win_7_or_higher = ( IsWindows7OrGreater() != FALSE ? true : false );
			is_win_8_1_or_higher = ( IsWindows8Point1OrGreater() != FALSE ? true : false );

			TraverseDirectory( g_filepath );
		}
		else
		{
			SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )"Volume name could not be found." );
		}
	}
	else
	{
		TraverseDatabase( g_filepath );
	}

	CleanupFileinfoTree();

	InvalidateRect( g_hWnd_list, NULL, TRUE );

	// Update the details.
	if ( !g_show_details )
	{
		char msg[ 11 ] = { 0 };
		sprintf_s( msg, 11, "%lu", g_file_count );
		SendMessageA( g_hWnd_scan, WM_PROPAGATE, 5, ( LPARAM )msg );
	}

	// Reset button and text.
	SendMessage( g_hWnd_scan, WM_PROPAGATE, 2, 0 );

	if ( g_match_count > 0 )
	{
		char msg[ 30 ] = { 0 };
		sprintf_s( msg, 30, "%d file%s mapped.", g_match_count, ( g_match_count > 1 ? "s were" : " was" ) );
		SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 1, ( LPARAM )msg );
	}
	else
	{
		SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 1, ( LPARAM )"No files were mapped." );
	}

	SendMessage( g_hWnd_scan, WM_CHANGE_CURSOR, FALSE, 0 );	// Reset the cursor.
	SetWindowTextA( g_hWnd_scan, "Map File Paths to Cache Entry Hashes" );	// Reset the window title.

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}
