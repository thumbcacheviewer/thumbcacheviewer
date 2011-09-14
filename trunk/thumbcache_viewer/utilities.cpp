/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011 Eric Kutcher

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

#define FILE_TYPE_BMP	"BM"
#define FILE_TYPE_JPEG	"\xFF\xD8\xFF\xE0"
#define FILE_TYPE_PNG	"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"

#define SNAP_WIDTH		10;		// The minimum distance at which our windows will attach together.

HANDLE prompt_mutex = NULL;

bool cancelled_prompt = false;	// User cancelled the prompt.
unsigned int entry_begin = 0;	// Beginning position to start reading.
unsigned int entry_end = 0;		// Ending position to stop reading.

bool is_close( int a, int b )
{
	// See if the distance between two points is less than the snap width.
	return abs( a - b ) < SNAP_WIDTH;
}

unsigned __stdcall read_database( void *pArguments )
{
	wchar_t *filepath = ( wchar_t * )pArguments;

	// Attempt to open our database file.
	HANDLE hFile = CreateFile( filepath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		DWORD read = 0;

		database_header dh = { 0 };
		ReadFile( hFile, &dh, sizeof( database_header ), &read, NULL );

		// Make sure it's a thumbcache database and the stucture was filled correctly.
		if ( memcmp( dh.magic_identifier, "CMMM", 4 ) != 0 || read != sizeof( database_header ) )
		{
			CloseHandle( hFile );
			free( filepath );

			MessageBox( g_hWnd_main, L"The file is not a thumbcache database.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

			_endthreadex( 0 );
			return 0;
		}

		// Set the file pointer to the first available cache entry.
		// current_position will keep track our our file pointer position before setting the file pointer. (ReadFile sets it as well)
		unsigned int current_position = SetFilePointer( hFile, dh.first_cache_entry, NULL, FILE_BEGIN );
		if ( current_position == INVALID_SET_FILE_POINTER )
		{
			// The file pointer reached the EOF.
			CloseHandle( hFile );
			free( filepath );

			MessageBox( g_hWnd_main, L"The first cache entry location is invalid.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

			_endthreadex( 0 );
			return 0;
		}

		entry_begin = 1;						// By default, we'll start with the first entry.
		entry_end = dh.number_of_cache_entries;	// By default, we'll end with the last entry.

		// If we've hit our display limit, then prompt the user for a range of entries.
		if ( dh.number_of_cache_entries > MAX_ENTRIES )
		{
			// This mutex will be released when the user selects a range of entries.
			prompt_mutex = CreateSemaphore( NULL, 0, 1, NULL );

			cancelled_prompt = false;	// Assume they haven't cancelled the prompt.
			SendMessage( g_hWnd_prompt, WM_PROPAGATE, dh.number_of_cache_entries, 0 );

			// Wait for the user to select the range of entries.
			WaitForSingleObject( prompt_mutex, INFINITE );
			CloseHandle( prompt_mutex );
			prompt_mutex = NULL;

			// If the user cancelled the prompt, then exit the thread. We'll assume they don't want to load the database.
			if ( cancelled_prompt == true )
			{
				CloseHandle( hFile );
				free( filepath );

				_endthreadex( 0 );
				return 0;
			}
		}

		// If the user selected an ending value that's larger than the number of entries, then set it to the number of entries instead.
		if ( entry_end > dh.number_of_cache_entries )
		{
			entry_end = dh.number_of_cache_entries;
		}

		// Determine whether we're going to offset the entries we list.
		bool offset_beginning = ( entry_begin > 1 ? true : false );

		// Go through our database and attempt to extract each cache entry.
		for ( unsigned int i = 1; i <= entry_end; ++i )
		{
			// Set the file pointer to the end of the last cache entry.
			current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );
			if ( current_position == INVALID_SET_FILE_POINTER )
			{
				CloseHandle( hFile );
				free( filepath );

				wchar_t msg[ 21 ] = { 0 };
				swprintf_s( msg, 21, L"Invalid cache entry.", current_position );
				MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

				_endthreadex( 0 );
				return 0;
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
					CloseHandle( hFile );
					free( filepath );

					_endthreadex( 0 );
					return 0;
				}
				else if ( memcmp( ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );
					CloseHandle( hFile );
					free( filepath );

					wchar_t msg[ 49 ] = { 0 };
					swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
					MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

					_endthreadex( 0 );
					return 0;
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
					CloseHandle( hFile );
					free( filepath );

					_endthreadex( 0 );
					return 0;
				}
				else if ( memcmp( ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );
					CloseHandle( hFile );
					free( filepath );

					wchar_t msg[ 49 ] = { 0 };
					swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
					MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

					_endthreadex( 0 );
					return 0;
				}
			}
			else	// If this is true, then the file isn't from Vista or 7 and not supported by this program.
			{
				CloseHandle( hFile );
				free( filepath );

				MessageBox( g_hWnd_main, L"The file is not supported by this program.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

				_endthreadex( 0 );
				return 0;
			}

			// Size of the cache entry.
			unsigned int cache_entry_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->cache_entry_size : ( ( database_cache_entry_vista * )database_cache_entry )->cache_entry_size );

			current_position += cache_entry_size;

			// If we've selected a range of values, then we're going to offset the file pointer.
			if ( offset_beginning == true )
			{
				// If we've reached the beginning of the range, then we're no longer going to offset the file pointer.
				if ( ( i + 1 ) == entry_begin )
				{
					offset_beginning = false;
				}

				// Free each database entry that we've skipped over.
				free( database_cache_entry );

				continue;
			}
			
			// Filename length should be the total number of bytes (excluding the NULL character) that the UTF-16 filename takes up. A realistic limit should be twice the size of MAX_PATH.
			unsigned int filename_length = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->filename_length : ( ( database_cache_entry_vista * )database_cache_entry )->filename_length );

			// Skip blank filenames.
			if ( filename_length == 0 )
			{
				// Free each database entry that we've skipped over.
				free( database_cache_entry );

				continue;
			}

			// It's unlikely that a filename will be longer than MAX_PATH, but to be on the safe side, we should truncate it if it is.
			unsigned short filename_truncate_length = min( filename_length, ( sizeof( wchar_t ) * MAX_PATH ) );
			
			// UTF-16 filename. Allocate the filename length plus 5 for the unicode extension and null character. This will get deleted before MainWndProc is destroyed. See WM_DESTROY in MainWndProc.
			wchar_t *filename = ( wchar_t * )malloc( filename_truncate_length + ( sizeof( wchar_t ) * 5 ) );
			memset( filename, 0, filename_truncate_length + ( sizeof( wchar_t ) * 5 ) );
			ReadFile( hFile, filename, filename_truncate_length, &read, NULL );
			if ( read == 0 )
			{
				free( filename );
				free( database_cache_entry );
				CloseHandle( hFile );
				free( filepath );
				
				wchar_t msg[ 49 ] = { 0 };
				swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
				MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

				_endthreadex( 0 );
				return 0;
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
					CloseHandle( hFile );
					free( filepath );

					wchar_t msg[ 49 ] = { 0 };
					swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
					MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
					
					_endthreadex( 0 );
					return 0;
				}
			}

			// Padding before the data entry.
			unsigned int padding_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->padding_size : ( ( database_cache_entry_vista * )database_cache_entry )->padding_size );

			// This will set our file pointer to the beginning of the data entry.
			file_position = SetFilePointer( hFile, padding_size, 0, FILE_CURRENT );
			if ( file_position == INVALID_SET_FILE_POINTER )
			{
				free( filename );
				free( database_cache_entry );
				CloseHandle( hFile );
				free( filepath );

				wchar_t msg[ 49 ] = { 0 };
				swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
				MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

				_endthreadex( 0 );
				return 0;
			}

			// Size of our image.
			unsigned int data_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_size : ( ( database_cache_entry_vista * )database_cache_entry )->data_size );

			// Create a new info structure to send to the listview item's lParam value.
			fileinfo *fi = ( fileinfo * )malloc( sizeof( fileinfo ) );
			fi->offset = file_position;
			fi->size = data_size;
			fi->system = dh.version;

			long long entry_hash = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->entry_hash : ( ( database_cache_entry_vista * )database_cache_entry )->entry_hash );
			long long data_checksum = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_checksum : ( ( database_cache_entry_vista * )database_cache_entry )->data_checksum );
			long long header_checksum = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->header_checksum : ( ( database_cache_entry_vista * )database_cache_entry )->header_checksum );

			// Reverse the little endian values for data_checksum and header_checksum.

			// Swaps the 32bit ints of the 64bit int.
			_asm mov eax, dword ptr entry_hash;
			_asm mov ecx, dword ptr entry_hash + 4;
			_asm mov dword ptr entry_hash, ecx;
			_asm mov dword ptr entry_hash + 4, eax;

			// Swaps the 32bit ints of the 64bit int.
			_asm mov eax, dword ptr data_checksum;
			_asm mov ecx, dword ptr data_checksum + 4;
			_asm mov dword ptr data_checksum, ecx;
			_asm mov dword ptr data_checksum + 4, eax;

			// Swaps the 32bit ints of the 64bit int.
			_asm mov eax, dword ptr header_checksum;
			_asm mov ecx, dword ptr header_checksum + 4;
			_asm mov dword ptr header_checksum, ecx;
			_asm mov dword ptr header_checksum + 4, eax;

			fi->data_checksum = data_checksum;
			fi->header_checksum = header_checksum;
			fi->entry_hash = entry_hash;

			// Read any data that exists and get its file extension.
			if ( data_size != 0 )
			{
				// Retrieve the data content. (Offsets the file pointer as well).
				char *buf = ( char * )malloc( sizeof( char ) * data_size );
				ReadFile( hFile, buf, data_size, &read, NULL );
				if ( read == 0 )
				{
					free( buf );
					free( fi );
					free( filename );
					free( database_cache_entry );
					CloseHandle( hFile );
					free( filepath );

					wchar_t msg[ 49 ] = { 0 };
					swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
					MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

					_endthreadex( 0 );
					return 0;
				}

				// Detect the file extension and copy it into the filename string.
				if ( memcmp( buf, FILE_TYPE_BMP, 2 ) == 0 )			// First 3 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".bmp", 5 );
					fi->extension = 0;
				}
				else if ( memcmp( buf, FILE_TYPE_JPEG, 4 ) == 0 )	// First 4 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".jpg", 5 );
					fi->extension = 1;
				}
				else if ( memcmp( buf, FILE_TYPE_PNG, 8 ) == 0 )	// First 8 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".png", 5 );
					fi->extension = 2;
				}
				else if ( dh.version == WINDOWS_VISTA && wcslen( ( ( database_cache_entry_vista * )database_cache_entry )->extension ) > 0 )	// If it's a Windows Vista thumbcache file and we can't detect the extension, then use the one given.
				{
					swprintf_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".%s", ( ( database_cache_entry_vista * )database_cache_entry )->extension ); 
					fi->extension = 3;	// Unknown extension
				}

				// Free our data buffer.
				free( buf );
			}
			else	// No data exists.
			{
				// Windows Vista thumbcache files should include the extension.
				if ( dh.version == WINDOWS_VISTA && wcslen( ( ( database_cache_entry_vista * )database_cache_entry )->extension ) > 0 )
				{
					swprintf_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".%s", ( ( database_cache_entry_vista * )database_cache_entry )->extension ); 
				}

				fi->extension = 3;	// Unknown extension
			}

			wcscpy_s( fi->dbpath, MAX_PATH, filepath );
			fi->filename = filename;	// Gets deleted during shutdown.

			// Insert a row into our listview.
			LVITEM lvi = { NULL };
			lvi.mask = LVIF_PARAM; // Our listview items will display the text contained the lParam value.
			lvi.iItem = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );
			lvi.iSubItem = 0;
			lvi.lParam = ( LPARAM )fi;
			SendMessage( g_hWnd_list, LVM_INSERTITEM, 0, ( LPARAM )&lvi );

			// Enable the Save All and Select All menu items.
			EnableMenuItem( g_hMenu, MENU_SAVE_ALL, MF_ENABLED );
			EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_ENABLED );
			EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_ENABLED );
	
			// Free our database cache entry.
			free( database_cache_entry );
		}
		// Close the input file.
		CloseHandle( hFile );
	}
	else
	{
		// If this occurs, then there's something wrong with the user's system.
		MessageBox( g_hWnd_main, L"The database file failed to open.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
	}

	free( filepath );

	_endthreadex( 0 );
	return 0;
}
