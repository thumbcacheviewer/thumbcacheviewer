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

#ifndef LITE_SQLITE3_H
#define LITE_SQLITE3_H

// Extensible Storage Engine library.
#pragma comment( lib, "esent.lib" )

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define SQLITE3_STATE_SHUTDOWN	0
#define SQLITE3_STATE_RUNNING	1

#define SQLITE_OK				0
#define SQLITE_OPEN_READONLY	0x00000001

//typedef int ( WINAPIV *c_psqlite3_open16 )( const void *filename, void /*sqlite3*/ **ppDb );
typedef int ( WINAPIV *c_psqlite3_open_v2 )( const void *filename, void /*sqlite3*/ **ppDb, int flags, const char *zVfs );
typedef int ( WINAPIV *c_psqlite3_exec )( void /*sqlite3*/ *pDb, const char *sql, int ( WINAPIV *callback )( void *, int, char **, char ** ), void *arg, char **errmsg );
//typedef const char * ( WINAPIV *c_psqlite3_errmsg )( void /*sqlite3*/ *pDb );
//typedef const void * ( WINAPIV *c_psqlite3_errmsg16 )( void /*sqlite3*/ *pDb );
typedef void ( WINAPIV *c_psqlite3_free )( void *val );
typedef int ( WINAPIV *c_psqlite3_close )( void /*sqlite3*/ *pDb );

//typedef int ( WINAPI *s_psqlite3_open16 )( const void *filename, void /*sqlite3*/ **ppDb );
typedef int ( WINAPI *s_psqlite3_open_v2 )( const void *filename, void /*sqlite3*/ **ppDb, int flags, const char *zVfs );
typedef int ( WINAPI *s_psqlite3_exec )( void /*sqlite3*/ *pDb, const char *sql, int ( WINAPI *callback )( void *, int, char **, char ** ), void *arg, char **errmsg );
//typedef const char * ( WINAPI *s_psqlite3_errmsg )( void /*sqlite3*/ *pDb );
//typedef const void * ( WINAPI *s_psqlite3_errmsg16 )( void /*sqlite3*/ *pDb );
typedef void ( WINAPI *s_psqlite3_free )( void *val );
typedef int ( WINAPI *s_psqlite3_close )( void /*sqlite3*/ *pDb );

//extern c_psqlite3_open16		c_sqlite3_open16;
extern c_psqlite3_open_v2		c_sqlite3_open_v2;
extern c_psqlite3_exec		c_sqlite3_exec;
//extern c_psqlite3_errmsg		c_sqlite3_errmsg;
//extern c_psqlite3_errmsg16		c_sqlite3_errmsg16;
extern c_psqlite3_free		c_sqlite3_free;
extern c_psqlite3_close		c_sqlite3_close;

//extern s_psqlite3_open16		s_sqlite3_open16;
extern s_psqlite3_open_v2		s_sqlite3_open_v2;
extern s_psqlite3_exec		s_sqlite3_exec;
//extern s_psqlite3_errmsg		s_sqlite3_errmsg;
//extern s_psqlite3_errmsg16		s_sqlite3_errmsg16;
extern s_psqlite3_free		s_sqlite3_free;
extern s_psqlite3_close		s_sqlite3_close;

extern unsigned char sqlite3_state;

extern unsigned char sqlite3_calling_convention;

#define sqlite3_open_v2( filename, ppDb, flags, zVfs )	( sqlite3_calling_convention == 1 ? s_sqlite3_open_v2( filename, ppDb, flags, zVfs ) : c_sqlite3_open_v2( filename, ppDb, flags, zVfs ) )
#define sqlite3_exec( pDb, sql, fp, arg, errmsg ) ( sqlite3_calling_convention == 1 ? s_sqlite3_exec( pDb, sql, s_##fp, arg, errmsg ) : c_sqlite3_exec( pDb, sql, c_##fp, arg, errmsg ) )
#define sqlite3_free( val ) ( sqlite3_calling_convention == 1 ? s_sqlite3_free( val ) : c_sqlite3_free( val ) )
#define sqlite3_close( pDb ) ( sqlite3_calling_convention == 1 ? s_sqlite3_close( pDb ) : c_sqlite3_close( pDb ) )

bool InitializeSQLite3();
bool UnInitializeSQLite3();

#endif
