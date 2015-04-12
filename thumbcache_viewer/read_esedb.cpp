/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2015 Eric Kutcher

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
#include "utilities.h"
#include "read_esedb.h"

#include "lite_mssrch.h"
#include "lite_msscb.h"

#include <stdio.h>

#define JET_coltypUnsignedLong		14
#define JET_coltypLongLong			15
#define JET_coltypGUID				16
#define JET_coltypUnsignedShort		17

// Internal variables. All all freed/reset in cleanup_esedb_info.
JET_ERR g_err = JET_errSuccess;
JET_INSTANCE g_instance = JET_instanceNil;
JET_SESID g_sesid = JET_sesidNil;
JET_DBID g_dbid = JET_bitNil;
JET_TABLEID g_tableid_0P = JET_tableidNil, g_tableid_0A = JET_tableidNil;
JET_RETRIEVECOLUMN *g_rc_array = NULL;
unsigned long g_column_count = 0;
char *g_ascii_filepath = NULL;
column_info *g_ci = NULL;

column_info *g_item_path_display = NULL;
column_info *g_file_attributes = NULL;
column_info *g_thumbnail_cache_id = NULL;
column_info *g_file_extension = NULL;

bool g_use_big_endian = true;
unsigned long g_revision = 0;

// Internal error states.
int g_error_offset = 0;
char g_error[ ERROR_BUFFER_SIZE ] = { 0 };
unsigned char g_error_state = 0;

wchar_t *uncompress_value( unsigned char *value, unsigned long value_length )
{
	wchar_t *ret_value = NULL;

	if ( ( mssrch_state != MSSRCH_STATE_SHUTDOWN || msscb_state != MSSRCH_STATE_SHUTDOWN ) && value != NULL )
	{
		int uncompressed_byte_length = MSSUncompressText( value, value_length, NULL, 0 );
		if ( uncompressed_byte_length > 0 )
		{
			ret_value = ( wchar_t * )malloc( sizeof( char ) * ( uncompressed_byte_length + sizeof( wchar_t ) ) );	// Include the NULL terminator.
			memset( ret_value, 0, sizeof( char ) * ( uncompressed_byte_length + sizeof( wchar_t ) ) );

			MSSUncompressText( value, value_length, ret_value, uncompressed_byte_length );
			ret_value[ uncompressed_byte_length / sizeof( wchar_t ) ] = 0;	// Sanity.
		}
	}

	return ret_value;
}

