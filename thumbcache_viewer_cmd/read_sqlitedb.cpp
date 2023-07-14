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

#include "lite_sqlite3.h"

#include "read_sqlitedb.h"
#include "utilities.h"
#include "dllrbt.h"

#include <stdlib.h>
#include <stdio.h>

void *g_sql_db = NULL;
dllrbt_tree *g_property_tree = NULL;

int dllrbt_property_compare( void *a, void *b )
{
	unsigned long id1 = ( unsigned long )a;
	unsigned long id2 = ( unsigned long )b;

	if ( id1 > id2 )
	{
		return 1;
	}

	if ( id1 < id2 )
	{
		return -1;
	}

	return 0;
}

void CleanupSQLiteInfo()
{
	sqlite3_close( g_sql_db );
	g_sql_db = NULL;

	// Free the values of the property tree.
	node_type *node = dllrbt_get_head( g_property_tree );
	while ( node != NULL )
	{
		// Free the linked list if there is one.
		PROPERTY_INFO *pi = ( PROPERTY_INFO * )node->val;

		node = node->next;

		if ( pi != NULL )
		{
			if ( pi->sei != NULL  )
			{
				free( pi->sei->windows_property );
				free( pi->sei );
			}
			free( pi );
		}
	}

	// Clean up our property tree.
	dllrbt_delete_recursively( g_property_tree );
	g_property_tree = NULL;
}

int GetLengthCallback( void *arg, int argc, char **argv, char ** /*azColName*/ )
{
	unsigned long *length = ( unsigned long * )arg;

	if ( length != NULL && argc == 1 )
	{
		*length = strtoul( argv[ 0 ], NULL, 10 );
	}

	return 0;
}
int WINAPI	s_GetLengthCallback( void *arg, int argc, char **argv, char **azColName ) { return GetLengthCallback( arg, argc, argv, azColName ); }
int WINAPIV	c_GetLengthCallback( void *arg, int argc, char **argv, char **azColName ) { return GetLengthCallback( arg, argc, argv, azColName ); }

// argv[ 0 ] = Id
// argv[ 1 ] = UniqueKey
int BuildPropertyTreeCallback( void * /*arg*/, int argc, char **argv, char ** /*azColName*/ )
{
	if ( argc == 2 )
	{
		// Windows 8+ columns have a hex number followed by a '-' separating the Windows Property.
		char *prop_name = strchr( argv[ 1 ], '-' );
		if ( prop_name == NULL )
		{
			prop_name = argv[ 1 ];
		}
		else
		{
			++prop_name;
		}

		SHARED_EXTENDED_INFO *sei = ( SHARED_EXTENDED_INFO * )malloc( sizeof( SHARED_EXTENDED_INFO ) );

		int property_name_length = MultiByteToWideChar( CP_UTF8, 0, prop_name, -1, NULL, 0 );	// Include the NULL terminator.
		sei->windows_property = ( wchar_t * )malloc( sizeof( wchar_t ) * property_name_length );
		property_name_length = MultiByteToWideChar( CP_UTF8, 0, prop_name, -1, sei->windows_property, property_name_length ) - 1;

		// Create the file info tree if it doesn't exist.
		if ( g_property_tree == NULL )
		{
			g_property_tree = dllrbt_create( dllrbt_property_compare );
		}

		PROPERTY_INFO *pi = ( PROPERTY_INFO * )malloc( sizeof( PROPERTY_INFO ) );
		pi->sei = sei;
		pi->id = ( argv[ 0 ] != NULL ? strtoul( argv[ 0 ], NULL, 10 ) : 0 );
		pi->property_name_length = property_name_length;

		if ( dllrbt_insert( g_property_tree, ( void * )pi->id, pi ) != DLLRBT_STATUS_OK )
		{
			free( pi->sei->windows_property );
			free( pi->sei );
			free( pi );
		}
	}

	return 0;
}
int WINAPI	s_BuildPropertyTreeCallback( void *arg, int argc, char **argv, char **azColName ) { return BuildPropertyTreeCallback( arg, argc, argv, azColName ); }
int WINAPIV	c_BuildPropertyTreeCallback( void *arg, int argc, char **argv, char **azColName ) { return BuildPropertyTreeCallback( arg, argc, argv, azColName ); }

