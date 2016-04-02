/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2016 Eric Kutcher

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
#include "read_esedb.h"

#include "lite_mssrch.h"
#include "lite_msscb.h"

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
unsigned int file_count = 0;						// Number of files scanned.
unsigned int match_count = 0;						// Number of files that match an entry hash.

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

void update_scan_info( unsigned long long hash, wchar_t *filepath )
{
	// Now that we have a hash value to compare, search our fileinfo tree for the same value.
	linked_list *ll = ( linked_list * )dllrbt_find( fileinfo_tree, ( void * )hash, true );

	// Retrieve the column information once so we don't have to call this for duplicate entries in the loop below.
	bool got_columns = false;
	extended_info *ei = NULL;
	if ( g_retrieve_extended_information == true )
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
			++match_count;

			// Replace the hash filename with the local filename.
			free( ll->fi->filename );
			ll->fi->filename = _wcsdup( filepath );

			if ( got_columns == true )
			{
				// Convert the record values to strings so we can print them out.
				convert_values( &( ll->fi->ei ) );
			}
			else if ( ll->fi->ei == NULL )
			{
				// If we have extended information, but didn't retrieve any columns, then copy it to our fileinfo linked list nodes.
				extended_info *l_ei = NULL;
				extended_info *t_ei = ei;
				while ( t_ei != NULL )
				{
					if ( t_ei->sei != NULL )
					{
						++( t_ei->sei->count );

						extended_info *c_ei = ( extended_info * )malloc( sizeof( extended_info ) );
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

	++file_count; 

	// Update our scan window with new scan information.
	if ( g_show_details == true )
	{
		SendMessage( g_hWnd_scan, WM_PROPAGATE, 3, ( LPARAM )filepath );
		char buf[ 17 ] = { 0 };
		sprintf_s( buf, 17, "%016llx", hash );
		SendMessageA( g_hWnd_scan, WM_PROPAGATE, 4, ( LPARAM )buf );
		sprintf_s( buf, 17, "%lu", file_count );
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

	HANDLE hFile = CreateFile( filepath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		// Hash Volume GUID
		hash = hash_data( ( char * )&clsid, hash, sizeof( CLSID ) );

		// Hash File ID - found in the Master File Table.
		BY_HANDLE_FILE_INFORMATION bhfi;
		GetFileInformationByHandle( hFile, &bhfi );
		CloseHandle( hFile );
		unsigned long long file_id = bhfi.nFileIndexHigh;
		file_id = ( file_id << 32 ) | bhfi.nFileIndexLow;

		hash = hash_data( ( char * )&file_id, hash, sizeof( unsigned long long ) );

		// Windows Vista doesn't hash the file extension or modified DOS time.
		if ( is_win_7_or_higher == true )
		{
			// Hash Wide Character File Extension
			hash = hash_data( ( char * )extension, hash, wcslen( extension ) * sizeof( wchar_t ) );

			// Hash Last Modified DOS Time
			unsigned short fat_date;
			unsigned short fat_time;
			FileTimeToDosDateTime( &bhfi.ftLastWriteTime, &fat_date, &fat_time );
			unsigned int dos_time = fat_date;
			dos_time = ( dos_time << 16 ) | fat_time;

			hash = hash_data( ( char * )&dos_time, hash, sizeof( unsigned int ) );

			// Windows 8.1 calculates the precision loss between the converted write time and original write time.
			if ( is_win_8_1_or_higher == true )
			{
				// Convert the DOS time back into a FILETIME.
				FILETIME converted_write_time;
				DosDateTimeToFileTime( fat_date, fat_time, &converted_write_time );

				// We only need to hash the low order int.
				unsigned int precision_loss = converted_write_time.dwLowDateTime - bhfi.ftLastWriteTime.dwLowDateTime;

				// Hash if there's any precision loss.
				if ( precision_loss != 0 )
				{
					hash = hash_data( ( char * )&precision_loss, hash, sizeof( unsigned int ) );
				}
			}
		}

		update_scan_info( hash, filepath );
	}
}

void traverse_directory( wchar_t *path )
{
	// We don't want to continue scanning if the user cancels the scan.
	if ( g_kill_scan == true )
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
			if ( g_kill_scan == true )
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
						traverse_directory( filepath );

						// Only hash folders if enabled.
						if ( g_include_folders == true )
						{
							hash_file( filepath, L"" );
						}
					}
				}
			}
			else
			{
				// See if the file's extension is in our filter. Go to the next file if it's not.
				wchar_t *ext = get_extension_from_filename( FindFileData.cFileName, wcslen( FindFileData.cFileName ) );
				if ( g_extension_filter[ 0 ] != 0 )
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

				hash_file( filepath, ext );
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
	JET_RETRIEVECOLUMN rc[ 4 ] = { 0 };

	unsigned long long thumbnail_cache_id = 0;
	unsigned char *item_path_display = NULL;
	unsigned char *file_extension = NULL;
	unsigned long file_attributes = 0;

	// Initialize the Jet database session and get column information for later retrieval. SystemIndex_0A and SystemIndex_0P will be opened on success.
	if ( ( g_err = init_esedb_info( g_filepath ) ) != JET_errSuccess || ( g_err = ( g_revision < 0x14 ? get_column_info() : get_column_info_win8() ) ) != JET_errSuccess ) { goto CLEANUP; }

	// These are set in get_column_info. Make sure they exist.
	if ( g_thumbnail_cache_id == NULL )
	{
		if ( g_err == JET_errColumnNotFound ) { set_error_message( "The System_ThumbnailCacheId column was not found." ); }
		goto CLEANUP;
	}
	else if ( g_item_path_display == NULL )
	{
		if ( g_err == JET_errColumnNotFound ) { set_error_message( "The System_ItemPathDisplay column was not found." ); }
		goto CLEANUP;
	}
	else if ( g_file_attributes == NULL )
	{
		if ( g_err == JET_errColumnNotFound ) { set_error_message( "The System_FileAttributes column was not found." ); }
		goto CLEANUP;
	}
	else if ( g_file_extension == NULL )
	{
		if ( g_err == JET_errColumnNotFound ) { set_error_message( "The System_FileExtension column was not found." ); }
		goto CLEANUP;
	}

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
	// mssrch.dll is for Windows 7. msscb.dll is for Winows Vista. Load whichever one we can.
	// These dlls will allow us to uncompress certain column's data/text.
	if ( g_revision < 0x14 && mssrch_state == MSSRCH_STATE_SHUTDOWN && msscb_state == MSSRCH_STATE_SHUTDOWN )
	{
		// We only need one to load successfully.
		if ( InitializeMsSrch() == false && InitializeMsSCB() == false )
		{
			SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )"The modules mssrch.dll and msscb.dll failed to load.\r\n\r\nCompressed data will not be read." );
		}
	}

	if ( g_retrieve_extended_information == true )
	{
		// Initialize g_rc_array to hold all of the record information we'll retrieve.
		build_retrieve_column_array();
	}

	while ( true )
	{
		// We don't want to continue scanning if the user cancels the scan.
		if ( g_kill_scan == true )
		{
			break;
		}

		// Retrieve the 4 column values.
		if ( ( g_err = JetRetrieveColumns( g_sesid, g_tableid_0A, rc, 4 ) ) != JET_errSuccess )
		{
			break;
		}

		// For XP and 7
		if ( g_use_big_endian == true )
		{
			file_attributes = ntohl( file_attributes );
		}

		// See if the entry is a folder.
		if ( ( file_attributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
		{
			if ( g_include_folders == false )
			{
				if ( JetMove( g_sesid, g_tableid_0A, JET_MoveNext, JET_bitNil ) != JET_errSuccess )
				{
					break;
				}

				continue;
			}
		}
		else
		{
			// The file extension should be an unterminated Unicode string.
			file_extension[ rc[ 2 ].cbActual ] = 0;
			file_extension[ rc[ 2 ].cbActual + 1 ] = 0;

			wchar_t *uc_file_extension = NULL;
			if ( g_file_extension->JetCompress == true )
			{
				uc_file_extension = uncompress_value( file_extension, rc[ 2 ].cbActual );
			}

			// See if the file's extension is in our filter. Go to the next entry if it's not.
			wchar_t *ext = ( uc_file_extension == NULL ? ( wchar_t * )file_extension : uc_file_extension );
			if ( g_extension_filter[ 0 ] != 0 )
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

				if ( wcsstr( g_extension_filter, temp_ext ) == NULL )
				{
					free( temp_ext );
					free( uc_file_extension );

					// Move to the next record (column row).
					if ( JetMove( g_sesid, g_tableid_0A, JET_MoveNext, JET_bitNil ) != JET_errSuccess )
					{
						break;
					}

					continue;
				}

				free( temp_ext );
			}

			free( uc_file_extension );
		}

		// Swap the byte order of the hash. For XP and 7
		if ( g_use_big_endian == true )
		{
			thumbnail_cache_id = ntohll( thumbnail_cache_id );
		}

		// The file path should be an unterminated Unicode string. Add L"\0" to the end.
		item_path_display[ rc[ 1 ].cbActual ] = 0;
		item_path_display[ rc[ 1 ].cbActual + 1 ] = 0;

		wchar_t *uc_item_path_display = NULL;
		if ( g_item_path_display->JetCompress == true )
		{
			uc_item_path_display = uncompress_value( item_path_display, rc[ 1 ].cbActual );
		}

		update_scan_info( thumbnail_cache_id, ( uc_item_path_display == NULL ? ( wchar_t * )item_path_display : uc_item_path_display ) );

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
	handle_esedb_error();

	// Cleanup and reset all values associated with processing the database.
	cleanup_esedb_info();
}

unsigned __stdcall map_entries( void *pArguments )
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

	file_count = 0;		// Reset the file count.
	match_count = 0;	// Reset the match count.

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

			traverse_directory( g_filepath );
		}
		else
		{
			SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )"Volume name could not be found." );
		}
	}
	else
	{
		traverse_ese_database();
	}

	cleanup_fileinfo_tree();

	InvalidateRect( g_hWnd_list, NULL, TRUE );

	// Update the details.
	if ( g_show_details == false )
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