void convert_values( extended_info **ei )
{
	extended_info *l_ei = *ei;
	column_info *t_ci = g_ci;

	int buf_count = 0;

	wchar_t *format = NULL;

	for ( unsigned long i = 0; i < g_column_count; ++i )
	{
		// Stop processing and exit the thread.
		if ( g_kill_scan == true )
		{
			break;
		}

		if ( t_ci != NULL )
		{
			// Create a shared extended info structure to share the Windows Property names across entries.
			if ( t_ci->sei == NULL )
			{
				t_ci->sei = ( shared_extended_info * )malloc( sizeof( shared_extended_info ) );
				t_ci->sei->count = 0;

				t_ci->sei->windows_property = ( wchar_t * )malloc( sizeof( char ) * ( t_ci->Name_byte_length + sizeof( wchar_t ) ) );
				memcpy_s( t_ci->sei->windows_property, sizeof( char ) * ( t_ci->Name_byte_length + sizeof( wchar_t ) ), t_ci->Name, t_ci->Name_byte_length );

				t_ci->sei->windows_property[ t_ci->Name_byte_length / sizeof( wchar_t ) ] = 0;	// Sanity.
			}

			++( t_ci->sei->count );

			extended_info *t_ei = ( extended_info * )malloc( sizeof( extended_info ) );
			t_ei->sei = t_ci->sei;
			t_ei->property_value = NULL;
			t_ei->next = NULL;

			// See if the property value was retrieved.
			if ( g_rc_array[ i ].cbActual != 0 && g_rc_array[ i ].cbActual <= t_ci->max_size )
			{
				switch ( t_ci->column_type )
				{
					case JET_coltypBit:
					{
						// Handles Type(s): VT_BOOL

						if ( t_ci->Type == VT_BOOL )	// bool (true == -1, false == 0)
						{
							buf_count = ( t_ci->data[ 0 ] != 0 ? 4 : 5 );	// "true" or "false"
							t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 1 ) );
							wcscpy_s( t_ei->property_value, buf_count + 1, ( t_ci->data[ 0 ] != 0 ? L"true" : L"false" ) );

							break;
						}

						// For anything else, fall through to JET_coltypUnsignedByte
					}

					case JET_coltypUnsignedByte:
					{
						// Handles Type(s): VT_I1, VT_UI1, anything else from JET_coltypBit

						format = ( t_ci->Type == VT_I1 ? L"%d" : L"%lu" );
						buf_count = _scwprintf( format, t_ci->data[ 0 ] );
						if ( buf_count > 0 )
						{
							t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 1 ) );
							swprintf_s( t_ei->property_value, buf_count + 1, format, t_ci->data[ 0 ] );
						}
					}
					break;

					case JET_coltypShort:
					case JET_coltypUnsignedShort:
					{
						// Handles Type(s): VT_UI2

						unsigned short val = 0;
						memcpy_s( &val, sizeof( unsigned short ), t_ci->data, sizeof( unsigned short ) );

						format = ( t_ci->column_type == JET_coltypShort ? L"%d" : L"%lu" );
						buf_count = _scwprintf( format, val );
						if ( buf_count > 0 )
						{
							t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 1 ) );
							swprintf_s( t_ei->property_value, buf_count + 1, format, val );
						}
					}
					break;

					case JET_coltypLong:
					case JET_coltypUnsignedLong:
					{
						// Handles Type(s): VT_I4, VT_UI4

						unsigned long val = 0;
						memcpy_s( &val, sizeof( unsigned long ), t_ci->data, sizeof( unsigned long ) );

						if ( t_ci->Name_byte_length == 42 && wcscmp( t_ci->Name, L"System_FileAttributes" ) == 0 )
						{
							t_ei->property_value = get_file_attributes( val );
						}
						else if ( ( t_ci->Name_byte_length == 34 && wcscmp( t_ci->Name, L"System_SFGAOFlags" ) == 0 ) ||
								  ( t_ci->Name_byte_length == 56 && wcscmp( t_ci->Name, L"System_Link_TargetSFGAOFlags" ) == 0 ) )
						{
							t_ei->property_value = get_sfgao( val );
						}
						else
						{
							format = ( t_ci->column_type == JET_coltypLong ? L"%d" : L"%lu" );
							buf_count = _scwprintf( format, val );
							if ( buf_count > 0 )
							{
								t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 1 ) );
								swprintf_s( t_ei->property_value, buf_count + 1, format, val );
							}
						}
					}
					break;

					case JET_coltypIEEEDouble:
					{
						// Handles Type(s): VT_R8

						double val = 0.0f;
						memcpy_s( &val, sizeof( double ), t_ci->data, sizeof( double ) );

						buf_count = _scwprintf( L"%f", val );
						if ( buf_count > 0 )
						{
							t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 1 ) );
							swprintf_s( t_ei->property_value, buf_count + 1, L"%f", val );
						}
					}
					break;

					case JET_coltypCurrency:
					case JET_coltypBinary:		// May be compressed. We'll fall through to JET_coltypLongBinary to handle uncompressing it.
					case JET_coltypLongLong:
					{
						// Handles Type(s): VT_FILETIME, VT_UI8, VT_LPWSTR

						unsigned long long val = 0;
						memcpy_s( &val, sizeof( unsigned long long ), t_ci->data, sizeof( unsigned long long ) );

						if ( g_use_big_endian == true )
						{
							val = ntohll( val );
						}

						if ( t_ci->Type == VT_FILETIME )	// FILETIME
						{
							SYSTEMTIME st;
							FILETIME ft;
							ft.dwLowDateTime = ( DWORD )val;
							ft.dwHighDateTime = ( DWORD )( val >> 32 );
							FileTimeToSystemTime( &ft, &st );

							buf_count = _scwprintf( L"%d/%d/%d (%02d:%02d:%02d.%d) [UTC]", st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );
							if ( buf_count > 0 )
							{
								t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 1 ) );
								swprintf_s( t_ei->property_value, buf_count + 1, L"%d/%d/%d (%02d:%02d:%02d.%d) [UTC]", st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );
							}

							break;
						}
						else
						{
							if ( t_ci->Name_byte_length == 22 && wcscmp( t_ci->Name, L"System_Size" ) == 0 )
							{
								buf_count = _scwprintf( L"%llu", val );
								if ( buf_count > 0 )
								{
									t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 6 + 1 ) );
									swprintf_s( t_ei->property_value, buf_count + 6 + 1, L"%llu bytes", val );
								}

								break;
							}
							else if ( t_ci->Name_byte_length == 46 && wcscmp( t_ci->Name, L"System_ThumbnailCacheId" ) == 0 )
							{
								t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * 17 );
								swprintf_s( t_ei->property_value, 17, L"%016llx", val );

								break;
							}
							else if ( t_ci->Name_byte_length == 30 && wcscmp( t_ci->Name, L"InvertedOnlyMD5" ) == 0 )
							{
								// Output hex values.
								unsigned long property_value_offset = 0;
								buf_count = ( ( g_rc_array[ i ].cbActual * 2 ) + 1 );
								t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * buf_count );
								for ( unsigned long h = 0; h < g_rc_array[ i ].cbActual; ++h )
								{
									property_value_offset += swprintf_s( t_ei->property_value + property_value_offset, buf_count - property_value_offset, L"%02x", t_ci->data[ h ] );
								}

								break;
							}
							else
							{
								if ( t_ci->JetCompress == false )
								{
									// Handle 8 byte values. For everything else, fall through.
									if ( g_rc_array[ i ].cbActual == sizeof( unsigned long long ) )
									{
										format = ( t_ci->column_type == JET_coltypLongLong ? L"%lld" : L"%llu" );
										buf_count = _scwprintf( format, val );
										if ( buf_count > 0 )
										{
											t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * ( buf_count + 1 ) );
											swprintf_s( t_ei->property_value, buf_count + 1, format, val );
										}

										break;
									}
								}

								// Fall through to JET_coltypLongBinary.
							}
						}
					}

					case JET_coltypText:
					case JET_coltypLongBinary:
					case JET_coltypLongText:
					case JET_coltypGUID:
					{
						// Handles Type(s): VT_NULL, VT_LPWSTR, ( VT_VECTOR | VT_LPWSTR ), VT_BLOB, anything else from JET_coltypBinary

						if ( t_ci->column_type == JET_coltypGUID )
						{
							if ( g_rc_array[ i ].cbActual == 16 )
							{
								// Output GUID formatted value.
								unsigned long val_1 = 0;
								unsigned short val_2 = 0, val_3 = 0;

								memcpy_s( &val_1, sizeof( unsigned long ), t_ci->data, sizeof( unsigned long ) );
								memcpy_s( &val_2, sizeof( unsigned short ), t_ci->data + sizeof( unsigned long ), sizeof( unsigned short ) );
								memcpy_s( &val_3, sizeof( unsigned short ), t_ci->data + sizeof( unsigned long ) + sizeof( unsigned short ), sizeof( unsigned short ) );

								buf_count = ( ( g_rc_array[ i ].cbActual * 2 ) + 6 + 1 );
								t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * buf_count );

								unsigned long property_value_offset = swprintf_s( t_ei->property_value, buf_count, L"{%04x-%02x-%02x-", val_1, val_2, val_3 );
								for ( unsigned long h = sizeof( unsigned long ) + ( sizeof( unsigned short ) * 2 ); h < g_rc_array[ i ].cbActual; ++h )
								{
									if ( h == 10 )
									{
										t_ei->property_value[ property_value_offset ] = L'-';
										++property_value_offset;
									}
									property_value_offset += swprintf_s( t_ei->property_value + property_value_offset, buf_count - property_value_offset, L"%02x", t_ci->data[ h ] );
								}
								t_ei->property_value[ buf_count - 2 ] = L'}';
								t_ei->property_value[ buf_count - 1 ] = 0;	// Sanity.

								break;
							}

							// Fall through to default.
						}
						else
						{
							// On Vista, if Type == ( VT_VECTOR | VT_LPWSTR ), then the first 2 bytes (little-endian) = array count?
							if ( t_ci->JetCompress == true )
							{
								// Make a copy first because we may need to reuse t_ci->data and don't want it modified.
								unsigned char *data_copy = ( unsigned char * )malloc( sizeof( unsigned char ) * g_rc_array[ i ].cbActual );
								memcpy_s( data_copy, sizeof( unsigned char ) * g_rc_array[ i ].cbActual, t_ci->data, g_rc_array[ i ].cbActual );

								t_ei->property_value = uncompress_value( data_copy, g_rc_array[ i ].cbActual );

								free( data_copy );

								break;
							}
							else
							{
								if ( t_ci->Name_byte_length == 32 && wcscmp( t_ci->Name, L"InvertedOnlyPids" ) == 0 )
								{
									// Output hex values.
									unsigned long property_value_offset = 0;
									buf_count = ( ( g_rc_array[ i ].cbActual * 2 ) + 1 );
									t_ei->property_value = ( wchar_t * )malloc( sizeof( wchar_t ) * buf_count );
									for ( unsigned long h = 0; h < g_rc_array[ i ].cbActual; ++h )
									{
										property_value_offset += swprintf_s( t_ei->property_value + property_value_offset, buf_count - property_value_offset, L"%02x", t_ci->data[ h ] );
									}

									break;
								}

								// Fall through to default.
							}
						}
					}

					default:
					{
						// This is usually wchar strings.

						t_ei->property_value = ( wchar_t * )malloc( sizeof( char ) * ( g_rc_array[ i ].cbActual + sizeof( wchar_t ) ) );	// Include the NULL terminator.
						memcpy_s( t_ei->property_value, sizeof( char ) * ( g_rc_array[ i ].cbActual + sizeof( wchar_t ) ), t_ci->data, g_rc_array[ i ].cbActual );

						unsigned long property_value_size = g_rc_array[ i ].cbActual / sizeof( wchar_t );
						t_ei->property_value[ property_value_size ] = 0;	// Sanity.

						// See if we have any string arrays.
						if ( wcslen( t_ei->property_value ) < property_value_size )
						{
							// Replace the NULL character at the end of each string (except the last) with a ';' separator.
							wchar_t *t_val = t_ei->property_value;
							while ( t_val < ( t_ei->property_value + property_value_size ) )
							{
								if ( *t_val == 0 )
								{
									*t_val = L';';
								}

								++t_val;
							}
						}
					}
					break;
				}
			}

			if ( l_ei != NULL )
			{
				l_ei->next = t_ei;
			}
			else
			{
				*ei = t_ei;
			}

			l_ei = t_ei;

			// Go to the next Windows property.
			t_ci = t_ci->next;
		}
		else
		{
			break;
		}
	}
}

