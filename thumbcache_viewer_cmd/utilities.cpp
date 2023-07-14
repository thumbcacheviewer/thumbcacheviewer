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

#include "utilities.h"

wchar_t *GetSFGAOStr( unsigned long sfgao_flags )
{
	wchar_t *ret = NULL;
	if ( sfgao_flags == 0 )
	{
		ret = ( wchar_t * )malloc( sizeof( wchar_t ) * 5 );
		wmemcpy_s( ret, 5, L"None\0", 5 );
	}

	int size = _scwprintf( L"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
						( ( sfgao_flags & SFGAO_CANCOPY ) ? L"SFGAO_CANCOPY, " : L"" ),
						( ( sfgao_flags & SFGAO_CANMOVE ) ? L"SFGAO_CANMOVE, " : L"" ),
						( ( sfgao_flags & SFGAO_CANLINK ) ? L"SFGAO_CANLINK, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGE ) ? L"SFGAO_STORAGE, " : L"" ),
						( ( sfgao_flags & SFGAO_CANRENAME ) ? L"SFGAO_CANRENAME, " : L"" ),
						( ( sfgao_flags & SFGAO_CANDELETE ) ? L"SFGAO_CANDELETE, " : L"" ),
						( ( sfgao_flags & SFGAO_HASPROPSHEET ) ? L"SFGAO_HASPROPSHEET, " : L"" ),
						( ( sfgao_flags & SFGAO_DROPTARGET ) ? L"SFGAO_DROPTARGET, " : L"" ),
						( ( sfgao_flags & SFGAO_CAPABILITYMASK ) ? L"SFGAO_CAPABILITYMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_ENCRYPTED ) ? L"SFGAO_ENCRYPTED, " : L"" ),
						( ( sfgao_flags & SFGAO_ISSLOW ) ? L"SFGAO_ISSLOW, " : L"" ),
						( ( sfgao_flags & SFGAO_GHOSTED ) ? L"SFGAO_GHOSTED, " : L"" ),
						( ( sfgao_flags & SFGAO_LINK ) ? L"SFGAO_LINK, " : L"" ),
						( ( sfgao_flags & SFGAO_SHARE ) ? L"SFGAO_SHARE, " : L"" ),
						( ( sfgao_flags & SFGAO_READONLY ) ? L"SFGAO_READONLY, " : L"" ),
						( ( sfgao_flags & SFGAO_HIDDEN ) ? L"SFGAO_HIDDEN, " : L"" ),
						( ( sfgao_flags & SFGAO_DISPLAYATTRMASK ) ? L"SFGAO_DISPLAYATTRMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_FILESYSANCESTOR ) ? L"SFGAO_FILESYSANCESTOR, " : L"" ),
						( ( sfgao_flags & SFGAO_FOLDER ) ? L"SFGAO_FOLDER, " : L"" ),
						( ( sfgao_flags & SFGAO_FILESYSTEM ) ? L"SFGAO_FILESYSTEM, " : L"" ),
						( ( sfgao_flags & SFGAO_HASSUBFOLDER ) ? L"SFGAO_HASSUBFOLDER / SFGAO_CONTENTSMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_VALIDATE ) ? L"SFGAO_VALIDATE, " : L"" ),
						( ( sfgao_flags & SFGAO_REMOVABLE ) ? L"SFGAO_REMOVABLE, " : L"" ),
						( ( sfgao_flags & SFGAO_COMPRESSED ) ? L"SFGAO_COMPRESSED, " : L"" ),
						( ( sfgao_flags & SFGAO_BROWSABLE ) ? L"SFGAO_BROWSABLE, " : L"" ),
						( ( sfgao_flags & SFGAO_NONENUMERATED ) ? L"SFGAO_NONENUMERATED, " : L"" ),
						( ( sfgao_flags & SFGAO_NEWCONTENT ) ? L"SFGAO_NEWCONTENT, " : L"" ),
						( ( sfgao_flags & SFGAO_CANMONIKER ) ? L"SFGAO_CANMONIKER / SFGAO_HASSTORAGE / SFGAO_STREAM, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGEANCESTOR ) ? L"SFGAO_STORAGEANCESTOR, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGECAPMASK ) ? L"SFGAO_STORAGECAPMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_PKEYSFGAOMASK ) ? L"SFGAO_PKEYSFGAOMASK" : L"" ) );

	ret = ( wchar_t * )malloc( sizeof( wchar_t ) * ( size + 1 ) );

	size = swprintf_s( ret, size + 1, L"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
						( ( sfgao_flags & SFGAO_CANCOPY ) ? L"SFGAO_CANCOPY, " : L"" ),
						( ( sfgao_flags & SFGAO_CANMOVE ) ? L"SFGAO_CANMOVE, " : L"" ),
						( ( sfgao_flags & SFGAO_CANLINK ) ? L"SFGAO_CANLINK, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGE ) ? L"SFGAO_STORAGE, " : L"" ),
						( ( sfgao_flags & SFGAO_CANRENAME ) ? L"SFGAO_CANRENAME, " : L"" ),
						( ( sfgao_flags & SFGAO_CANDELETE ) ? L"SFGAO_CANDELETE, " : L"" ),
						( ( sfgao_flags & SFGAO_HASPROPSHEET ) ? L"SFGAO_HASPROPSHEET, " : L"" ),
						( ( sfgao_flags & SFGAO_DROPTARGET ) ? L"SFGAO_DROPTARGET, " : L"" ),
						( ( sfgao_flags & SFGAO_CAPABILITYMASK ) ? L"SFGAO_CAPABILITYMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_ENCRYPTED ) ? L"SFGAO_ENCRYPTED, " : L"" ),
						( ( sfgao_flags & SFGAO_ISSLOW ) ? L"SFGAO_ISSLOW, " : L"" ),
						( ( sfgao_flags & SFGAO_GHOSTED ) ? L"SFGAO_GHOSTED, " : L"" ),
						( ( sfgao_flags & SFGAO_LINK ) ? L"SFGAO_LINK, " : L"" ),
						( ( sfgao_flags & SFGAO_SHARE ) ? L"SFGAO_SHARE, " : L"" ),
						( ( sfgao_flags & SFGAO_READONLY ) ? L"SFGAO_READONLY, " : L"" ),
						( ( sfgao_flags & SFGAO_HIDDEN ) ? L"SFGAO_HIDDEN, " : L"" ),
						( ( sfgao_flags & SFGAO_DISPLAYATTRMASK ) ? L"SFGAO_DISPLAYATTRMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_FILESYSANCESTOR ) ? L"SFGAO_FILESYSANCESTOR, " : L"" ),
						( ( sfgao_flags & SFGAO_FOLDER ) ? L"SFGAO_FOLDER, " : L"" ),
						( ( sfgao_flags & SFGAO_FILESYSTEM ) ? L"SFGAO_FILESYSTEM, " : L"" ),
						( ( sfgao_flags & SFGAO_HASSUBFOLDER ) ? L"SFGAO_HASSUBFOLDER / SFGAO_CONTENTSMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_VALIDATE ) ? L"SFGAO_VALIDATE, " : L"" ),
						( ( sfgao_flags & SFGAO_REMOVABLE ) ? L"SFGAO_REMOVABLE, " : L"" ),
						( ( sfgao_flags & SFGAO_COMPRESSED ) ? L"SFGAO_COMPRESSED, " : L"" ),
						( ( sfgao_flags & SFGAO_BROWSABLE ) ? L"SFGAO_BROWSABLE, " : L"" ),
						( ( sfgao_flags & SFGAO_NONENUMERATED ) ? L"SFGAO_NONENUMERATED, " : L"" ),
						( ( sfgao_flags & SFGAO_NEWCONTENT ) ? L"SFGAO_NEWCONTENT, " : L"" ),
						( ( sfgao_flags & SFGAO_CANMONIKER ) ? L"SFGAO_CANMONIKER / SFGAO_HASSTORAGE / SFGAO_STREAM, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGEANCESTOR ) ? L"SFGAO_STORAGEANCESTOR, " : L"" ),
						( ( sfgao_flags & SFGAO_STORAGECAPMASK ) ? L"SFGAO_STORAGECAPMASK, " : L"" ),
						( ( sfgao_flags & SFGAO_PKEYSFGAOMASK ) ? L"SFGAO_PKEYSFGAOMASK" : L"" ) );

	// Remove any trailing ", ".
	if ( size > 1 && ret[ size - 1 ] == L' ' )
	{
		ret[ size - 2 ] = L'\0';
	}

	return ret;
}

wchar_t *GetFileAttributesStr( unsigned long fa_flags )
{
	wchar_t *ret = NULL;
	if ( fa_flags == 0 )
	{
		ret = ( wchar_t * )malloc( sizeof( wchar_t ) * 5 );
		wmemcpy_s( ret, 5, L"None\0", 5 );
	}
	else
	{
		int size = _scwprintf( L"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
							( ( fa_flags & FILE_ATTRIBUTE_READONLY ) ? L"FILE_ATTRIBUTE_READONLY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_HIDDEN ) ? L"FILE_ATTRIBUTE_HIDDEN, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_SYSTEM ) ? L"FILE_ATTRIBUTE_SYSTEM, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_DIRECTORY ) ? L"FILE_ATTRIBUTE_DIRECTORY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_ARCHIVE ) ? L"FILE_ATTRIBUTE_ARCHIVE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_DEVICE ) ? L"FILE_ATTRIBUTE_DEVICE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_NORMAL ) ? L"FILE_ATTRIBUTE_NORMAL, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_TEMPORARY ) ? L"FILE_ATTRIBUTE_TEMPORARY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_SPARSE_FILE ) ? L"FILE_ATTRIBUTE_SPARSE_FILE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_REPARSE_POINT ) ? L"FILE_ATTRIBUTE_REPARSE_POINT, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_COMPRESSED ) ? L"FILE_ATTRIBUTE_COMPRESSED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_OFFLINE ) ? L"FILE_ATTRIBUTE_OFFLINE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED ) ? L"FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_ENCRYPTED ) ? L"FILE_ATTRIBUTE_ENCRYPTED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_VIRTUAL ) ? L"FILE_ATTRIBUTE_VIRTUAL" : L"" ) );

		ret = ( wchar_t * )malloc( sizeof( wchar_t ) * ( size + 1 ) );

		size = swprintf_s( ret, size + 1, L"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
							( ( fa_flags & FILE_ATTRIBUTE_READONLY ) ? L"FILE_ATTRIBUTE_READONLY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_HIDDEN ) ? L"FILE_ATTRIBUTE_HIDDEN, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_SYSTEM ) ? L"FILE_ATTRIBUTE_SYSTEM, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_DIRECTORY ) ? L"FILE_ATTRIBUTE_DIRECTORY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_ARCHIVE ) ? L"FILE_ATTRIBUTE_ARCHIVE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_DEVICE ) ? L"FILE_ATTRIBUTE_DEVICE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_NORMAL ) ? L"FILE_ATTRIBUTE_NORMAL, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_TEMPORARY ) ? L"FILE_ATTRIBUTE_TEMPORARY, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_SPARSE_FILE ) ? L"FILE_ATTRIBUTE_SPARSE_FILE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_REPARSE_POINT ) ? L"FILE_ATTRIBUTE_REPARSE_POINT, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_COMPRESSED ) ? L"FILE_ATTRIBUTE_COMPRESSED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_OFFLINE ) ? L"FILE_ATTRIBUTE_OFFLINE, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED ) ? L"FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_ENCRYPTED ) ? L"FILE_ATTRIBUTE_ENCRYPTED, " : L"" ),
							( ( fa_flags & FILE_ATTRIBUTE_VIRTUAL ) ? L"FILE_ATTRIBUTE_VIRTUAL" : L"" ) );

		// Remove any trailing ", ".
		if ( size > 1 && ret[ size - 1 ] == L' ' )
		{
			ret[ size - 2 ] = L'\0';
		}
	}

	return ret;
}
