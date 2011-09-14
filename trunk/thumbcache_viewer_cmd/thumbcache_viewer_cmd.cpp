/*
    thumbcache_viewer_cmd will extract thumbnail images from thumbcache database files.
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

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

// Magic identifiers for various image formats.
#define FILE_TYPE_BMP	"BM"
#define FILE_TYPE_JPEG	"\xFF\xD8\xFF\xE0"
#define FILE_TYPE_PNG	"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"

// Database version.
#define WINDOWS_VISTA	0x14
#define WINDOWS_7		0x15

// Thumbcache header information.
struct database_header
{
	char magic_identifier[ 4 ];
	unsigned int version;
	unsigned int type;
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

int main( int argc, char *argv[] )
{
	// Ask user for input filename.
	char name[ MAX_PATH ] = { 0 };
	if ( argc == 1 )
	{
		printf( "Please enter the name of the database: " );
		gets_s( name, MAX_PATH );
	}
	else
	{
		// Copy the maximum amount of bytes from argv[ 1 ] that a path can be, and no more.
		memcpy_s( name, MAX_PATH, argv[ 1 ], MAX_PATH );
	}

	printf( "Attempting to open the database file\n" );

	// Attempt to open our database file.
	HANDLE hFile = CreateFileA( name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		DWORD read = 0;

		database_header dh = { 0 };
		ReadFile( hFile, &dh, sizeof( database_header ), &read, NULL );

		// Make sure it's a thumbcache database and the stucture was filled correctly.
		if ( memcmp( dh.magic_identifier, "CMMM", 4 ) != 0 || read != sizeof( database_header ) )
		{
			CloseHandle( hFile );
			printf( "The file is not a thumbcache database." );
			return 0;
		}

		printf( "---------------------------------------------\n" );
		printf( "Extracting file header (24 bytes).\n" );
		printf( "---------------------------------------------\n" );

		// Magic identifer.
		char stmp[ 5 ] = { 0 };
		memcpy( stmp, dh.magic_identifier, sizeof( char ) * 4 );
		printf( "Signature (magic identifier): %s\n", stmp );

		// Version of database.
		if ( dh.version == WINDOWS_VISTA )
		{
			printf( "Version: Windows Vista\n" );
		}
		else if ( dh.version == WINDOWS_7 )
		{
			printf( "Version: Windows 7\n" );
		}
		else
		{
			CloseHandle( hFile );
			printf( "Database is not supported.\n" );
			return 0;
		}

		// Type of thumbcache database.
		if ( dh.type == 0x00 )
		{
			printf( "Cache type: thumbcache_32.db, 32x32\n" );
		}
		else if ( dh.type == 0x01 )
		{
			printf( "Cache type: thumbcache_96.db, 96x96\n" );
		}
		else if ( dh.type == 0x02 )
		{
			printf( "Cache type: thumbcache_256.db, 256x256\n" );
		}
		else if ( dh.type == 0x03 )
		{
			printf( "Cache type: thumbcache_1024.db, 1024x0124\n" );
		}
		else if ( dh.type == 0x04 )
		{
			printf( "Cache type: thumbcache_sr.db\n" );
		}

		// Offset to the first cache entry.
		printf( "Offset to first cache entry: %lu bytes\n", dh.first_cache_entry );

		// Offset to the available cache entry.
		printf( "Offset to available cache entry: %lu bytes\n", dh.available_cache_entry );

		// Number of cache entries.
		printf( "Number of cache entries: %lu\n", dh.number_of_cache_entries );

		// Set the file pointer to the first available cache entry.
		unsigned int current_position = SetFilePointer( hFile, dh.first_cache_entry, NULL, FILE_BEGIN );
		if ( current_position == INVALID_SET_FILE_POINTER )
		{
			// The file pointer reached the EOF.
			CloseHandle( hFile );
			printf( "End of file reached. There are no more entires.\n" );
			return 0;
		}

		// Go through our database and attempt to extract each cache entry.
		for ( unsigned int i = 0; i < dh.number_of_cache_entries; i++ )
		{
			printf( "\n---------------------------------------------\n" );
			printf( "Extracting cache entry %lu at %lu bytes.\n", i + 1, current_position );
			printf( "---------------------------------------------\n" );

			// Set the file pointer to the end of the last cache entry.
			current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );
			if ( current_position == INVALID_SET_FILE_POINTER )
			{
				// EOF reached.
				CloseHandle( hFile );
				printf( "End of file reached. There are no more entires.\n" );
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
					printf( "End of file reached. There are no more entires.\n" );
					return 0;
				}
				else if ( memcmp( ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );
					CloseHandle( hFile );
					printf( "Invalid cache entry located at %lu bytes.", current_position );
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
					printf( "End of file reached. There are no more entires.\n" );
					return 0;
				}
				else if ( memcmp( ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );
					CloseHandle( hFile );
					printf( "Invalid cache entry located at %lu bytes.", current_position );
					return 0;
				}
			}

			// Cache size includes the 4 byte signature and itself ( 4 bytes ).
			unsigned int cache_entry_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->cache_entry_size : ( ( database_cache_entry_vista * )database_cache_entry )->cache_entry_size );		
			
			current_position += cache_entry_size;

			// The length of our filename.
			unsigned int filename_length = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->filename_length : ( ( database_cache_entry_vista * )database_cache_entry )->filename_length );

			// Skip blank filenames.
			if ( filename_length == 0 )
			{
				// Free each database entry that we've skipped over.
				free( database_cache_entry );

				continue;
			}

			// The magic identifier for the current entry.
			char *magic_identifier = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier : ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier );
			memcpy( stmp, magic_identifier, sizeof( char ) * 4 );
			printf( "Signature (magic identifier): %s\n", stmp );

			printf( "Cache size: %lu bytes\n", cache_entry_size );

			long long entry_hash = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->entry_hash : ( ( database_cache_entry_vista * )database_cache_entry )->entry_hash );

			// Swaps the 32bit ints of the 64bit int.
			_asm mov eax, dword ptr entry_hash;
			_asm mov ecx, dword ptr entry_hash + 4;
			_asm mov dword ptr entry_hash, ecx;
			_asm mov dword ptr entry_hash + 4, eax;

			// The entry hash may be the same as the filename.
			wchar_t s_entry_hash[ 19 ] = { 0 };
			swprintf_s( s_entry_hash, 19, L"0x%08x%08x", entry_hash, entry_hash + 4 );	// This will probably be the same as the file name.
			wprintf_s( L"Entry hash: %s\n", s_entry_hash );

			// Windows Vista
			if ( dh.version == WINDOWS_VISTA )
			{
				// UTF-16 file extension.
				wprintf_s( L"File extension: %s\n", ( ( database_cache_entry_vista * )database_cache_entry )->extension );
			}

			printf( "Identifier string size: %lu bytes\n", filename_length );

			// Padding size.
			unsigned int padding_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->padding_size : ( ( database_cache_entry_vista * )database_cache_entry )->padding_size );
			printf( "Padding size: %lu bytes\n", padding_size );

			// The size of our data.
			unsigned int data_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_size : ( ( database_cache_entry_vista * )database_cache_entry )->data_size );
			printf( "Data size: %lu bytes\n", data_size );

			// Unknown value.
			unsigned int unknown = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->unknown : ( ( database_cache_entry_vista * )database_cache_entry )->unknown );
			printf( "Unknown value: 0x%04x\n", unknown );

			long long data_checksum = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_checksum : ( ( database_cache_entry_vista * )database_cache_entry )->data_checksum );
			
			// Swaps the 32bit ints of the 64bit int.
			_asm mov eax, dword ptr data_checksum;
			_asm mov ecx, dword ptr data_checksum + 4;
			_asm mov dword ptr data_checksum, ecx;
			_asm mov dword ptr data_checksum + 4, eax;

			// CRC-64 data checksum.
			wchar_t s_data_checksum[ 19 ] = { 0 };
			swprintf_s( s_data_checksum, 19, L"0x%08x%08x", data_checksum, data_checksum + 4 );
			wprintf_s( L"Data checksum (CRC-64): %s\n", s_data_checksum );

			long long header_checksum = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->header_checksum : ( ( database_cache_entry_vista * )database_cache_entry )->header_checksum );

			// Swaps the 32bit ints of the 64bit int.
			_asm mov eax, dword ptr header_checksum;
			_asm mov ecx, dword ptr header_checksum + 4;
			_asm mov dword ptr header_checksum, ecx;
			_asm mov dword ptr header_checksum + 4, eax;

			// CRC-64 header checksum.
			wchar_t s_header_checksum[ 19 ] = { 0 };
			swprintf_s( s_header_checksum, 19, L"0x%08x%08x", header_checksum, header_checksum + 4 );
			wprintf_s( L"Header checksum (CRC-64): %s\n", s_header_checksum );

			// It's unlikely that a filename will be longer than MAX_PATH, but to be on the safe side, we should truncate it if it is.
			unsigned short filename_truncate_length = min( filename_length, ( sizeof( wchar_t ) * MAX_PATH ) );

			// UTF-16 filename. Allocate the filename length plus 5 for the unicode extension and null character.
			wchar_t *filename = ( wchar_t * )malloc( filename_truncate_length + ( sizeof( wchar_t ) * 5 ) );
			memset( filename, 0, filename_truncate_length + ( sizeof( wchar_t ) * 5 ) );
			ReadFile( hFile, filename, filename_truncate_length, &read, NULL );
			if ( read == 0 )
			{
				free( filename );
				free( database_cache_entry );
				CloseHandle( hFile );
				printf( "End of file reached. There are no more valid entires.\n" );
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
					printf( "End of file reached. There are no more valid entires.\n" );
					return 0;
				}
			}

			// This will set our file pointer to the beginning of the data entry.
			file_position = SetFilePointer( hFile, padding_size, 0, FILE_CURRENT );
			if ( file_position == INVALID_SET_FILE_POINTER )
			{
				free( filename );
				free( database_cache_entry );
				CloseHandle( hFile );
				printf( "End of file reached. There are no more valid entires.\n" );
				return 0;
			}

			// Retrieve the data content.
			char *buf = NULL;
			
			if ( data_size != 0 )
			{
				buf = ( char * )malloc( sizeof( char ) * data_size );
				ReadFile( hFile, buf, data_size, &read, NULL );
				if ( read == 0 )
				{
					free( buf );
					free( filename );
					free( database_cache_entry );
					CloseHandle( hFile );
					printf( "End of file reached. There are no more valid entires.\n" );
					return 0;
				}

				// Detect the file extension and copy it into the filename string.
				if ( memcmp( buf, FILE_TYPE_BMP, 2 ) == 0 )			// First 3 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".bmp", 5 );
				}
				else if ( memcmp( buf, FILE_TYPE_JPEG, 4 ) == 0 )	// First 4 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".jpg", 5 );
				}
				else if ( memcmp( buf, FILE_TYPE_PNG, 8 ) == 0 )	// First 8 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".png", 5 );
				}
				else if ( dh.version == WINDOWS_VISTA && wcslen( ( ( database_cache_entry_vista * )database_cache_entry )->extension ) > 0 )	// If it's a Windows Vista thumbcache file and we can't detect the extension, then use the one given.
				{
					swprintf_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".%s", ( ( database_cache_entry_vista * )database_cache_entry )->extension ); 
				}
			}
			else
			{
				// Windows Vista thumbcache files should include the extension.
				if ( dh.version == WINDOWS_VISTA && wcslen( ( ( database_cache_entry_vista * )database_cache_entry )->extension ) > 0 )
				{
					swprintf_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 5, L".%s", ( ( database_cache_entry_vista * )database_cache_entry )->extension ); 
				}
			}

			wprintf_s( L"Identifier string: %s\n", filename );

			// Output the data with the given (UTF-16) filename.
			printf( "---------------------------------------------\n" );
			printf( "Writing data to file.\n" );

			// Attempt to save the buffer to a file.
			HANDLE hFile_save = CreateFile( filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			if ( hFile_save != INVALID_HANDLE_VALUE )
			{
				DWORD written = 0;
				WriteFile( hFile_save, buf, data_size, &written, NULL );
				CloseHandle( hFile_save );
				printf( "Writing complete.\n" );
			}
			else
			{
				printf( "Writing failed.\n" );
			}
			printf( "---------------------------------------------\n" );

			// Delete our data buffer.
			free( buf );

			// Delete our filename.
			free( filename );

			// Delete our database cache entry.
			free( database_cache_entry );
		}
		// Close the input file.
		CloseHandle( hFile );
	}
	else
	{
		// See if they typed an incorrect filename.
		if ( GetLastError() == ERROR_FILE_NOT_FOUND )
		{
			printf( "The database file does not exist.\n" );
		}
		else	// For all other errors, it probably failed to open.
		{
			printf( "The database file failed to open.\n" );
		}
	}

	return 0;
}
