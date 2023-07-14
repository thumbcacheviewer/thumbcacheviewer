/*
	thumbcache_viewer_cmd will extract thumbnail images from thumbcache database files.
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

#include "lite_mssrch.h"
#include "lite_msscb.h"
#include "lite_sqlite3.h"

#include "utilities.h"
#include "map_entries.h"
#include "read_esedb.h"
#include "read_sqlitedb.h"

#include <io.h>
#include <fcntl.h>

#include "dllrbt.h"

char g_query[ 512 ];
unsigned char g_database_type = 0;	// 0 = None, 1 = ESE, 2 = SQLite

int dllrbt_compare( void *a, void *b )
{
	unsigned long long hash1 = ( unsigned long long )a;
	unsigned long long hash2 = ( unsigned long long )b;

	if ( hash1 > hash2 )
	{
		return 1;
	}

	if ( hash1 < hash2 )
	{
		return -1;
	}

	return 0;
}

void TraverseSQLiteDatabase( wchar_t *database_filepath )
{
	char *sql_err_msg = NULL;

	int filepath_length = WideCharToMultiByte( CP_ACP, 0, database_filepath, -1, NULL, 0, NULL, NULL );
	char *ascii_filepath = ( char * )malloc( sizeof( char ) * filepath_length ); // Size includes the null character.
	WideCharToMultiByte( CP_ACP, 0, database_filepath, -1, ascii_filepath, filepath_length, NULL, NULL );

	int sql_rc = sqlite3_open_v2( ascii_filepath, &g_sql_db, SQLITE_OPEN_READONLY, NULL );
	if ( sql_rc )
	{
		// sqlite3_errmsg16( g_sql_db );
		printf( "The SQLite database could not be opened.\n" );

		goto CLEANUP;
	}

	sql_rc = sqlite3_exec( g_sql_db, "SELECT Id, UniqueKey FROM SystemIndex_1_PropertyStore_Metadata", BuildPropertyTreeCallback, NULL, &sql_err_msg );
	if ( sql_rc != SQLITE_OK )
	{
		goto CLEANUP;
	}

	memcpy_s( g_query, 512,
		"SELECT WorkId, Id, UniqueKey, Value, VariantType FROM SystemIndex_1_PropertyStore " \
		"JOIN SystemIndex_1_PropertyStore_Metadata ON SystemIndex_1_PropertyStore_Metadata.Id = SystemIndex_1_PropertyStore.ColumnId " \
		"WHERE WorkId IN ( SELECT WorkId FROM SystemIndex_1_PropertyStore WHERE Value = (X\'", 288 );

CLEANUP:

	free( ascii_filepath );

	if ( sql_err_msg != NULL )
	{
		printf( "SQLite: %s\n", sql_err_msg );

		sqlite3_free( sql_err_msg );
	}
}

// The Microsoft Jet Database Engine seems to have a lot of annoying quirks/compatibility issues.
// The directory scanner is a nice compliment should this function not work 100%.
// Ideally, the database being scanned should be done with the same esent.dll that was used to create it.
// If there are issues with the database, make a copy and use esentutl.exe to fix it.
void TraverseESEDatabase( wchar_t *database_filepath, unsigned long revision, unsigned long page_size )
{
	JET_RETRIEVECOLUMN rc = { 0 };

	unsigned long long thumbnail_cache_id = 0;

	// Initialize the Jet database session and get column information for later retrieval. SystemIndex_0A and SystemIndex_0P will be opened on success.
	if ( ( g_err = InitESEDBInfo( database_filepath, revision, page_size ) ) != JET_errSuccess || ( g_err = ( g_revision < 0x14 ? GetColumnInfo() : GetColumnInfoWin8() ) ) != JET_errSuccess ) { goto CLEANUP; }

	// These are set in get_column_info. Make sure they exist.
	if ( g_thumbnail_cache_id == NULL ) { if ( g_err == JET_errColumnNotFound ) { printf( "The System_ThumbnailCacheId column was not found.\n" ); } goto CLEANUP; }

	// Ensure that the values we retrieve are of the correct size.
	if ( g_thumbnail_cache_id->max_size != sizeof( unsigned long long ) ) { g_err = JET_errInvalidColumnType; goto CLEANUP; }

	// Set up the column info we want to retrieve.
	rc.columnid = g_thumbnail_cache_id->column_id;
	rc.pvData = ( void * )&thumbnail_cache_id;
	rc.cbData = g_thumbnail_cache_id->max_size;
	rc.itagSequence = 1;

	if ( ( g_err = JetMove( g_sesid, g_tableid_0A, JET_MoveFirst, JET_bitNil ) ) != JET_errSuccess ) { goto CLEANUP; }

	// Windows 8+ (database revision >= 0x14) doesn't use compression.
	// mssrch.dll is for Windows 7. msscb.dll is for Windows Vista. Load whichever one we can.
	// These dlls will allow us to uncompress certain column's data/text.
	if ( g_revision < 0x14 && mssrch_state == MSSRCH_STATE_SHUTDOWN && msscb_state == MSSRCH_STATE_SHUTDOWN )
	{
		// We only need one to load successfully.
		if ( !InitializeMsSrch() && !InitializeMsSCB() )
		{
			printf( "The modules mssrch.dll and msscb.dll failed to load.\nCompressed data will not be read.\n" );
		}
	}

	// Initialize g_rc_array to hold all of the record information we'll retrieve.
	BuildRetrieveColumnArray();

	// Create the file info tree if it doesn't exist.
	if ( g_file_info_tree == NULL )
	{
		g_file_info_tree = dllrbt_create( dllrbt_compare );
	}

	for ( unsigned int index = 0; ; ++index )
	{
		// Retrieve the column value.
		if ( ( g_err = JetRetrieveColumns( g_sesid, g_tableid_0A, &rc, 1 ) ) != JET_errSuccess )
		{
			break;
		}

		// Swap the byte order of the hash. For XP and 7
		if ( g_use_big_endian )
		{
			thumbnail_cache_id = ntohll( thumbnail_cache_id );
		}

		// Create the node to insert into a linked list.
		LINKED_LIST *fi_node = ( LINKED_LIST * )malloc( sizeof( LINKED_LIST ) );
		fi_node->fi.entry_hash = thumbnail_cache_id;
		fi_node->fi.index = index;	// Row index.
		fi_node->fi.ei = NULL;
		fi_node->next = NULL;

		if ( dllrbt_insert( g_file_info_tree, ( void * )fi_node->fi.entry_hash, fi_node ) != DLLRBT_STATUS_OK )
		{
			free( fi_node );
		}

		// Move to the next record (column row).
		if ( JetMove( g_sesid, g_tableid_0A, JET_MoveNext, JET_bitNil ) != JET_errSuccess )
		{
			break;
		}
	}

CLEANUP:

	// Process any error that occurred.
	HandleESEDBError();
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
					g_database_type = 2;

					TraverseSQLiteDatabase( database_filepath );
				}
				else
				{
					printf( "The module sqlite3.dll failed to load.\nThe DLL can be downloaded from: www.sqlite.org\n" );
				}
			}
			else if ( memcmp( partial_header + 4, "\xEF\xCD\xAB\x89", sizeof( unsigned long ) ) == 0 )	// Make sure we got enough of the header and it has the magic identifier (0x89ABCDEF) for an ESE database.
			{
				unsigned long revision = 0, page_size = 0;

				memcpy_s( &revision, sizeof( unsigned long ), partial_header + 0xE8, sizeof( unsigned long ) );		// Revision number
				memcpy_s( &page_size, sizeof( unsigned long ), partial_header + 0xEC, sizeof( unsigned long ) );	// Page size

				g_database_type = 1;

				TraverseESEDatabase( database_filepath, revision, page_size );
			}
			else
			{
				printf( "The selected file is not an ESE or SQLite database.\n" );
			}
		}
		else
		{
			printf( "The selected file could not be read or is an invalid database.\n" );
		}
	}
	else
	{
		printf( "The selected file could not be opened.\n" );
	}
}

void MapHash( unsigned long long hash, bool output_html, HANDLE hFile_html )
{
	FILE_INFO fi;
	EXTENDED_INFO *ei = NULL;

	if ( g_database_type == 1 )	// ESE Database
	{
		LINKED_LIST *ll = ( LINKED_LIST * )dllrbt_find( g_file_info_tree, ( void * )hash, true );
		if ( ll != NULL )
		{
			if ( ( g_err = JetMove( g_sesid, g_tableid_0A, JET_MoveFirst, JET_bitNil ) ) == JET_errSuccess )
			{
				if ( ( g_err = JetMove( g_sesid, g_tableid_0A, ll->fi.index, JET_bitNil ) ) == JET_errSuccess )
				{
					// Retrieve all the records associated with the matching System_ThumbnailCacheId.
					if ( ( g_err = JetRetrieveColumns( g_sesid, g_tableid_0A, g_rc_array, g_column_count ) ) == JET_errSuccess )
					{
						ConvertValues( &ei );
					}
				}
			}
		}
	}
	else if ( g_database_type == 2 )	// SQLite Database
	{
		// Hex value must be padded.
		sprintf_s( g_query + 288, 512 - 288, "%016llx\') )", ntohll( hash ) );

		fi.ei = NULL;
		fi.entry_hash = hash;
		//fi.index = 0;	// Not used.

		// Get all the values associated with the hash.
		int sql_rc = sqlite3_exec( g_sql_db, g_query, CreatePropertyInfoCallback, ( void * )&fi, NULL/*&sql_err_msg*/ );
		if ( sql_rc != SQLITE_OK )
		{
			if ( fi.ei != NULL )
			{
				free( fi.ei->property_value );
				free( fi.ei );
				fi.ei = NULL;
			}
		}

		ei = fi.ei;
	}

	if ( ei != NULL )
	{
		char buf[ 128 ];
		int write_size = 0;
		DWORD written = 0;

		printf( "---------------------------------------------\n" );
		printf( "Mapped Windows Search Information\n" );
		printf( "---------------------------------------------\n" );

		if ( output_html )
		{
			write_size = sprintf_s( buf, 128, "<tr><td></td><td colspan=\"9\">Mapped Windows Search information for: %016llx</td></tr>", hash );
			WriteFile( hFile_html, buf, write_size, &written, NULL );
		}

		int mode = _setmode( _fileno( stdout ), _O_U16TEXT );	// For Unicode output.

		while ( ei != NULL )
		{
			EXTENDED_INFO *del_ei = ei;

			if ( ei->property_value != NULL && ei->si != NULL )
			{
				wchar_t *property_name;
				if ( g_database_type == 1 )
				{
					property_name = ( ( COLUMN_INFO * )ei->si )->Name;
					wprintf( L"%.*s: %s\n", ( ( COLUMN_INFO * )ei->si )->Name_byte_length, property_name, ei->property_value );
				}
				else// ( g_database_type == 2 )
				{
					property_name = ( ( SHARED_EXTENDED_INFO * )ei->si )->windows_property;
					wprintf( L"%s: %s\n", property_name, ei->property_value );
				}

				if ( output_html )
				{
					int nlength = WideCharToMultiByte( CP_UTF8, 0, property_name, -1, NULL, 0, NULL, NULL );
					char *name = ( char * )malloc( sizeof( char ) * nlength ); // Size includes the null character.
					nlength = WideCharToMultiByte( CP_UTF8, 0, property_name, -1, name, nlength, NULL, NULL ) - 1;

					int vlength = WideCharToMultiByte( CP_UTF8, 0, ei->property_value, -1, NULL, 0, NULL, NULL );
					char *val = ( char * )malloc( sizeof( char ) * vlength ); // Size includes the null character.
					vlength = WideCharToMultiByte( CP_UTF8, 0, ei->property_value, -1, val, vlength, NULL, NULL ) - 1;

					WriteFile( hFile_html, "<tr><td></td><td>", 17, &written, NULL );
					WriteFile( hFile_html, name, nlength, &written, NULL );
					WriteFile( hFile_html, "</td><td colspan=\"8\"><pre>", 26, &written, NULL );
					WriteFile( hFile_html, val, vlength, &written, NULL );
					WriteFile( hFile_html, "</pre></td></tr>", 16, &written, NULL );

					free( name );
					free( val );
				}
			}

			ei = ei->next;

			free( del_ei->property_value );
			free( del_ei );
		}

		_setmode( _fileno( stdout ), mode );	// Reset.
	}
}
