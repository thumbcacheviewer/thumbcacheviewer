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

#ifndef LITE_MSSRCH_H
#define LITE_MSSRCH_H

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MSSRCH_STATE_SHUTDOWN	0
#define MSSRCH_STATE_RUNNING	1

// in:		_Src:		A wide character string (NULL terminated) to compress.
// out:		_Dst:		A binary buffer to hold the compressed output (is not NULL terminated).
// in:		_DstSize:	Size in bytes of the _Dst buffer.
//
// On success: Returns the length in bytes of the compressed value in _Dst.
//
// Remarks: This function is not safe. _Src and _Dst must not be NULL, and _DstSize must not be 0.
//
//typedef int ( WINAPI *pMSSCompressText )( wchar_t *_Src, unsigned char *_Dst, size_t _DstSize );

// inout:	_Src:		A binary buffer to uncompress (not NULL terminated). Gets unobfuscated in-place.
// in:		_SrcSize:	Size in bytes of the _Src buffer.
// out:		_Dst:		A wide character buffer to hold the uncompressed output (is not NULL terminated).
// in:		_DstSize:	Size in bytes of the _Dst buffer.
//
// On success: Returns the length in bytes of the uncompressed value in _Dst.
// On failure: Returns -1. _Src and _Dst will remain unchanged.
//
// Remarks: If _DstSize is too small, then _Src will remain unchanged and the function will return the length in bytes of what the uncompressed value would be.
// Remarks: This function is not safe. _Src must not be NULL, and _Dst must not be NULL while _DstSize is greater than 0.
//
typedef int ( WINAPI *pMSSUncompressText )( unsigned char *_Src, size_t _SrcSize, wchar_t *_Dst, size_t _DstSize );

//extern pMSSCompressText		MSSCompressText;
extern pMSSUncompressText	MSSUncompressText;

extern unsigned char mssrch_state;

bool InitializeMsSrch();
bool UnInitializeMsSrch();

#endif
