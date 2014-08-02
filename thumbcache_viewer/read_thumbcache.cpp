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

#include "read_thumbcache.h"
#include "globals.h"
#include "utilities.h"

#include <stdio.h>

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

unsigned __stdcall read_thumbcache( void *pArguments )
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
			if ( g_kill_thread == true )
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

			// Attempt to open our database file. The following block will parse the entire database.
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
					if ( g_kill_thread == true )
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
					
					// UTF-16 filename. Allocate the filename length, any extra extension, 4 for the unicode extension (.xxx), and 1 for the null character. This will get deleted before MainWndProc is destroyed. See WM_DESTROY in MainWndProc.
					unsigned int filename_end = filename_truncate_length / sizeof( wchar_t );
					unsigned char extra_extension = 0;
					if ( dh.version == WINDOWS_VISTA )
					{
						// The Vista file extension can be up to 4 characters long and unterminated.
						extra_extension = wcsnlen( ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 );

						// Include '.'
						if ( extra_extension > 0 )
						{
							++extra_extension;
						}
					}
					wchar_t *filename = ( wchar_t * )malloc( filename_truncate_length + ( sizeof( wchar_t ) * ( extra_extension + 4 + 1 ) ) );
					memset( filename, 0, filename_truncate_length + ( sizeof( wchar_t ) * ( extra_extension + 4 + 1 ) ) );
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

					// Add the Windows Vista file extension.
					if ( extra_extension > 0 )
					{
						wmemcpy_s( filename + filename_end, 1, L".", 1 );
						wmemcpy_s( filename + filename_end + 1, 4, ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 );
						filename_end += extra_extension;
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
					fi->ei = NULL;
					fi->header_offset = header_offset;
					fi->data_offset = file_position;
					fi->size = data_size;

					fi->entry_hash = fi->mapped_hash = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->entry_hash : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->entry_hash : ( ( database_cache_entry_8 * )database_cache_entry )->entry_hash ) );
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

						// Detect the file type and copy its file extension into the filename string.
						if ( memcmp( buf, FILE_TYPE_BMP, 2 ) == 0 )			// First 3 bytes
						{
							fi->flag = FIF_TYPE_BMP;

							if ( dh.version != WINDOWS_VISTA ||
							   ( dh.version == WINDOWS_VISTA && _wcsnicmp( ( ( database_cache_entry_vista * )database_cache_entry )->extension, L"bmp", 4 ) != 0 ) )
							{
								wmemcpy_s( filename + filename_end, 4, L".bmp", 4 );
							}
						}
						else if ( memcmp( buf, FILE_TYPE_JPEG, 4 ) == 0 )	// First 4 bytes
						{
							fi->flag = FIF_TYPE_JPG;

							if ( dh.version != WINDOWS_VISTA ||
							   ( dh.version == WINDOWS_VISTA && _wcsnicmp( ( ( database_cache_entry_vista * )database_cache_entry )->extension, L"jpg", 4 ) != 0 ) )
							{
								wmemcpy_s( filename + filename_end, 4, L".jpg", 4 );
							}
						}
						else if ( memcmp( buf, FILE_TYPE_PNG, 8 ) == 0 )	// First 8 bytes
						{
							fi->flag = FIF_TYPE_PNG;

							if ( dh.version != WINDOWS_VISTA ||
							   ( dh.version == WINDOWS_VISTA && _wcsnicmp( ( ( database_cache_entry_vista * )database_cache_entry )->extension, L"png", 4 ) != 0 ) )
							{
								wmemcpy_s( filename + filename_end, 4, L".png", 4 );
							}
						}

						// Free our data buffer.
						free( buf );

						// This will set our file pointer to the end of the data entry.
						SetFilePointer( hFile, data_size - 8, 0, FILE_CURRENT );
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
					++( si->count );

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
