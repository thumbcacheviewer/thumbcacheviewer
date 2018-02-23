/*
	thumbcache_viewer will extract thumbnail images from thumbcache database files.
	Copyright (C) 2011-2018 Eric Kutcher

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

#include "lite_msscb.h"
#include "lite_mssrch.h"	// For the typedef

//pMSSCompressText		MSSCompressText;
//pMSSUncompressText	MSSUncompressText;

HMODULE hModule_msscb = NULL;

unsigned char msscb_state = 0;	// 0 = Not running, 1 = running.

bool InitializeMsSCB()
{
	if ( msscb_state != MSSCB_STATE_SHUTDOWN )
	{
		return true;
	}

	SetErrorMode( SEM_FAILCRITICALERRORS );
	hModule_msscb = LoadLibrary( L"msscb.dll" );
	SetErrorMode( 0 );

	if ( hModule_msscb == NULL )
	{
		return false;
	}

	/*MSSCompressText = ( pMSSCompressText )GetProcAddress( hModule_msscb, "MSSCompressText" );
	if ( MSSCompressText == NULL )
	{
		FreeLibrary( hModule_msscb );
		hModule_msscb = NULL;
		return false;
	}*/

	MSSUncompressText = ( pMSSUncompressText )GetProcAddress( hModule_msscb, "MSSUncompressText" );
	if ( MSSUncompressText == NULL )
	{
		FreeLibrary( hModule_msscb );
		hModule_msscb = NULL;
		return false;
	}

	msscb_state = MSSCB_STATE_RUNNING;

	return true;
}

bool UnInitializeMsSCB()
{
	if ( msscb_state != MSSCB_STATE_SHUTDOWN )
	{
		msscb_state = MSSCB_STATE_SHUTDOWN;

		return ( FreeLibrary( hModule_msscb ) == FALSE ? false : true );
	}

	return true;
}
