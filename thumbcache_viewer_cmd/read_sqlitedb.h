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

#ifndef READ_SQLITEDB_H
#define READ_SQLITEDB_H

#include "globals.h"

#include <wtypes.h>

struct SHARED_EXTENDED_INFO
{
	wchar_t *windows_property;
};

struct PROPERTY_INFO
{
	SHARED_EXTENDED_INFO *sei;
	unsigned long property_name_length;
	unsigned long id;
};

void CleanupSQLiteInfo();

int BuildPropertyTreeCallback( void * /*arg*/, int argc, char **argv, char ** /*azColName*/ );
int CreatePropertyInfoCallback( void *arg, int argc, char **argv, char ** /*azColName*/ );

int WINAPIV c_BuildPropertyTreeCallback( void *arg, int argc, char **argv, char **azColName );
int WINAPIV c_CreatePropertyInfoCallback( void *arg, int argc, char **argv, char **azColName );

int WINAPI s_BuildPropertyTreeCallback( void *arg, int argc, char **argv, char **azColName );
int WINAPI s_CreatePropertyInfoCallback( void *arg, int argc, char **argv, char **azColName );

extern void *g_sql_db;

#endif