void cleanup_esedb_info()
{
	// Reverse the steps of initializing the Jet database engine, etc.
	if ( g_sesid != JET_sesidNil )
	{
		if ( g_tableid_0A != JET_tableidNil )
		{
			JetCloseTable( g_sesid, g_tableid_0A );
			g_tableid_0A = JET_tableidNil;
		}

		if ( g_tableid_0P != JET_tableidNil )
		{
			JetCloseTable( g_sesid, g_tableid_0P );
			g_tableid_0P = JET_tableidNil;
		}

		if ( g_error_state < 2 )
		{
			if ( g_error_state < 1 )
			{
				JetCloseDatabase( g_sesid, g_dbid, JET_bitNil );
				g_dbid = JET_bitNil;
			}

			JetDetachDatabase( g_sesid, g_ascii_filepath );	// If g_ascii_filepath is NULL, then all databases are detached from the session.
		}

		JetEndSession( g_sesid, JET_bitNil );
		g_sesid = JET_sesidNil;
	}

	if ( g_instance != JET_instanceNil )
	{
		JetTerm( g_instance );
		g_instance = JET_instanceNil;
	}

	g_error_state = 0;

	free( g_ascii_filepath );
	g_ascii_filepath = NULL;

	g_column_count = 0;

	g_use_big_endian = true;
	g_revision = 0;

	// Freed in the loop below.
	g_item_path_display = NULL;
	g_file_attributes = NULL;
	g_thumbnail_cache_id = NULL;
	g_file_extension = NULL;

	// Free the array, but not its contents. g_rc_array[n].pvData = g_cl->data (freed in the loop below).
	free( g_rc_array );
	g_rc_array = NULL;

	column_info *t_ci = g_ci;
	column_info *d_ci = NULL;
	while ( t_ci != NULL )
	{
		d_ci = t_ci;
		t_ci = t_ci->next;

		// d_ci->sei will be freed with the fileinfo structure in wnd_proc_main.
		free( d_ci->data );
		free( d_ci->Name );
		free( d_ci );
	}
	g_ci = NULL;
}

