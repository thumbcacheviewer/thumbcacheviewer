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
#include "globals.h"

int snap_width = 10;	// The minimum distance at which our windows will attach together.

bool is_close( int a, int b )
{
	// See if the distance between two points is less than the snap width.
	return abs( a - b ) < snap_width;
}

void read_database( wchar_t &filepath )
{
	// Attempt to open our database file.
	HANDLE hFile = CreateFile( &filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		DWORD read = 0;

		database_header dh = { 0 };
		ReadFile( hFile, &dh, sizeof( database_header ), &read, NULL );

		// Make sure it's a thumbcache database and the stucture was filled correctly.
		if ( memcmp( dh.magic_identifier, "CMMM", 4 ) != 0 || read != sizeof( database_header ) )
		{
			CloseHandle( hFile );
			MessageBox( g_hWnd_main, L"The file is not a thumbcache database.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
			return;
		}

		// Set the file pointer to the first available cache entry.
		if ( SetFilePointer( hFile, dh.first_cache_entry, NULL, FILE_BEGIN ) == INVALID_SET_FILE_POINTER )
		{
			// The file pointer reached the EOF.
			CloseHandle( hFile );
			return;
		}

		// Go through our database and attempt to extract each cache entry.
		for ( unsigned int i = 0; i < dh.number_of_cache_entries; i++ )
		{
			void *database_cache_entry = NULL;
			// Determine the type of database we're working with and store its content in the correct structure.
			if ( dh.version == WINDOWS_7 )
			{
				database_cache_entry = new database_cache_entry_7;
				ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_7 ), &read, NULL );
				// Make sure it's a thumbcache database and the stucture was filled correctly.
				if ( memcmp( ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 || read != sizeof( database_cache_entry_7 ) )
				{
					delete database_cache_entry;
					CloseHandle( hFile );
					return;
				}
			}
			else if ( dh.version == WINDOWS_VISTA )
			{
				database_cache_entry = new database_cache_entry_vista;
				ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_vista ), &read, NULL );
				// Make sure it's a thumbcache database and the stucture was filled correctly.
				if ( memcmp( ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 || read != sizeof( database_cache_entry_vista ) )
				{
					delete database_cache_entry;
					CloseHandle( hFile );
					return;
				}
			}
			else	// If this is true, then the file isn't from Vista or 7 and not supported by this program.
			{
				CloseHandle( hFile );
				MessageBox( g_hWnd_main, L"The file is not supported by this program.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
				return;
			}

			unsigned int filename_length = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->filename_length : ( ( database_cache_entry_vista * )database_cache_entry )->filename_length );

			// UTF-16 filename. Allocate the filename length plus 5 for the unicode extension and null character. This will get deleted before MainWndProc is destroyed. See WM_DESTROY in MainWndProc.
			char *filename = new char[ sizeof( char ) * filename_length + ( sizeof( wchar_t ) * 5 ) ];
			memset( filename, 0, sizeof( char ) * filename_length + ( sizeof( wchar_t ) * 5 ) );
			ReadFile( hFile, filename, sizeof( char ) * filename_length, &read, NULL );
			if ( read == 0 )
			{
				delete database_cache_entry;
				CloseHandle( hFile );
				return;
			}

			// Padding before the data content.
			unsigned int padding_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->padding_size : ( ( database_cache_entry_vista * )database_cache_entry )->padding_size );

			// This will set our file pointer to the beginning of the data entry.
			unsigned int file_position = SetFilePointer( hFile, padding_size, 0, FILE_CURRENT );

			// Size of our image.
			unsigned int data_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_size : ( ( database_cache_entry_vista * )database_cache_entry )->data_size );

			// No need to process anything if the data size is 0.
			if ( data_size != 0 )
			{
				// Create a new info structure to send to the listview item's lparam value.
				fileinfo *fi = new fileinfo;
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

				wcscpy_s( fi->dbpath, MAX_PATH + 1, &filepath );

				int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );
				wchar_t s_num[ 10 ] = { 0 };
				swprintf( s_num, 9, L"%d", item_count + 1 );

				// Retrieve the data content.
				char *buf = new char[ sizeof( char ) * data_size ];
				memset( buf, 0, sizeof( char ) * data_size );
				ReadFile( hFile, buf, data_size, &read, NULL );
				if ( read == 0 )
				{
					delete database_cache_entry;
					CloseHandle( hFile );
					return;
				}

				// Copy our file extension into the filename string.
				if ( memcmp( buf, FILE_TYPE_BMP, 2 ) == 0 )			// First 3 bytes
				{
					wcscat_s( ( wchar_t * )filename + wcslen( ( wchar_t * )filename ), 5, L".bmp" );
					fi->extension = 0;
				}
				else if ( memcmp( buf, FILE_TYPE_JPEG, 4 ) == 0 )	// First 4 bytes
				{
					wcscat_s( ( wchar_t * )filename + wcslen( ( wchar_t * )filename ), 5, L".jpg" );
					fi->extension = 1;
				}
				else if ( memcmp( buf, FILE_TYPE_PNG, 8 ) == 0 )	// First 8 bytes
				{
					wcscat_s( ( wchar_t * )filename + wcslen( ( wchar_t * )filename ), 5, L".png" );
					fi->extension = 2;
				}

				fi->filename = ( wchar_t * )filename;	// Gets deleted during shutdown.

				// Insert a row into our listview.
				LVITEM lvi = { NULL };
				lvi.mask = LVIF_PARAM; // Our listview items will display the text contained the lparam value.
				lvi.iItem = item_count;
				lvi.iSubItem = 0;
				lvi.lParam = ( LPARAM )fi;
				SendMessage( g_hWnd_list, LVM_INSERTITEM, 0, ( LPARAM )&lvi );

				// Enable the Save All and Select All menu items.
				EnableMenuItem( g_hMenu, MENU_SAVE_ALL, MF_ENABLED );
				EnableMenuItem( g_hMenu, MENU_SELECT_ALL, MF_ENABLED );
				EnableMenuItem( g_hMenuSub_context, MENU_SELECT_ALL, MF_ENABLED );
				
				// Delete our data buffer.
				delete[] buf;
			}
			// Delete our database cache entry.
			delete database_cache_entry;
		}
		// Close the input file.
		CloseHandle( hFile );
	}
	else
	{
		// If this occurs, then there's something wrong with the user's system. Or maybe the file is locked?
		MessageBox( g_hWnd_main, L"The database file failed to open.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
	}
	return;
}
