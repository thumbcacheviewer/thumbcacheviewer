/*
	thumbcache_viewer will extract thumbnail images from thumbcache database files.
	Copyright (C) 2011-2021 Eric Kutcher

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

#ifndef MAP_ENTRIES_H
#define MAP_ENTRIES_H

unsigned __stdcall map_entries( void *pArguments );

extern wchar_t g_filepath[];					// Path to the files and folders to scan.
extern wchar_t g_extension_filter[];			// A list of extensions to filter from a file scan.

extern bool g_include_folders;					// Include folders in a file scan.
extern bool g_retrieve_extended_information;	// Retrieve additional columns from Windows.edb
extern bool g_show_details;						// Show details in the scan window.

#endif