void set_error_message( char *msg )
{
	g_error_offset = sprintf_s( g_error, ERROR_BUFFER_SIZE, msg );
}

// Print out any error we've encountered.
void handle_esedb_error()
{
	if ( g_err != JET_errSuccess )
	{
		// Add two newlines if we set our message.
		if ( g_error_offset > 0 )
		{
			memcpy_s( g_error + g_error_offset, ERROR_BUFFER_SIZE - g_error_offset, "\r\n\r\n\0", 5 );
			g_error_offset += 4;
		}

		JET_API_PTR error_value = g_err;
		// It would be nice to know how big this buffer is supposed to be. It seems to silently fail if it's not big enough...thankfully.
		if ( JetGetSystemParameter( NULL, JET_sesidNil, JET_paramErrorToString, &error_value, g_error + g_error_offset, ERROR_BUFFER_SIZE - g_error_offset ) != JET_errBufferTooSmall )
		{
			char *search = strchr( g_error + g_error_offset, ',' );
			if ( search != NULL )
			{
				*search = ':';
			}
		}

		// Non blocking MessageBox.
		SendNotifyMessageA( g_hWnd_scan, WM_ALERT, 0, ( LPARAM )g_error );

		g_error_offset = 0;
	}
}

// g_ascii_filepath is freed in cleanup_esedb_info().
JET_ERR init_esedb_info( wchar_t *database_filepath )
{
	char *partial_header = NULL;
	DWORD read = 0;

	unsigned long cbPageSize = 0;

	if ( database_filepath == NULL )
	{
		g_err = JET_errDatabaseNotFound;
		goto CLEANUP;
	}

	// JetGetDatabaseFileInfo for Windows Vista doesn't seem to like weird page sizes so we'll get it manually.
	HANDLE hFile = CreateFile( database_filepath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		partial_header = ( char * )malloc( sizeof( char ) * 512 );

		ReadFile( hFile, partial_header, sizeof( char ) * 512, &read, NULL );

		CloseHandle( hFile );
	}

	// Make sure we got enough of the header and it has the magic identifier (0x89ABCDEF) for an ESE database.
	if ( read < 512 || partial_header == NULL || memcmp( partial_header + 4, "\xEF\xCD\xAB\x89", sizeof( unsigned long ) ) != 0 )
	{
		g_err = JET_errDatabaseCorrupted;
		goto CLEANUP;
	}

	memcpy_s( &g_revision, sizeof( unsigned long ), partial_header + 0xE8, sizeof( unsigned long ) );		// Revision number
	memcpy_s( &cbPageSize, sizeof( unsigned long ), partial_header + 0xEC, sizeof( unsigned long ) );	// Page size

	// Vista and 8+ use little-endian
	if ( g_revision == 0x0C || g_revision >= 0x14 )
	{
		g_use_big_endian = false;
	}

	// The following Jet functions don't have Unicode support on XP (our minimum compatibility) or below.
	// Putting the database in the root directory (along with a Unicode filename) would get around the issue.
	int filepath_length = WideCharToMultiByte( CP_ACP, 0, database_filepath, -1, NULL, 0, NULL, NULL );
	g_ascii_filepath = ( char * )malloc( sizeof( char ) * filepath_length ); // Size includes the null character.
	WideCharToMultiByte( CP_ACP, 0, database_filepath, -1, g_ascii_filepath, filepath_length, NULL, NULL );

	// Disable event logging.
	g_err = JetSetSystemParameter( NULL, JET_sesidNil, JET_paramNoInformationEvent, true, NULL ); if ( g_err != JET_errSuccess ) { goto CLEANUP; }
	// Don't generate recovery files.
	g_err = JetSetSystemParameter( NULL, JET_sesidNil, JET_paramRecovery, NULL, "Off" ); if ( g_err != JET_errSuccess ) { goto CLEANUP; }
	// Don't generate any temporary tables.
	g_err = JetSetSystemParameter( NULL, JET_sesidNil, JET_paramMaxTemporaryTables, 0, NULL ); if ( g_err != JET_errSuccess ) { goto CLEANUP; }

	// 2KB, 16KB, and 32KB page sizes were added to Windows 7 (0x11) and above.
	if ( ( g_err = JetSetSystemParameter( NULL, JET_sesidNil, JET_paramDatabasePageSize, cbPageSize, NULL ) ) != JET_errSuccess ) 
	{
		// The database engine doesn't like our page size. It's probably from Windows Vista (0x0C) or below.
		if ( g_err == JET_errInvalidParameter && g_revision >= 0x11 ) { set_error_message( "The Microsoft Jet database engine is not supported for this version of database.\r\n\r\nPlease run the program with esent.dll from Windows 7 or higher." ); }
		goto CLEANUP;
	}

	g_err = JetCreateInstance( &g_instance, PROGRAM_CAPTION_A ); if ( g_err != JET_errSuccess ) { goto CLEANUP; }
	g_err = JetInit( &g_instance ); if ( g_err != JET_errSuccess ) { goto CLEANUP; }
	g_err = JetBeginSession( g_instance, &g_sesid, 0, 0 ); if ( g_err != JET_errSuccess ) { goto CLEANUP; }

	if ( ( g_err = JetAttachDatabase( g_sesid, g_ascii_filepath, JET_bitDbReadOnly ) ) != JET_errSuccess )
	{
		g_error_state = 2;	// Don't detach database.
		if ( g_err == JET_errDatabaseDirtyShutdown ) { set_error_message( "Please run esentutl.exe to recover or repair the database." ); }
		else if ( g_err == JET_errDatabaseInvalidPath || g_err == JET_errDiskIO || g_err == JET_errInvalidPath || g_err == JET_errInvalidSystemPath ) { set_error_message( "The database could not be loaded from its current location.\r\n\r\nTry moving the database into the root directory and ensure that there are no Unicode characters in the path." ); }
		else if ( g_err == JET_errReadVerifyFailure ) { set_error_message( "The Microsoft Jet database engine may not be supported for this version of database.\r\n\r\nPlease run the program with esent.dll from Windows Vista or higher." ); }	// I see this with XP esent.dll.
		goto CLEANUP;
	}

	if ( ( g_err = JetOpenDatabase( g_sesid, g_ascii_filepath, NULL, &g_dbid, JET_bitDbReadOnly ) ) != JET_errSuccess )
	{
		g_error_state = 1;	// Don't close database.
		goto CLEANUP;
	}

CLEANUP:

	free( partial_header );

	return g_err;
}