/*
argv[ 0 ] = WorkId
argv[ 1 ] = Id
argv[ 2 ] = UniqueKey
argv[ 3 ] = Value
argv[ 4 ] = VariantType
*/
int CreatePropertyInfoCallback( void *arg, int argc, char **argv, char ** /*azColName*/ )
{
	FILE_INFO *fi = ( FILE_INFO * )arg;
	if ( fi != NULL && argc == 5 )
	{
		SHARED_EXTENDED_INFO *sei = NULL;

		unsigned long Id_num = ( argv[ 1 ] != NULL ? strtoul( argv[ 1 ], NULL, 10 ) : 0 );
		PROPERTY_INFO *pi = ( PROPERTY_INFO * )dllrbt_find( g_property_tree, ( void * )Id_num, true );
		if ( pi != NULL && pi->sei != NULL )
		{
			sei = pi->sei;
		}
		else
		{
			return 0;
		}

		EXTENDED_INFO *ei = ( EXTENDED_INFO * )malloc( sizeof( EXTENDED_INFO ) );
		ei->si = ( void * )sei;
		ei->property_value = NULL;
		ei->next = NULL;

		// Windows 8+ columns have a hex number followed by a '-' separating the Windows Property.
		char *prop_name = strchr( argv[ 2 ], '-' );
		if ( prop_name == NULL )
		{
			prop_name = argv[ 2 ];
		}
		else
		{
			++prop_name;
		}

		VARENUM Type = ( VARENUM )( argv[ 4 ] != NULL ? strtoul( argv[ 4 ], NULL, 10 ) : 0 );
		char *val = argv[ 3 ];

		char *WorkId = argv[ 0 ];
		char *Id = argv[ 1 ];	// Property type numeric value.

		int buf_count = 0;

		switch ( Type )
		{
			case VT_BOOL:
			{
				buf_count = ( *val != '0' ? 4 : 5 );	// "true" or "false"
				ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 1 ) );
				wcscpy_s( ei->property_value, buf_count + 1, ( *val != '0' ? L"true" : L"false" ) );
			}
			break;

			case VT_LPWSTR:
			{
				int val_length = MultiByteToWideChar( CP_UTF8, 0, val, -1, NULL, 0 );	// Include the NULL terminator.
				ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * val_length );
				MultiByteToWideChar( CP_UTF8, 0, val, -1, ei->property_value, val_length );
			}
			break;

			case VT_R8:
			{
				int val_length = MultiByteToWideChar( CP_UTF8, 0, val, -1, NULL, 0 );	// Include the NULL terminator.
				ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * val_length );
				MultiByteToWideChar( CP_UTF8, 0, val, -1, ei->property_value, val_length );
			}
			break;

			case VT_FILETIME:
			{
				unsigned long long time = 0;
				memcpy_s( &time, sizeof( unsigned long long ), val, sizeof( unsigned long long ) );

				SYSTEMTIME st;
				FILETIME ft;
				ft.dwLowDateTime = ( DWORD )time;
				ft.dwHighDateTime = ( DWORD )( time >> 32 );
				FileTimeToSystemTime( &ft, &st );

				buf_count = _scwprintf( L"%d/%d/%d (%02d:%02d:%02d.%d) [UTC]", st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );
				if ( buf_count > 0 )
				{
					ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 1 ) );
					swprintf_s( ei->property_value, buf_count + 1, L"%d/%d/%d (%02d:%02d:%02d.%d) [UTC]", st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );
				}
			}
			break;

			case VT_UI4:
			{
				unsigned long num_val = strtoul( val, NULL, 10 );

				if ( pi->property_name_length == 21 && wcscmp( sei->windows_property, L"System_FileAttributes" ) == 0 )
				{
					ei->property_value = GetFileAttributesStr( num_val );
				}
				else if ( ( pi->property_name_length == 17 && wcscmp( sei->windows_property, L"System_SFGAOFlags" ) == 0 ) ||
						  ( pi->property_name_length == 28 && wcscmp( sei->windows_property, L"System_Link_TargetSFGAOFlags" ) == 0 ) )
				{
					ei->property_value = GetSFGAOStr( num_val );
				}
				else
				{
					int val_length = MultiByteToWideChar( CP_UTF8, 0, val, -1, NULL, 0 );	// Include the NULL terminator.
					ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * val_length );
					MultiByteToWideChar( CP_UTF8, 0, val, -1, ei->property_value, val_length );
				}
			}
			break;

			case VT_UI8:
			{
				unsigned long long ui8_val = 0;
				memcpy_s( &ui8_val, sizeof( unsigned long long ), val, sizeof( unsigned long long ) );

				if ( pi->property_name_length == 23 && wcscmp( sei->windows_property, L"System_ThumbnailCacheId" ) == 0 )
				{
					ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * 17 );
					swprintf_s( ei->property_value, 17, L"%016llx", ui8_val );
				}
				else if ( pi->property_name_length == 11 && wcscmp( sei->windows_property, L"System_Size" ) == 0 )
				{
					ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( 15 ) );
					swprintf_s( ei->property_value, 15, L"%llu bytes", ui8_val );
				}
				else
				{
					ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( 9 ) );
					swprintf_s( ei->property_value, 9, L"%llu", ui8_val );
				}
			}
			break;

			case VT_CLSID:
			{
				// Output GUID formatted value.
				unsigned long val_1 = 0;
				unsigned short val_2 = 0, val_3 = 0;

				memcpy_s( &val_1, sizeof( unsigned long ), val, sizeof( unsigned long ) );
				memcpy_s( &val_2, sizeof( unsigned short ), val + sizeof( unsigned long ), sizeof( unsigned short ) );
				memcpy_s( &val_3, sizeof( unsigned short ), val + sizeof( unsigned long ) + sizeof( unsigned short ), sizeof( unsigned short ) );

				buf_count = ( 32 + 6 + 1 );
				ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * buf_count );

				unsigned long property_value_offset = swprintf_s( ei->property_value, buf_count, L"{%08x-%04x-%04x-", val_1, val_2, val_3 );
				for ( unsigned long h = sizeof( unsigned long ) + ( sizeof( unsigned short ) * 2 ); h < 16; ++h )
				{
					if ( h == 10 )
					{
						ei->property_value[ property_value_offset ] = L'-';
						++property_value_offset;
					}
					property_value_offset += swprintf_s( ei->property_value + property_value_offset, buf_count - property_value_offset, L"%02x", ( ( unsigned char * )val )[ h ] );
				}
				ei->property_value[ buf_count - 2 ] = L'}';
				ei->property_value[ buf_count - 1 ] = 0;	// Sanity.
			}
			break;

			case VT_BLOB:
			{
				if ( pi->property_name_length == 11 && wcscmp( sei->windows_property, L"System_Kind" ) == 0 )
				{
					unsigned long length = 0;

					char query[ 256 ];
					sprintf_s( query, 256, "SELECT length(Value) FROM SystemIndex_1_PropertyStore WHERE WorkId = %s AND ColumnId = %s", WorkId, Id );
					int sql_rc = sqlite3_exec( g_sql_db, query, GetLengthCallback, ( void * )&length, NULL/*&sql_err_msg*/ );
					if ( sql_rc == SQLITE_OK )
					{
						ei->property_value = ( wchar_t * )malloc( sizeof( char ) * ( length + sizeof( wchar_t ) ) );	// Include the NULL terminator.
						memcpy_s( ei->property_value, sizeof( char ) * ( length + sizeof( wchar_t ) ), val, length );

						unsigned long property_value_size = length / sizeof( wchar_t );
						ei->property_value[ property_value_size ] = 0;	// Sanity.

						// See if we have any string arrays.
						if ( wcslen( ei->property_value ) < property_value_size )
						{
							// Replace the NULL character at the end of each string (except the last) with a ';' separator.
							wchar_t *t_val = ei->property_value;
							while ( t_val < ( ei->property_value + property_value_size ) )
							{
								if ( *t_val == 0 )
								{
									*t_val = L';';
								}

								++t_val;
							}
						}
					}
					/*else
					{
						sqlite3_free( sql_err_msg );
					}*/

					break;
				}
				/*else if ( ( pi->property_name_length == 16 && wcscmp( sei->windows_property, L"InvertedOnlyPids" ) == 0 ) ||
						  ( pi->property_name_length == 15 && wcscmp( sei->windows_property, L"InvertedOnlyMD5" ) == 0 ) )
				{
					// Output hex values.
					unsigned long property_value_offset = 0;

					buf_count = ( length * 2 ) + 1;
					ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * buf_count );
					for ( unsigned long h = 0; h < length; ++h )
					{
						property_value_offset += swprintf_s( ei->property_value + property_value_offset, buf_count - property_value_offset, L"%02x", ( ( unsigned char * )val )[ h ] );
					}
				}*/

				// Fall through for everything else and output hex values.
			}

			default:
			{
				unsigned long length = 0;

				char query[ 256 ];
				sprintf_s( query, 256, "SELECT length(Value) FROM SystemIndex_1_PropertyStore WHERE WorkId = %s AND ColumnId = %s", WorkId, Id );
				int sql_rc = sqlite3_exec( g_sql_db, query, GetLengthCallback, ( void * )&length, NULL/*&sql_err_msg*/ );
				if ( sql_rc == SQLITE_OK )
				{
					// Output hex values.
					unsigned long property_value_offset = 0;

					buf_count = ( length * 2 ) + 1;
					ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * buf_count );
					for ( unsigned long h = 0; h < length; ++h )
					{
						property_value_offset += swprintf_s( ei->property_value + property_value_offset, buf_count - property_value_offset, L"%02x", ( ( unsigned char * )val )[ h ] );
					}
				}
				/*else
				{
					sqlite3_free( sql_err_msg );
				}*/
			}
			break;
		}

		if ( fi->ei != NULL )
		{
			ei->next = fi->ei;
		}

		fi->ei = ei;
	}
	return 0;
}
int WINAPI	s_CreatePropertyInfoCallback( void *arg, int argc, char **argv, char **azColName ) { return CreatePropertyInfoCallback( arg, argc, argv, azColName ); }
int WINAPIV	c_CreatePropertyInfoCallback( void *arg, int argc, char **argv, char **azColName ) { return CreatePropertyInfoCallback( arg, argc, argv, azColName ); }
