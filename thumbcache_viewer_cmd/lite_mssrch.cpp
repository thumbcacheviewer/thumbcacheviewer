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

//pMSSCompressText		MSSCompressText;
pMSSUncompressText		MSSUncompressText;

HMODULE hModule_mssrch = NULL;

unsigned char mssrch_state = 0;	// 0 = Not running, 1 = running.

bool InitializeMsSrch()
{
	if ( mssrch_state != MSSRCH_STATE_SHUTDOWN )
	{
		return true;
	}

	SetErrorMode( SEM_FAILCRITICALERRORS );
	hModule_mssrch = LoadLibrary( L"mssrch.dll" );
	SetErrorMode( 0 );

	if ( hModule_mssrch == NULL )
	{
		return false;
	}

	/*MSSCompressText = ( pMSSCompressText )GetProcAddress( hModule_mssrch, "MSSCompressText" );
	if ( MSSCompressText == NULL )
	{
		FreeLibrary( hModule_mssrch );
		hModule_mssrch = NULL;
		return false;
	}*/

	MSSUncompressText = ( pMSSUncompressText )GetProcAddress( hModule_mssrch, "MSSUncompressText" );
	if ( MSSUncompressText == NULL )
	{
		FreeLibrary( hModule_mssrch );
		hModule_mssrch = NULL;
		return false;
	}

	mssrch_state = MSSRCH_STATE_RUNNING;

	return true;
}

bool UnInitializeMsSrch()
{
	if ( mssrch_state != MSSRCH_STATE_SHUTDOWN )
	{
		mssrch_state = MSSRCH_STATE_SHUTDOWN;

		return ( FreeLibrary( hModule_mssrch ) == FALSE ? false : true );
	}

	return true;
}