// *ptableid is closed in cleanup_esedb_info().
JET_ERR open_table( JET_PCSTR szTableName, JET_TABLEID *ptableid )
{
	if ( ( g_err = JetOpenTable( g_sesid, g_dbid, szTableName, NULL, 0, JET_bitTableReadOnly, ptableid ) ) != JET_errSuccess )
	{
		if ( g_err == JET_errObjectNotFound ) { g_error_offset = sprintf_s( g_error, 1024, "The %s table was not found.", szTableName ); }
	}

	return g_err;
}

JET_ERR get_table_column_info( JET_TABLEID tableid, JET_PCSTR szColumnName, JET_COLUMNDEF *pcolumndef )
{
	if ( ( g_err = JetGetTableColumnInfo( g_sesid, tableid, szColumnName, pcolumndef, sizeof( *pcolumndef ), JET_ColInfo ) ) != JET_errSuccess )
	{
		if ( g_err == JET_errColumnNotFound ) { g_error_offset = sprintf_s( g_error, 1024, "The %s column was not found.", szColumnName ); }
	}

	return g_err;
}

// Convert the Windows Property to its column name. (Replace '_' with '.')
char *get_column_name( wchar_t *name, unsigned long name_length )
{
	char *column_name = NULL;
	wchar_t *t_cn = NULL;

	t_cn = name;
	while ( t_cn != NULL && *t_cn != L'\0' )
	{
		if ( *t_cn == L'.' )
		{
			*t_cn = L'_';
		}

		++t_cn;
	}

	column_name = ( char * )malloc( sizeof( char ) * ( name_length + 1 ) );

	wcstombs_s( NULL, column_name, name_length + 1, ( wchar_t * )name, name_length );
	column_name[ name_length ] = 0;	// Sanity.

	return column_name;
}

