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

#ifndef READ_ESEDB_H
#define READ_ESEDB_H

#define JET_VERSION 0x0501
#include <esent.h>
#include <wtypes.h>

#include "globals.h"
#include "dllrbt.h"

#define ERROR_BUFFER_SIZE	1024

#define JET_coltypUnsignedLong		14
#define JET_coltypLongLong			15
#define JET_coltypGUID				16
#define JET_coltypUnsignedShort		17

struct COLUMN_INFO
{
	wchar_t *Name;					// Windows Property string (the unformatted column name in tableid_0A)
	unsigned char *data;			// The retrieved record value.
	COLUMN_INFO *next;				// Next property
	unsigned long Name_byte_length;	// The btye length of the Windows Property string excluding the NULL character.
	VARENUM Type;					// The VarType of the column.
	unsigned long column_type;		// The Jet column data type.
	long column_id;					// The column ID.
	unsigned long max_size;			// The maximum size of the column's records.
	bool JetCompress;				// The column has compressed data.
};

JET_ERR InitESEDBInfo( wchar_t *database_filepath, unsigned long revision, unsigned long page_size );
JET_ERR OpenTable( JET_PCSTR szTableName, JET_TABLEID *ptableid );
JET_ERR GetTableColumnInfo( JET_TABLEID tableid, JET_PCSTR szColumnName, JET_COLUMNDEF *pcolumndef );
void CleanupESEDBInfo();

JET_ERR GetColumnInfo();		// tableid_0A and tableid_0P will be opened on success.
JET_ERR GetColumnInfoWin8();	// tableid_0A and tableid_0P will be opened on success.
void BuildRetrieveColumnArray();
wchar_t *UncompressValue( unsigned char *value, unsigned long value_length );
void ConvertValues( EXTENDED_INFO **ei );

void SetErrorMessage( char *msg );
void HandleESEDBError();

// Internal variables. All all freed/reset in CleanupESEDBInfo().
extern JET_ERR g_err;
extern JET_INSTANCE g_instance;
extern JET_SESID g_sesid;
extern JET_DBID g_dbid;
extern JET_TABLEID g_tableid_0P, g_tableid_0A;
extern JET_RETRIEVECOLUMN *g_rc_array;
extern unsigned long g_column_count;
extern char *g_ascii_filepath;
extern COLUMN_INFO *g_ci;

extern COLUMN_INFO *g_thumbnail_cache_id;

extern bool g_use_big_endian;
extern unsigned long g_revision;

// Internal error states.
extern int g_error_offset;
extern char g_error[ ERROR_BUFFER_SIZE ];
extern unsigned char g_error_state;

extern dllrbt_tree *g_file_info_tree;

#endif
