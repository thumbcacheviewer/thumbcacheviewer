/*
	thumbcache_viewer will extract thumbnail images from thumbcache database files.
	Copyright (C) 2011-2021 Eric Kutcher

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

#ifndef READ_ESEDB_H
#define READ_ESEDB_H

#define JET_VERSION 0x0501
#include <esent.h>

#include "globals.h"

#define ERROR_BUFFER_SIZE	1024

struct column_info
{
	wchar_t *Name;					// Windows Property string (the unformatted column name in tableid_0A)
	unsigned char *data;			// The retrieved record value.
	shared_extended_info *sei;		// Temporary pointer to hold the Windows Property string when processing the edb.
	column_info *next;				// Next property
	unsigned long Name_byte_length;	// The btye length of the Windows Property string excluding the NULL character.
	unsigned long Type;				// The VarType of the column.
	unsigned long column_type;		// The Jet column data type.
	long column_id;					// The column ID.
	unsigned long max_size;			// The maximum size of the column's records.
	bool JetCompress;				// The column has compressed data.
};

JET_ERR init_esedb_info( wchar_t *database_filepath );
JET_ERR open_table( JET_PCSTR szTableName, JET_TABLEID *ptableid );
JET_ERR get_table_column_info( JET_TABLEID tableid, JET_PCSTR szColumnName, JET_COLUMNDEF *pcolumndef );
void cleanup_esedb_info();

JET_ERR get_column_info();		// tableid_0A and tableid_0P will be opened on success.
JET_ERR get_column_info_win8();	// tableid_0A and tableid_0P will be opened on success.
void build_retrieve_column_array();
wchar_t *uncompress_value( unsigned char *value, unsigned long value_length );
void convert_values( extended_info **ei );

void set_error_message( char *msg );
void handle_esedb_error();

// Internal variables. All all freed/reset in cleanup_esedb_info.
extern JET_ERR g_err;
extern JET_INSTANCE g_instance;
extern JET_SESID g_sesid;
extern JET_DBID g_dbid;
extern JET_TABLEID g_tableid_0P, g_tableid_0A;
extern JET_RETRIEVECOLUMN *g_rc_array;
extern unsigned long g_column_count;
extern char *g_ascii_filepath;
extern column_info *g_ci;

extern column_info *g_item_path_display;
extern column_info *g_file_attributes;
extern column_info *g_thumbnail_cache_id;
extern column_info *g_file_extension;

extern bool g_use_big_endian;
extern unsigned long g_revision;

// Internal error states.
extern int g_error_offset;
extern char g_error[ ERROR_BUFFER_SIZE ];
extern unsigned char g_error_state;

#endif