// tableid_0A and tableid_0P will be opened on success.
// g_ci is freed in cleanup_esedb_info().
// Handles Windows Vista and Windows 7 databases.
// The Windows Property values are stored in SystemIndex_0A and the list of columns is stored in SystemIndex_0P.
// We could use MSysObjects to get the list of columns, but SystemIndex_0A contains compression and data type information.
// In the MSysObjects table, columns with a flag mask of 0x80 or 0x10 (not sure which) causes JetRetrieveColumns in update_scan_info(...) to fail. They aren't listed in SystemIndex_0P.
JET_ERR get_column_info()
{
	long type = 0, docid = 0;
	unsigned char jet_compress = 0;
	JET_COLUMNDEF name_column = { 0 }, type_column = { 0 }, jet_compress_column = { 0 }, system_index_0a_column = { 0 };

	JET_RETRIEVECOLUMN rc_0P[ 3 ] = { 0 };

	column_info *ci = NULL;
	column_info *last_ci = NULL;

	g_column_count = 0;

	// This table has a list of Windows Properties and various information about the columns we'll be retrieving.
	if ( ( g_err = open_table( "SystemIndex_0P", &g_tableid_0P ) ) != JET_errSuccess ) { goto CLEANUP; }

	if ( ( g_err = get_table_column_info( g_tableid_0P, "Name", &name_column ) ) != JET_errSuccess ) { goto CLEANUP; }
	if ( ( g_err = get_table_column_info( g_tableid_0P, "Type", &type_column ) ) != JET_errSuccess ) { goto CLEANUP; }
	if ( ( g_err = get_table_column_info( g_tableid_0P, "JetCompress", &jet_compress_column ) ) != JET_errSuccess ) { goto CLEANUP; }

	// Ensure that the values we retrieve are of the correct size.
	if ( type_column.cbMax != sizeof( long ) ||
		 jet_compress_column.cbMax != sizeof( unsigned char ) ||
		 name_column.cbMax == 0 )
	{ g_err = JET_errInvalidColumnType; goto CLEANUP; }

	// Set up the column info we want to retrieve.
	rc_0P[ 0 ].columnid = name_column.columnid;
	rc_0P[ 0 ].cbData = name_column.cbMax;
	rc_0P[ 0 ].itagSequence = 1;

	rc_0P[ 1 ].columnid = type_column.columnid;
	rc_0P[ 1 ].pvData = ( void * )&type;
	rc_0P[ 1 ].cbData = type_column.cbMax;
	rc_0P[ 1 ].itagSequence = 1;

	rc_0P[ 2 ].columnid = jet_compress_column.columnid;
	rc_0P[ 2 ].pvData = ( void * )&jet_compress;
	rc_0P[ 2 ].cbData = jet_compress_column.cbMax;
	rc_0P[ 2 ].itagSequence = 1;

	// This table has all of the Windows Properties and their values.
	if ( ( g_err = open_table( "SystemIndex_0A", &g_tableid_0A ) ) != JET_errSuccess ) { goto CLEANUP; }

	// Get the DocID table since it isn't listed in SystemIndex_0P.
	if ( JetGetTableColumnInfo( g_sesid, g_tableid_0A, "DocID", &system_index_0a_column, sizeof( system_index_0a_column ), JET_ColInfo ) == JET_errSuccess )
	{
		if ( system_index_0a_column.cbMax != sizeof( long ) ) { g_err = JET_errInvalidColumnType; goto CLEANUP; }

		++g_column_count;

		ci = ( column_info * )malloc( sizeof( column_info ) );
		ci->Name = _wcsdup( L"DocID" );
		ci->Name_byte_length = 10;
		ci->column_id = system_index_0a_column.columnid;
		ci->max_size = system_index_0a_column.cbMax;
		ci->column_type = system_index_0a_column.coltyp;
		ci->Type = VT_I4;	// 4 byte signed int
		ci->JetCompress = false;
		ci->data = NULL;
		ci->sei = NULL;
		ci->next = NULL;

		g_ci = last_ci = ci;
	}

	if ( ( g_err = JetMove( g_sesid, g_tableid_0P, JET_MoveFirst, JET_bitNil ) ) != JET_errSuccess ) { goto CLEANUP; }

	while ( true )
	{
		// Stop processing and exit the thread.
		if ( g_kill_scan == true )
		{
			break;
		}

		unsigned char *name = ( unsigned char * )malloc( sizeof( char ) * ( name_column.cbMax + 2 ) ); // Add 2 bytes for a L"\0" to be added.
		memset( name, 0, sizeof( char ) * ( name_column.cbMax + 2 ) );

		rc_0P[ 0 ].pvData = ( void * )name;

		// Retrieve the 3 column values.
		if ( ( g_err = JetRetrieveColumns( g_sesid, g_tableid_0P, rc_0P, 3 ) ) != JET_errSuccess )
		{
			free( name );
			name = NULL;

			break;
		}

		// The name should be an unterminated Unicode string. Add L"\0" to the end.
		name[ rc_0P[ 0 ].cbActual ] = 0;
		name[ rc_0P[ 0 ].cbActual + 1 ] = 0;

		++g_column_count;

		ci = ( column_info * )malloc( sizeof( column_info ) );
		ci->Name = ( wchar_t * )name;
		ci->Name_byte_length = rc_0P[ 0 ].cbActual;
		ci->Type = type;
		ci->JetCompress = ( jet_compress != 0 ? true : false );
		ci->data = NULL;
		ci->sei = NULL;
		ci->next = NULL;

		memset( &system_index_0a_column, 0, sizeof( system_index_0a_column ) );
		char *column_name = get_column_name( ci->Name, ci->Name_byte_length );	// Converts the column name into a char string and replaces all '.' with '_'.
		JetGetTableColumnInfo( g_sesid, g_tableid_0A, column_name, &system_index_0a_column, sizeof( system_index_0a_column ), JET_ColInfo );
		free( column_name );

		ci->column_type = system_index_0a_column.coltyp;
		ci->column_id = system_index_0a_column.columnid;
		ci->max_size = system_index_0a_column.cbMax;

		if ( last_ci != NULL )
		{
			last_ci->next = ci;
		}
		else
		{
			g_ci = ci;
		}

		last_ci = ci;

		if ( ci->Name_byte_length == 42 && wcscmp( ci->Name, L"System_FileAttributes" ) == 0 )
		{
			g_file_attributes = ci;
		}
		else if ( ci->Name_byte_length == 40 && wcscmp( ci->Name, L"System_FileExtension" ) == 0 )
		{
			g_file_extension = ci;
		}
		else if ( ci->Name_byte_length == 44 && wcscmp( ci->Name, L"System_ItemPathDisplay" ) == 0 )
		{
			g_item_path_display = ci;
		}
		else if ( ci->Name_byte_length == 46 && wcscmp( ci->Name, L"System_ThumbnailCacheId" ) == 0 )
		{
			g_thumbnail_cache_id = ci;
		}

		// Move to the next record (column row).
		if ( JetMove( g_sesid, g_tableid_0P, JET_MoveNext, JET_bitNil ) != JET_errSuccess )
		{
			break;
		}
	}

CLEANUP:

	return g_err;
}

