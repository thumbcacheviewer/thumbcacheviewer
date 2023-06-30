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

// Extensible Storage Engine library.
#pragma comment( lib, "esent.lib" )

#define JET_VERSION 0x0501
#include <esent.h>

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void load_esedb_info( wchar_t *filepath );
void map_esedb_hash( unsigned long long hash, bool output_html, HANDLE hFile_html );
void cleanup_esedb_info();

#endif
