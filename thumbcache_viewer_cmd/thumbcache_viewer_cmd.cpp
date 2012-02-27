/*
    thumbcache_viewer_cmd will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2012 Eric Kutcher

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
#define WINDOWS_8		0x1A

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

// Window 8 Thumbcache entry.
struct database_cache_entry_8
{
	char magic_identifier[ 4 ];
	unsigned int cache_entry_size;
	long long entry_hash;
	unsigned int filename_length;
	unsigned int padding_size;
	unsigned int data_size;
	unsigned int width;
	unsigned int height;
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

bool scan_memory( HANDLE hFile, unsigned int &offset )
{
	// Allocate a 100 kilobyte chunk of memory to scan. This value is arbitrary.
	char *buf = ( char * )malloc( sizeof( char ) * 102400 );
	char *scan = NULL;
	DWORD read = 0;

	while ( true )
	{
		// Begin reading through the database.
		ReadFile( hFile, buf, sizeof( char ) * 102400, &read, NULL );
		if ( read == 0 )
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

int wmain( int argc, wchar_t *argv[] )
{
	bool output_html = false;
	bool output_csv = false;
	bool skip_blank = false;
	bool extract_thumbnails = true;

	// Ask user for input filename.
	wchar_t name[ MAX_PATH ] = { 0 };
	wchar_t output_path[ MAX_PATH ] = { 0 };
	if ( argc == 1 )
	{
		printf( "Please enter the name of the database: " );
		fgetws( name, MAX_PATH, stdin );

		// Remove the newline character if it was appended.
		int input_length = wcslen( name );
		if ( name[ input_length - 1 ] == L'\n' )
		{
			name[ input_length - 1 ] = L'\0';
		}

		printf( "Select a report to output:\n 1\tHTML\n 2\tComma-separated values (CSV)\n 3\tHTML and CSV\n 0\tNo report\nSelect: " );
		wint_t choice = getwchar();	// Newline character will remain in buffer.
		if ( choice == L'1' )
		{
			output_html = true;
		}
		else if ( choice == L'2' )
		{
			output_csv = true;
		}
		else if ( choice == L'3' )
		{
			output_html = output_csv = true;
		}

		if ( output_html == true || output_csv == true )
		{
			printf( "Do you want to skip reporting 0 byte files? (Y/N) " );
			while ( getwchar() != L'\n' );	// Clear the input buffer.
			choice = getwchar();		// Newline character will remain in buffer.
			if ( choice == L'y' || choice == L'Y' )
			{
				skip_blank = true;
			}

			printf( "Do you want to extract the thumbnail images? (Y/N) " );
			while ( getwchar() != L'\n' );	// Clear the input buffer.
			choice = getwchar();				// Newline character will remain in buffer.
			if ( choice == L'n' || choice == L'N' )
			{
				extract_thumbnails = false;
			}
		}

		while ( getwchar() != L'\n' );		// Clear the input buffer.

		printf( "Please enter a path to output the database files (Press Enter for the current directory): " );
		fgetws( output_path, MAX_PATH, stdin );

		// Remove the newline character if it was appended.
		input_length = wcslen( output_path );
		if ( output_path[ input_length - 1 ] == L'\n' )
		{
			output_path[ input_length - 1 ] = L'\0';
		}
	}
	else
	{
		// We're going to designate the last argument as the database path.
		int arg_len = wcslen( argv[ argc - 1 ] );
		wmemcpy_s( name, MAX_PATH, argv[ argc - 1 ], ( arg_len > MAX_PATH ? MAX_PATH : arg_len ) );

		// Go through each argument and set the appropriate switch.
		for ( int i = 1; i <= ( argc - 1 ); i++ )
		{
			if ( wcslen( argv[ i ] ) > 1 && ( argv[ i ][ 0 ] == '-' || argv[ i ][ 0 ] == '/' ) )
			{
				switch ( argv[ i ][ 1 ] )
				{
					case 'o':
					case 'O':
					{
						// Make sure our output switch is not the second to last argument.
						if ( i < ( argc - 2 ) )
						{
							arg_len = wcslen( argv[ ++i ] );
							wmemcpy_s( output_path, MAX_PATH, argv[ i ], ( arg_len > MAX_PATH ? MAX_PATH : arg_len ) );
						}
					}
					break;

					case 'w':
					case 'W':
					{
						output_html = true;
					}
					break;

					case 'c':
					case 'C':
					{
						output_csv = true;
					}
					break;

					case 'z':
					case 'Z':
					{
						skip_blank = true;
					}
					break;

					case 'n':
					case 'N':
					{
						extract_thumbnails = false;
					}
					break;

					case '?':
					case 'h':
					case 'H':
					{
						printf( "\nthumbcache_viewer_cmd [-o directory][-w][-c][-z][-n] thumbcache_*.db\n" \
								" -o\tSet the output directory for thumbnails and reports.\n" \
								" -w\tGenerate an HTML report.\n" \
								" -c\tGenerate a comma-separated values (CSV) report.\n" \
								" -z\tIgnore 0 byte files when generating a report.\n" \
								" -n\tDo not extract thumbnails.\n");
						return 0;
					}
					break;

					case 'a':
					case 'A':
					{
						printf( "\nThumbcache Viewer is made free under the GPLv3 license.\nCopyright (c) 2011-2012 Eric Kutcher\n" );
						return 0;
					}
					break;
				}
			}
		}
	}

	printf( "Attempting to open the database file.\n" );

	// Attempt to open our database file.
	HANDLE hFile = CreateFile( name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		DWORD read = 0;
		DWORD written = 0;

		unsigned int file_offset = 0;

		int utf8_path_length = 0;
		char *utf8_path = NULL;
		int utf8_name_length = 0;
		char *utf8_name = NULL;
		
		HANDLE hFile_html = INVALID_HANDLE_VALUE;
		HANDLE hFile_csv = INVALID_HANDLE_VALUE;

		database_header dh = { 0 };
		ReadFile( hFile, &dh, sizeof( database_header ), &read, NULL );

		// Make sure it's a thumbcache database and the stucture was filled correctly.
		if ( memcmp( dh.magic_identifier, "CMMM", 4 ) != 0 || read != sizeof( database_header ) )
		{
			CloseHandle( hFile );
			printf( "The file is not a thumbcache database.\n" );
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
		else if ( dh.version == WINDOWS_8 )
		{
			printf( "Version: Windows 8\n" );
		}
		else
		{
			CloseHandle( hFile );
			printf( "Database is not supported.\n" );
			return 0;
		}

		// Type of thumbcache database.
		if ( dh.version != WINDOWS_8 )	// Windows Vista & 7: 00 = 32, 01 = 96, 02 = 256, 03 = 1024, 04 = sr
		{
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
				printf( "Cache type: thumbcache_1024.db, 1024x1024\n" );
			}
			else if ( dh.type == 0x04 )
			{
				printf( "Cache type: thumbcache_sr.db\n" );
			}
			else
			{
				printf( "Cache type: Unknown\n" );
			}
		}
		else // Windows 8: 00 = 16, 01 = 32, 02 = 48, 03 = 96, 04 = 256, 05 = 1024, 06 = sr, 07 = wide, 08 = exif
		{
			if ( dh.type == 0x00 )
			{
				printf( "Cache type: thumbcache_16.db, 16x16\n" );
			}
			else if ( dh.type == 0x01 )
			{
				printf( "Cache type: thumbcache_32.db, 32x32\n" );
			}
			else if ( dh.type == 0x02 )
			{
				printf( "Cache type: thumbcache_48.db, 48x48\n" );
			}
			else if ( dh.type == 0x03 )
			{
				printf( "Cache type: thumbcache_96.db, 96x96\n" );
			}
			else if ( dh.type == 0x04 )
			{
				printf( "Cache type: thumbcache_256.db, 256x256\n" );
			}
			else if ( dh.type == 0x05 )
			{
				printf( "Cache type: thumbcache_1024.db, 1024x1024\n" );
			}
			else if ( dh.type == 0x06 )
			{
				printf( "Cache type: thumbcache_sr.db\n" );
			}
			else if ( dh.type == 0x07 )
			{
				printf( "Cache type: thumbcache_wide.db\n" );
			}
			else if ( dh.type == 0x08 )
			{
				printf( "Cache type: thumbcache_exif.db\n" );
			}
			else
			{
				printf( "Cache type: Unknown\n" );
			}
		}

		// Offset to the first cache entry.
		printf( "Offset to first cache entry: %lu bytes\n", dh.first_cache_entry );

		// Offset to the available cache entry.
		printf( "Offset to available cache entry: %lu bytes\n", dh.available_cache_entry );

		// Number of cache entries.
		printf( "Number of cache entries: %lu\n", dh.number_of_cache_entries );

		// Set the file pointer to the first possible cache entry. (Should be at an offset of 24 bytes)
		unsigned int current_position = 24;

		// Create and set the directory that we'll be outputting files to.
		if ( GetFileAttributes( output_path ) == INVALID_FILE_ATTRIBUTES )
		{
			CreateDirectory( output_path, NULL );
		}

		SetCurrentDirectory( output_path );				// Set the path (relative or full)
		GetCurrentDirectory( MAX_PATH, output_path );	// Get the full path

		// Convert our wide character strings to UTF-8 if we're going to output a report.
		if ( output_html == true || output_csv == true )
		{
			utf8_path_length = WideCharToMultiByte( CP_UTF8, 0, output_path, -1, NULL, 0, NULL, NULL );
			utf8_path = ( char * )malloc( sizeof( char ) * utf8_path_length );
			WideCharToMultiByte( CP_UTF8, 0, output_path, -1, utf8_path, utf8_path_length, NULL, NULL );

			utf8_name_length = WideCharToMultiByte( CP_UTF8, 0, name, -1, NULL, 0, NULL, NULL );
			utf8_name = ( char * )malloc( sizeof( char ) * utf8_name_length );
			WideCharToMultiByte( CP_UTF8, 0, name, -1, utf8_name, utf8_name_length, NULL, NULL );
		}

		// Create the HTML report file.
		if ( output_html == true )
		{
			hFile_html = CreateFileA( "Report.html", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			if ( hFile_html == INVALID_HANDLE_VALUE )
			{
				printf( "HTML report could not be created.\n" );
				output_html = false;
			}
			else
			{
				// Add UTF-8 marker (BOM) if we're at the beginning of the file.
				if ( SetFilePointer( hFile_html, 0, NULL, FILE_END ) == 0 )
				{
					WriteFile( hFile_html, "\xEF\xBB\xBF", 3, &written, NULL );
				}

				char *buf = ( char * )malloc( sizeof( char ) * ( utf8_name_length + utf8_path_length + 557 ) );
				int write_size = sprintf_s( buf, utf8_name_length + utf8_path_length + 557, 
								"<html><head><title>HTML Report</title></head><body>Filename: %s<br />" \
								"Version: %s<br />" \
								"Type: %s<br />" \
								"Offset to first cache entry (bytes): %lu<br />" \
								"Offset to available cache entry (bytes): %lu<br />" \
								"Number of cache entries: %lu<br />" \
								"Output path: %s\\<br /><br />" \
								"<table border=1 cellspacing=0><tr><td>Index</td><td>Offset (bytes)</td><td>Cache Size (bytes)</td><td>Data Size (bytes)</td>%s<td>Entry Hash</td><td>Data Checksum</td><td>Header Checksum</td><td>Indentifier String</td><td>Image</td></tr>",
								utf8_name, ( dh.version == WINDOWS_VISTA ? "Windows Vista" : ( dh.version == WINDOWS_7 ? "Windows 7" : "Windows 8" ) ),
								( dh.version != WINDOWS_8 ) ? \
								( dh.type == 0x00 ? "thumbcache_32.db" : ( dh.type == 0x01 ? "thumbcache_96.db" : ( dh.type == 0x02 ? "thumbcache_256.db" : ( dh.type == 0x03 ? "thumbcache_1024.db" : ( dh.type == 0x04 ? "thumbcache_sr.db" : "Unknown" ) ) ) ) ) : \
								( dh.type == 0x00 ? "thumbcache_16.db" : ( dh.type == 0x01 ? "thumbcache_32.db" : ( dh.type == 0x02 ? "thumbcache_48.db" : ( dh.type == 0x03 ? "thumbcache_96.db" : ( dh.type == 0x04 ? "thumbcache_256.db" : ( dh.type == 0x05 ? "thumbcache_1024.db" : ( dh.type == 0x06 ? "thumbcache_sr.db" : ( dh.type == 0x07 ? "thumbcache_wide.db" : ( dh.type == 0x08 ? "thumbcache_exif.db" : "Unknown" ) ) ) ) ) ) ) ) ),
								dh.first_cache_entry, dh.available_cache_entry, dh.number_of_cache_entries, utf8_path, ( dh.version == WINDOWS_8 ? "<td>Dimensions</td>" : "" ) );
				WriteFile( hFile_html, buf, write_size, &written, NULL );

				free( buf );
			}
		}

		// Create the CSV report file.
		if ( output_csv == true )
		{
			hFile_csv = CreateFileA( "Report.csv", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			if ( hFile_csv == INVALID_HANDLE_VALUE )
			{
				printf( "CVS report could not be created.\n" );
				output_csv = false;
			}
			else
			{
				// Add UTF-8 marker (BOM) if we're at the beginning of the file.
				if ( SetFilePointer( hFile_csv, 0, NULL, FILE_END ) == 0 )
				{
					WriteFile( hFile_csv, "\xEF\xBB\xBF", 3, &written, NULL );
				}

				char *buf = ( char * )malloc( sizeof( char ) * ( utf8_name_length + utf8_path_length + 347 ) );
				int write_size = sprintf_s( buf, utf8_name_length + utf8_path_length + 347,
								"Filename,\"%s\"\r\n" \
								"Version,%s\r\n" \
								"Type,%s\r\n" \
								"Offset to first cache entry (bytes),%lu\r\n" \
								"Offset to available cache entry (bytes),%lu\r\n" \
								"Number of cache entries,%lu\r\n" \
								"Output path,\"%s\\\"\r\n\r\n" \
								"Index,Offset (bytes),Cache Size (bytes),Data Size (bytes),%sEntry Hash,Data Checksum,Header Checksum,Indentifier String\r\n",
								utf8_name, ( dh.version == WINDOWS_VISTA ? "Windows Vista" : ( dh.version == WINDOWS_7 ? "Windows 7" : "Windows 8" ) ),
								( dh.version != WINDOWS_8 ) ? \
								( dh.type == 0x00 ? "thumbcache_32.db" : ( dh.type == 0x01 ? "thumbcache_96.db" : ( dh.type == 0x02 ? "thumbcache_256.db" : ( dh.type == 0x03 ? "thumbcache_1024.db" : ( dh.type == 0x04 ? "thumbcache_sr.db" : "Unknown" ) ) ) ) ) : \
								( dh.type == 0x00 ? "thumbcache_16.db" : ( dh.type == 0x01 ? "thumbcache_32.db" : ( dh.type == 0x02 ? "thumbcache_48.db" : ( dh.type == 0x03 ? "thumbcache_96.db" : ( dh.type == 0x04 ? "thumbcache_256.db" : ( dh.type == 0x05 ? "thumbcache_1024.db" : ( dh.type == 0x06 ? "thumbcache_sr.db" : ( dh.type == 0x07 ? "thumbcache_wide.db" : ( dh.type == 0x08 ? "thumbcache_exif.db" : "Unknown" ) ) ) ) ) ) ) ) ),
								dh.first_cache_entry, dh.available_cache_entry, dh.number_of_cache_entries, utf8_path, ( dh.version == WINDOWS_8 ? "Dimensions," : "" ) );
				WriteFile( hFile_csv, buf, write_size, &written, NULL );

				free( buf );
			}
		}
		
		// Free our UTF-8 strings.
		free( utf8_name );
		free( utf8_path );

		// Go through our database and attempt to extract each cache entry.
		for ( unsigned int i = 0; i < dh.number_of_cache_entries; i++ )
		{
			printf( "\n---------------------------------------------\n" );
			printf( "Extracting cache entry %lu at %lu bytes.\n", i + 1, current_position );
			printf( "---------------------------------------------\n" );

			file_offset = current_position;	// Save for our report files.

			// Set the file pointer to the end of the last cache entry.
			current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );
			if ( current_position == INVALID_SET_FILE_POINTER )
			{
				printf( "End of file reached. There are no more entries.\n" );
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
					free( database_cache_entry );
					printf( "End of file reached. There are no more entries.\n" );
					break;
				}
				else if ( memcmp( ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );

					printf( "Invalid cache entry located at %lu bytes.\n", current_position );
					printf( "Attempting to scan for next entry.\n" );

					// Walk back to the end of the last cache entry.
					current_position = SetFilePointer( hFile, current_position - read, NULL, FILE_BEGIN );

					// If we found the beginning of the entry, attempt to read it again.
					if ( scan_memory( hFile, current_position ) == true )
					{
						printf( "A valid entry has been found.\n" );
						printf( "---------------------------------------------\n" );
						--i;
						continue;
					}

					printf( "Scan failed to find any valid entries.\n" );
					printf( "---------------------------------------------\n" );
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
					free( database_cache_entry );
					printf( "End of file reached. There are no more entries.\n" );
					break;
				}
				else if ( memcmp( ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );

					printf( "Invalid cache entry located at %lu bytes.\n", current_position );
					printf( "Attempting to scan for next entry.\n" );

					// Walk back to the end of the last cache entry.
					current_position = SetFilePointer( hFile, current_position - read, NULL, FILE_BEGIN );

					// If we found the beginning of the entry, attempt to read it again.
					if ( scan_memory( hFile, current_position ) == true )
					{
						printf( "A valid entry has been found.\n" );
						printf( "---------------------------------------------\n" );
						--i;
						continue;
					}

					printf( "Scan failed to find any valid entries.\n" );
					printf( "---------------------------------------------\n" );
					break;
				}
			}
			else if ( dh.version == WINDOWS_8 )
			{
				database_cache_entry = ( database_cache_entry_8 * )malloc( sizeof( database_cache_entry_8 ) );
				ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_8 ), &read, NULL );
				
				// Make sure it's a thumbcache database and the stucture was filled correctly.
				if ( read != sizeof( database_cache_entry_8 ) )
				{
					free( database_cache_entry );
					printf( "End of file reached. There are no more entries.\n" );
					break;
				}
				else if ( memcmp( ( ( database_cache_entry_8 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );

					printf( "Invalid cache entry located at %lu bytes.\n", current_position );
					printf( "Attempting to scan for next entry.\n" );

					// Walk back to the end of the last cache entry.
					current_position = SetFilePointer( hFile, current_position - read, NULL, FILE_BEGIN );

					// If we found the beginning of the entry, attempt to read it again.
					if ( scan_memory( hFile, current_position ) == true )
					{
						printf( "A valid entry has been found.\n" );
						printf( "---------------------------------------------\n" );
						--i;
						continue;
					}

					printf( "Scan failed to find any valid entries.\n" );
					printf( "---------------------------------------------\n" );
					break;
				}
			}

			// Cache size includes the 4 byte signature and itself ( 4 bytes ).
			unsigned int cache_entry_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->cache_entry_size : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->cache_entry_size : ( ( database_cache_entry_vista * )database_cache_entry )->cache_entry_size ) );		
			
			current_position += cache_entry_size;

			// The length of our filename.
			unsigned int filename_length = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->filename_length : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->filename_length : ( ( database_cache_entry_vista * )database_cache_entry )->filename_length ) );

			// Skip blank filenames.
			if ( filename_length == 0 )
			{
				// Free each database entry that we've skipped over.
				free( database_cache_entry );

				continue;
			}

			// The magic identifier for the current entry.
			char *magic_identifier = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->magic_identifier : ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier ) );
			memcpy( stmp, magic_identifier, sizeof( char ) * 4 );
			printf( "Signature (magic identifier): %s\n", stmp );

			printf( "Cache size: %lu bytes\n", cache_entry_size );

			// The entry hash may be the same as the filename.
			char s_entry_hash[ 19 ] = { 0 };
			sprintf_s( s_entry_hash, 19, "0x%016llx", ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->entry_hash : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->entry_hash : ( ( database_cache_entry_vista * )database_cache_entry )->entry_hash ) ) );	// This will probably be the same as the file name.
			printf_s( "Entry hash: %s\n", s_entry_hash );

			// Windows Vista
			if ( dh.version == WINDOWS_VISTA )
			{
				// UTF-16 file extension.
				wprintf_s( L"File extension: %.4s\n", ( ( database_cache_entry_vista * )database_cache_entry )->extension );
			}

			printf( "Identifier string size: %lu bytes\n", filename_length );

			// Padding size.
			unsigned int padding_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->padding_size : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->padding_size : ( ( database_cache_entry_vista * )database_cache_entry )->padding_size ) );
			printf( "Padding size: %lu bytes\n", padding_size );

			// The size of our data.
			unsigned int data_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_size : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->data_size : ( ( database_cache_entry_vista * )database_cache_entry )->data_size ) );
			printf( "Data size: %lu bytes\n", data_size );

			// Windows 8 contains the width and height of the image.
			if ( dh.version == WINDOWS_8 )
			{
				printf( "Dimensions: %lux%lu\n", ( ( database_cache_entry_8 * )database_cache_entry )->width, ( ( database_cache_entry_8 * )database_cache_entry )->height );
			}

			// Unknown value.
			unsigned int unknown = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->unknown : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->unknown : ( ( database_cache_entry_vista * )database_cache_entry )->unknown ) );
			printf( "Unknown value: 0x%04x\n", unknown );

			// CRC-64 data checksum.
			char s_data_checksum[ 19 ] = { 0 };
			sprintf_s( s_data_checksum, 19, "0x%016llx", ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_checksum : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->data_checksum : ( ( database_cache_entry_vista * )database_cache_entry )->data_checksum ) ) );
			printf_s( "Data checksum (CRC-64): %s\n", s_data_checksum );

			// CRC-64 header checksum.
			char s_header_checksum[ 19 ] = { 0 };
			sprintf_s( s_header_checksum, 19, "0x%016llx", ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->header_checksum : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->header_checksum : ( ( database_cache_entry_vista * )database_cache_entry )->header_checksum ) ) );
			printf_s( "Header checksum (CRC-64): %s\n", s_header_checksum );

			// Since the database can store CLSIDs that extend beyond MAX_PATH, we'll have to set a larger truncation length. A length of 32767 would probably never be seen. 
			unsigned int filename_truncate_length = min( filename_length, ( sizeof( wchar_t ) * SHRT_MAX ) );

			// UTF-16 filename. Allocate the filename length plus 6 for the unicode extension and null character.
			wchar_t *filename = ( wchar_t * )malloc( filename_truncate_length + ( sizeof( wchar_t ) * 6 ) );
			memset( filename, 0, filename_truncate_length + ( sizeof( wchar_t ) * 6 ) );
			ReadFile( hFile, filename, filename_truncate_length, &read, NULL );
			if ( read == 0 )
			{
				free( filename );
				free( database_cache_entry );
				printf( "End of file reached. There are no more valid entries.\n" );
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
					printf( "End of file reached. There are no more valid entries.\n" );
					break;
				}
			}

			// This will set our file pointer to the beginning of the data entry.
			file_position = SetFilePointer( hFile, padding_size, 0, FILE_CURRENT );
			if ( file_position == INVALID_SET_FILE_POINTER )
			{
				free( filename );
				free( database_cache_entry );
				printf( "End of file reached. There are no more valid entries.\n" );
				break;
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
					printf( "End of file reached. There are no more valid entries.\n" );
					break;
				}

				// Detect the file extension and copy it into the filename string.
				if ( memcmp( buf, FILE_TYPE_BMP, 2 ) == 0 )			// First 3 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".bmp", 4 );
				}
				else if ( memcmp( buf, FILE_TYPE_JPEG, 4 ) == 0 )	// First 4 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".jpg", 4 );
				}
				else if ( memcmp( buf, FILE_TYPE_PNG, 8 ) == 0 )	// First 8 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".png", 4 );
				}
				else if ( dh.version == WINDOWS_VISTA && ( ( database_cache_entry_vista * )database_cache_entry )->extension[ 0 ] != NULL )	// If it's a Windows Vista thumbcache file and we can't detect the extension, then use the one given.
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 1, L".", 1 );
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ) + 1, 4, ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 );
				}
			}
			else
			{
				// Windows Vista thumbcache files should include the extension.
				if ( dh.version == WINDOWS_VISTA && ( ( database_cache_entry_vista * )database_cache_entry )->extension[ 0 ] != NULL )
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 1, L".", 1 );
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ) + 1, 4, ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 ); 
				}
			}

			wprintf_s( L"Identifier string: %s\n", filename );

			char *utf8_filename = NULL;
			int utf8_filename_length = 0;

			// Write the entry to a new table row in the HTML report file.
			if ( output_html == true && ( skip_blank == false || ( skip_blank == true && data_size > 0 ) ) )
			{
				char buf[ 196 ];
				int write_size = 0;
				if ( dh.version == WINDOWS_8 )	// Windows 8 includes dimensions (width x height)
				{
					write_size = sprintf_s( buf, 196, "<tr><td>%lu</td><td>%lu</td><td>%lu</td><td>%lu</td><td>%lux%lu</td><td>%s</td><td>%s</td><td>%s</td><td>", i + 1, file_offset, cache_entry_size, data_size, ( ( database_cache_entry_8 * )database_cache_entry )->width, ( ( database_cache_entry_8 * )database_cache_entry )->height, s_entry_hash, s_data_checksum, s_header_checksum );
				}
				else
				{
					write_size = sprintf_s( buf, 196, "<tr><td>%lu</td><td>%lu</td><td>%lu</td><td>%lu</td><td>%s</td><td>%s</td><td>%s</td><td>", i + 1, file_offset, cache_entry_size, data_size, s_entry_hash, s_data_checksum, s_header_checksum );
				}
				WriteFile( hFile_html, buf, write_size, &written, NULL );

				utf8_filename_length = WideCharToMultiByte( CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL );
				utf8_filename = ( char * )malloc( sizeof( char ) * utf8_filename_length );	// Includes NULL character.
				WideCharToMultiByte( CP_UTF8, 0, filename, -1, utf8_filename, utf8_filename_length, NULL, NULL );
				WriteFile( hFile_html, utf8_filename, utf8_filename_length - 1, &written, NULL );

				// If there's an image we want to extract, then insert it into the last column.
				if ( data_size != 0 && extract_thumbnails == true )
				{
					char *out_buf = ( char * )malloc( sizeof( char ) * ( utf8_filename_length + 33 ) );
					write_size = sprintf_s( out_buf, utf8_filename_length + 33, "</td><td><img src=\"%s\" /></td></tr>", utf8_filename );

					WriteFile( hFile_html, out_buf, write_size, &written, NULL );

					free( out_buf );
				}
				else	// Otherwise, the column will remain empty.
				{
					WriteFile( hFile_html, "</td><td></td></tr>", 19, &written, NULL );
				}

				// Save the filename if we're going to output a cvs file. Cuts down on the number of conversions.
				if ( output_csv == false )
				{
					free( utf8_filename );
				}
			}

			// Write the entry to a new line in the CSV report file.
			if ( output_csv == true && ( skip_blank == false || ( skip_blank == true && data_size > 0 ) ) )
			{
				char buf[ 125 ];
				int write_size = 0;
				if ( dh.version == WINDOWS_8 )	// Windows 8 includes dimensions (width x height)
				{
					write_size = sprintf_s( buf, 125, "%lu,%lu,%lu,%lu,%lux%lu,%s,%s,%s,\"", i + 1, file_offset, cache_entry_size, data_size, ( ( database_cache_entry_8 * )database_cache_entry )->width, ( ( database_cache_entry_8 * )database_cache_entry )->height, s_entry_hash, s_data_checksum, s_header_checksum );
				}
				else
				{
					write_size = sprintf_s( buf, 125, "%lu,%lu,%lu,%lu,%s,%s,%s,\"", i + 1, file_offset, cache_entry_size, data_size, s_entry_hash, s_data_checksum, s_header_checksum );
				}
				WriteFile( hFile_csv, buf, write_size, &written, NULL );

				if ( utf8_filename == NULL )
				{
					utf8_filename_length = WideCharToMultiByte( CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL );
					utf8_filename = ( char * )malloc( sizeof( char ) * utf8_filename_length );	// Includes NULL character.
					WideCharToMultiByte( CP_UTF8, 0, filename, -1, utf8_filename, utf8_filename_length, NULL, NULL );
				}

				char *out_buf = ( char * )malloc( sizeof( char ) * ( utf8_filename_length + 3 ) );
				write_size = sprintf_s( out_buf, utf8_filename_length + 3, "%s\"\r\n", utf8_filename );
				WriteFile( hFile_csv, out_buf, write_size, &written, NULL );

				free( out_buf );
				free( utf8_filename );
			}

			// Output the data with the given (UTF-16) filename.
			printf( "---------------------------------------------\n" );
			if ( data_size != 0 && extract_thumbnails == true )
			{
				printf( "Writing data to file.\n" );
				// Attempt to save the buffer to a file.
				HANDLE hFile_save = CreateFile( filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
				if ( hFile_save != INVALID_HANDLE_VALUE )
				{
					WriteFile( hFile_save, buf, data_size, &written, NULL );
					CloseHandle( hFile_save );
					printf( "Writing complete.\n" );
				}
				else
				{
					printf( "Writing failed.\n" );
				}
			}
			else if ( extract_thumbnails == false )
			{
				printf( "Writing skipped.\n" );
			}
			else
			{
				printf( "No data to write.\n" );
			}
			printf( "---------------------------------------------\n" );

			// Delete our data buffer.
			free( buf );

			// Delete our filename.
			free( filename );

			// Delete our database cache entry.
			free( database_cache_entry );
		}

		// Close our HTML report.
		if ( output_html == true )
		{
			WriteFile( hFile_html, "</table><br /></body></html>", 28, &written, NULL );
			CloseHandle( hFile_html );
		}

		// Close our CSV report.
		if ( output_csv == true )
		{
			WriteFile( hFile_csv, "\r\n", 2, &written, NULL );
			CloseHandle( hFile_csv );
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