// tableid_0A and tableid_0P will be opened on success.
// g_ci is freed in cleanup_esedb_info().
// Handles Windows 8+ databases.
// The Windows Property values are stored in SystemIndex_PropertyStore and the list of columns is stored in MSysObjects.
// The values are in little endian and don't appear to be compressed. There is no data type associated with the values.
JET_ERR get_column_info_win8()
{
	short type = 0;
	long objid = 0, t_objid = 0;
	JET_COLUMNDEF name_column = { 0 }, type_column = { 0 }, objid_column = { 0 }, system_index_property_store_column = { 0 };

	JET_RETRIEVECOLUMN rc_0P[ 3 ] = { 0 };

	unsigned char *name = NULL;

	bool found_index = false;

	column_info *ci = NULL;
	column_info *last_ci = NULL;

	g_column_count = 0;

	// This table has a list of Windows Properties and various information about the columns we'll be retrieving.
	if ( ( g_err = open_table( "MSysObjects", &g_tableid_0P ) ) != JET_errSuccess ) { goto CLEANUP; }

	if ( ( g_err = get_table_column_info( g_tableid_0P, "Name", &name_column ) ) != JET_errSuccess ) { goto CLEANUP; }
	if ( ( g_err = get_table_column_info( g_tableid_0P, "Type", &type_column ) ) != JET_errSuccess ) { goto CLEANUP; }
	if ( ( g_err = get_table_column_info( g_tableid_0P, "ObjidTable", &objid_column ) ) != JET_errSuccess ) { goto CLEANUP; }

	// Ensure that the values we retrieve are of the correct size.
	if ( type_column.cbMax != sizeof( short ) ||
		 objid_column.cbMax != sizeof( long ) ||
		 name_column.cbMax == 0 )
	{ g_err = JET_errInvalidColumnType; goto CLEANUP; }

	name = ( unsigned char * )malloc( sizeof( char ) * ( name_column.cbMax + 2 ) ); // Add 2 bytes for a L"\0" to be added.

	// Set up the column info we want to retrieve.
	rc_0P[ 0 ].columnid = name_column.columnid;
	rc_0P[ 0 ].pvData = ( void * )name;
	rc_0P[ 0 ].cbData = name_column.cbMax;
	rc_0P[ 0 ].itagSequence = 1;

	rc_0P[ 1 ].columnid = type_column.columnid;
	rc_0P[ 1 ].pvData = ( void * )&type;
	rc_0P[ 1 ].cbData = type_column.cbMax;
	rc_0P[ 1 ].itagSequence = 1;

	rc_0P[ 2 ].columnid = objid_column.columnid;
	rc_0P[ 2 ].pvData = ( void * )&objid;
	rc_0P[ 2 ].cbData = objid_column.cbMax;
	rc_0P[ 2 ].itagSequence = 1;

	if ( ( g_err = JetMove( g_sesid, g_tableid_0P, JET_MoveFirst, JET_bitNil ) ) != JET_errSuccess ) { goto CLEANUP; }

	while ( true )
	{
		// Stop processing and exit the thread.
		if ( g_kill_scan == true )
		{
			break;
		}

		memset( name, 0, sizeof( char ) * ( name_column.cbMax + 2 ) );

		// Retrieve the 3 column values.
		if ( ( g_err = JetRetrieveColumns( g_sesid, g_tableid_0P, rc_0P, 3 ) ) != JET_errSuccess )
		{
			break;
		}

		// Look for a table record (type == 1).
		if ( type == 1 )
		{
			// The name should be an unterminated ASCII string. Add L"\0" to the end.
			name[ rc_0P[ 0 ].cbActual ] = 0;
			name[ rc_0P[ 0 ].cbActual + 1 ] = 0;

			if ( strcmp( ( const char * )name, "SystemIndex_PropertyStore" ) == 0 )
			{
				t_objid = objid;
				objid = 0;
				type = 0;
				found_index = true;
				break;
			}
		}

		// Move to the next record (column row).
		if ( JetMove( g_sesid, g_tableid_0P, JET_MoveNext, JET_bitNil ) != JET_errSuccess )
		{
			break;
		}
	}

	if ( found_index == true )
	{
		// This table has all of the Windows Properties and their values.
		if ( ( g_err = open_table( "SystemIndex_PropertyStore", &g_tableid_0A ) ) != JET_errSuccess ) { goto CLEANUP; }

		if ( ( g_err = JetMove( g_sesid, g_tableid_0P, JET_MoveFirst, JET_bitNil ) ) != JET_errSuccess ) { goto CLEANUP; }

		while ( true )
		{
			// Stop processing and exit the thread.
			if ( g_kill_scan == true )
			{
				break;
			}

			memset( name, 0, sizeof( char ) * ( name_column.cbMax + 2 ) );

			// Retrieve the 3 column values.
			if ( ( g_err = JetRetrieveColumns( g_sesid, g_tableid_0P, rc_0P, 3 ) ) != JET_errSuccess )
			{
				break;
			}

			// The name should be an unterminated ASCII string. Add L"\0" to the end.
			name[ rc_0P[ 0 ].cbActual ] = 0;
			name[ rc_0P[ 0 ].cbActual + 1 ] = 0;

			// Make sure our record is a column (type == 2) and it belongs to the SystemIndex_PropertyStore table.
			if ( objid == t_objid && type == 2 )
			{
				++g_column_count;

				// Windows 8+ columns have a hex number followed by a '-' separating the Windows Property.
				char *prop_name = strchr( ( char * )name, '-' );
				if ( prop_name == NULL )
				{
					prop_name = ( char * )name;
				}
				else
				{
					++prop_name;
				}

				// Convert the ASCII name to a wide char string.
				int val_length = MultiByteToWideChar( CP_UTF8, 0, prop_name, -1, NULL, 0 );	// Include the NULL terminator.
				wchar_t *val = ( wchar_t * )malloc( sizeof( wchar_t ) * val_length );
				MultiByteToWideChar( CP_UTF8, 0, prop_name, -1, val, val_length );

				ci = ( column_info * )malloc( sizeof( column_info ) );
				ci->Name = val;
				ci->Name_byte_length = ( val_length - 1 ) * sizeof( wchar_t );
				ci->JetCompress = false;
				ci->data = NULL;
				ci->sei = NULL;
				ci->next = NULL;

				memset( &system_index_property_store_column, 0, sizeof( system_index_property_store_column ) );
				JetGetTableColumnInfo( g_sesid, g_tableid_0A, ( JET_PCSTR )name, &system_index_property_store_column, sizeof( system_index_property_store_column ), JET_ColInfo );

				ci->column_type = system_index_property_store_column.coltyp;
				ci->column_id = system_index_property_store_column.columnid;
				ci->max_size = system_index_property_store_column.cbMax;

				// There's no data type information in Windows 8+ databases, so we'll make a guess based on the column type and data size.
				if ( ci->column_type == JET_coltypBinary && ci->max_size == sizeof( unsigned long long ) )
				{
					ci->Type = VT_FILETIME;
				}
				else if ( ci->column_type == JET_coltypBit && ci->max_size == sizeof( unsigned char ) )
				{
					ci->Type = VT_BOOL;
				}

				if ( last_ci != NULL )
				{
					last_ci->next = ci;
				}
				else
				{
					g_ci = ci;
				}

				last_ci = ci;

				if ( ci->Name_byte_length == 42 && wcscmp( ci->Name, L"System_FileAttributes" ) == 0 )
				{
					g_file_attributes = ci;
				}
				else if ( ci->Name_byte_length == 40 && wcscmp( ci->Name, L"System_FileExtension" ) == 0 )
				{
					g_file_extension = ci;
				}
				else if ( ci->Name_byte_length == 44 && wcscmp( ci->Name, L"System_ItemPathDisplay" ) == 0 )
				{
					g_item_path_display = ci;
				}
				else if ( ci->Name_byte_length == 46 && wcscmp( ci->Name, L"System_ThumbnailCacheId" ) == 0 )
				{
					ci->Type = 0;	// Reset the data type.
					g_thumbnail_cache_id = ci;
				}
				else if ( ( ci->Name_byte_length == 22 && wcscmp( ci->Name, L"System_Size" ) == 0 ) ||
						  ( ci->Name_byte_length == 42 && wcscmp( ci->Name, L"System_Media_Duration" ) == 0 ) ||
						  ( ci->Name_byte_length == 64 && wcscmp( ci->Name, L"System_Document_TotalEditingTime" ) == 0 ) ||
						  ( ci->Name_byte_length == 28 && wcscmp( ci->Name, L"System_FileFRN" ) == 0 ) )
				{
					ci->Type = 0;	// Reset the data type.
				}
			}

			// Move to the next record (column row).
			if ( JetMove( g_sesid, g_tableid_0P, JET_MoveNext, JET_bitNil ) != JET_errSuccess )
			{
				break;
			}
		}
	}

CLEANUP:

	free( name );
	name = NULL;

	return g_err;
}

