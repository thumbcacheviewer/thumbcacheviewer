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

#ifndef GLOBALS_H
#define GLOBALS_H

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

// Information retrieved from Windows.edb.
struct EXTENDED_INFO
{
	void *si;					// Shared information between items. (Cast this to SHARED_EXTENDED_INFO for SQLite or COLUMN_INFO for ESE)
	wchar_t *property_value;	// Converted data value.
	EXTENDED_INFO *next;
};

// This structure holds information obtained as we read the database.
struct FILE_INFO
{
	EXTENDED_INFO *ei;
	unsigned long long entry_hash;
	unsigned long index;	// Row index in ESE database.
};

struct LINKED_LIST
{
	FILE_INFO fi;
	LINKED_LIST *next;
};

#endif
