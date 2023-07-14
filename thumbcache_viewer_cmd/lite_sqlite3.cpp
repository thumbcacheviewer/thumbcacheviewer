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

//c_psqlite3_open16		c_sqlite3_open16;
c_psqlite3_open_v2		c_sqlite3_open_v2;
c_psqlite3_exec		c_sqlite3_exec;
//c_psqlite3_errmsg		c_sqlite3_errmsg;
//c_psqlite3_errmsg16		c_sqlite3_errmsg16;
c_psqlite3_free		c_sqlite3_free;
c_psqlite3_close		c_sqlite3_close;

//s_psqlite3_open16		s_sqlite3_open16;
s_psqlite3_open_v2		s_sqlite3_open_v2;
s_psqlite3_exec		s_sqlite3_exec;
//s_psqlite3_errmsg		s_sqlite3_errmsg;
//s_psqlite3_errmsg16		s_sqlite3_errmsg16;
s_psqlite3_free		s_sqlite3_free;
s_psqlite3_close		s_sqlite3_close;

HMODULE hModule_sqlite3 = NULL;

unsigned char sqlite3_state = 0;	// 0 = Not running, 1 = running.

unsigned char sqlite3_calling_convention = 0;	// 0 = cdecl, 1 = stdcall

bool InitializeSQLite3()
{
	if ( sqlite3_state != SQLITE3_STATE_SHUTDOWN )
	{
		return true;
	}

	SetErrorMode( SEM_FAILCRITICALERRORS );
	// Try to load sqlite3.dll first since it's more universal.
	hModule_sqlite3 = LoadLibrary( L"sqlite3.dll" );				// Has to be downloaded from sqlite.org.
	if ( hModule_sqlite3 == NULL )
	{
		hModule_sqlite3 = LoadLibrary( L"SearchIndexerCore.dll" );	// Native to Windows 11.
		if ( hModule_sqlite3 != NULL )
		{
			sqlite3_calling_convention = 1;
		}
	}
	else
	{
		sqlite3_calling_convention = 0;
	}
	SetErrorMode( 0 );

	if ( hModule_sqlite3 == NULL )
	{
		return false;
	}

	if ( sqlite3_calling_convention == 1 )
	{
		//s_sqlite3_open16 = ( s_psqlite3_open16 )GetProcAddress( hModule_sqlite3, "sqlite3_open16" );
		//if ( s_sqlite3_open16 == NULL ) { goto CLEANUP; }
		s_sqlite3_open_v2 = ( s_psqlite3_open_v2 )GetProcAddress( hModule_sqlite3, "sqlite3_open_v2" );
		if ( s_sqlite3_open_v2 == NULL ) { goto CLEANUP; }
		s_sqlite3_exec = ( s_psqlite3_exec )GetProcAddress( hModule_sqlite3, "sqlite3_exec" );
		if ( s_sqlite3_exec == NULL ) { goto CLEANUP; }
		//s_sqlite3_errmsg = ( s_psqlite3_errmsg )GetProcAddress( hModule_sqlite3, "sqlite3_errmsg" );
		//if ( s_sqlite3_errmsg == NULL ) { goto CLEANUP; }
		//s_sqlite3_errmsg16 = ( s_psqlite3_errmsg16 )GetProcAddress( hModule_sqlite3, "sqlite3_errmsg16" );
		//if ( s_sqlite3_errmsg16 == NULL ) { goto CLEANUP; }
		s_sqlite3_free = ( s_psqlite3_free )GetProcAddress( hModule_sqlite3, "sqlite3_free" );
		if ( s_sqlite3_free == NULL ) { goto CLEANUP; }
		s_sqlite3_close = ( s_psqlite3_close )GetProcAddress( hModule_sqlite3, "sqlite3_close" );
		if ( s_sqlite3_close == NULL ) { goto CLEANUP; }
	}
	else
	{
		//c_sqlite3_open16 = ( c_psqlite3_open16 )GetProcAddress( hModule_sqlite3, "sqlite3_open16" );
		//if ( c_sqlite3_open16 == NULL ) { goto CLEANUP; }
		c_sqlite3_open_v2 = ( c_psqlite3_open_v2 )GetProcAddress( hModule_sqlite3, "sqlite3_open_v2" );
		if ( c_sqlite3_open_v2 == NULL ) { goto CLEANUP; }
		c_sqlite3_exec = ( c_psqlite3_exec )GetProcAddress( hModule_sqlite3, "sqlite3_exec" );
		if ( c_sqlite3_exec == NULL ) { goto CLEANUP; }
		//c_sqlite3_errmsg = ( c_psqlite3_errmsg )GetProcAddress( hModule_sqlite3, "sqlite3_errmsg" );
		//if ( c_sqlite3_errmsg == NULL ) { goto CLEANUP; }
		//c_sqlite3_errmsg16 = ( c_psqlite3_errmsg16 )GetProcAddress( hModule_sqlite3, "sqlite3_errmsg16" );
		//if ( c_sqlite3_errmsg16 == NULL ) { goto CLEANUP; }
		c_sqlite3_free = ( c_psqlite3_free )GetProcAddress( hModule_sqlite3, "sqlite3_free" );
		if ( c_sqlite3_free == NULL ) { goto CLEANUP; }
		c_sqlite3_close = ( c_psqlite3_close )GetProcAddress( hModule_sqlite3, "sqlite3_close" );
		if ( c_sqlite3_close == NULL ) { goto CLEANUP; }
	}

	sqlite3_state = SQLITE3_STATE_RUNNING;

	return true;

CLEANUP:
	
	FreeLibrary( hModule_sqlite3 );
	hModule_sqlite3 = NULL;
	return false;
}

bool UnInitializeSQLite3()
{
	if ( sqlite3_state != SQLITE3_STATE_SHUTDOWN )
	{
		sqlite3_state = SQLITE3_STATE_SHUTDOWN;

		return ( FreeLibrary( hModule_sqlite3 ) == FALSE ? false : true );
	}

	return true;
}