// Build the retrieve column array's values from the columns list.
// g_rc_array is freed in cleanup_esedb_info().
void build_retrieve_column_array()
{
	if ( g_ci == NULL || g_column_count == 0 )
	{
		return;
	}

	column_info *t_ci = g_ci;

	g_rc_array = ( JET_RETRIEVECOLUMN * )malloc( sizeof( JET_RETRIEVECOLUMN ) * g_column_count );
	memset( g_rc_array, 0, sizeof( JET_RETRIEVECOLUMN ) * g_column_count );

	for ( unsigned long i = 0; i < g_column_count; ++i )
	{
		// Stop processing and exit the thread.
		if ( g_kill_scan == true )
		{
			break;
		}

		if ( t_ci != NULL )
		{
			t_ci->data = ( unsigned char * )malloc( sizeof( unsigned char ) * t_ci->max_size );
			memset( t_ci->data, 0, sizeof( char ) * t_ci->max_size );

			g_rc_array[ i ].columnid = t_ci->column_id;
			g_rc_array[ i ].pvData = ( void * )t_ci->data;
			g_rc_array[ i ].cbData = t_ci->max_size;
			g_rc_array[ i ].itagSequence = 1;

			t_ci = t_ci->next;
		}
		else
		{
			break;
		}
	}
}
