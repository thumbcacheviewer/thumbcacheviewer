/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2012 Eric Kutcher

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

#include "globals.h"

#define FILE_TYPE_BMP	"BM"
#define FILE_TYPE_JPEG	"\xFF\xD8\xFF\xE0"
#define FILE_TYPE_PNG	"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"

HANDLE prompt_mutex = NULL;

bool cancelled_prompt = false;	// User cancelled the prompt.
unsigned int entry_begin = 0;	// Beginning position to start reading.
unsigned int entry_end = 0;		// Ending position to stop reading.

HANDLE shutdown_mutex = NULL;	// Blocks shutdown while a worker thread is active.
bool kill_thread = false;		// Allow for a clean shutdown.

CRITICAL_SECTION pe_cs;			// Queues additional worker threads.
bool in_thread = false;			// Flag to indicate that we're in a worker thread.
bool skip_draw = false;			// Prevents WM_DRAWITEM from accessing listview items while we're removing them.

blank_entries_linked_list *g_be = NULL;	// A list to hold all of the blank entries.

bool scan_memory( HANDLE hFile, unsigned int &offset )
{
	// Allocate a 100 kilobyte chunk of memory to scan. This value is arbitrary.
	char *buf = ( char * )malloc( sizeof( char ) * 102400 );
	char *scan = NULL;
	DWORD read = 0;

	while ( true )
	{
		// Begin reading through the database.
		ReadFile( hFile, buf, sizeof( char ) * 102400, &read, NULL );
		if ( read <= 4 )
		{
			free( buf );
			return false;
		}

		// Binary string search. Look for the magic identifier.
		scan = buf;
		while ( read-- > 4 && memcmp( scan++, "CMMM", 4 ) != 0 )
		{
			++offset;
		}

		// If it's not found, then we'll keep scanning.
		if ( read < 4 )
		{
			// Adjust the offset back 4 characters (in case we truncated the magic identifier when reading).
			SetFilePointer( hFile, offset, NULL, FILE_BEGIN );
			// Keep scanning.
			continue;
		}

		break;
	}

	free( buf );
	return true;
}

// This will allow our main thread to continue while secondary threads finish their processing.
unsigned __stdcall cleanup( void *pArguments )
{
	// This mutex will be released when the thread gets killed.
	shutdown_mutex = CreateSemaphore( NULL, 0, 1, NULL );

	kill_thread = true;	// Causes our secondary threads to cease processing and release the mutex.

	// Wait for any active threads to complete. 5 second timeout in case we miss the release.
	WaitForSingleObject( shutdown_mutex, 5000 );
	CloseHandle( shutdown_mutex );
	shutdown_mutex = NULL;

	// DestroyWindow won't work on a window from a different thread. So we'll send a message to trigger it.
	SendMessage( g_hWnd_main, WM_DESTROY_ALT, 0, 0 );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall remove_items( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;
	
	skip_draw = true;	// Prevent the listview from drawing while freeing lParam values.

	SetWindowText( g_hWnd_main, L"Thumbcache Viewer - Please wait..." );	// Update the window title.
	EnableWindow( g_hWnd_list, FALSE );										// Prevent any interaction with the listview while we're processing.
	SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, TRUE, 0 );					// SetCursor only works from the main thread. Set it to an arrow with hourglass.
	update_menus( true );													// Disable all processing menu items.

	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;

	int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );
	int sel_count = SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 );

	// See if we've selected all the items. We can clear the list much faster this way.
	if ( item_count == sel_count )
	{
		// Go through each item, and free their lParam values. current_fileinfo will get deleted here.
		for ( int i = 0; i < item_count; ++i )
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
			{
				break;
			}

			// We first need to get the lParam value otherwise the memory won't be freed.
			lvi.iItem = i;
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			( ( fileinfo * )lvi.lParam )->si->count--;

			// Remove our shared information from the linked list if there's no more items for this database.
			if ( ( ( fileinfo * )lvi.lParam )->si->count == 0 )
			{
				free( ( ( fileinfo * )lvi.lParam )->si );
			}

			// Free our filename, then fileinfo structure.
			free( ( ( fileinfo * )lvi.lParam )->filename );
			free( ( fileinfo * )lvi.lParam );
		}

		SendMessage( g_hWnd_list, LVM_DELETEALLITEMS, 0, 0 );
	}
	else	// Otherwise, we're going to have to go through each selection one at a time. (SLOOOOOW) Start from the end and work our way to the beginning.
	{
		// Scroll to the first item.
		// This will reduce the time it takes to remove a large selection of items.
		// When we delete the item from the end of the listview, the control won't force a paint refresh (since the item's not likely to be visible)
		SendMessage( g_hWnd_list, LVM_ENSUREVISIBLE, 0, FALSE );

		int *index_array = ( int * )malloc( sizeof( int ) * sel_count );

		lvi.iItem = -1;	// Set this to -1 so that the LVM_GETNEXTITEM call can go through the list correctly.

		// Create an index list of selected items (in reverse order).
		for ( int i = 0; i < sel_count; i++ )
		{
			lvi.iItem = index_array[ sel_count - 1 - i ] = SendMessage( g_hWnd_list, LVM_GETNEXTITEM, lvi.iItem, LVNI_SELECTED );
		}

		for ( int i = 0; i < sel_count; i++ )
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
			{
				break;
			}

			// We first need to get the lParam value otherwise the memory won't be freed.
			lvi.iItem = index_array[ i ];
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			( ( fileinfo * )lvi.lParam )->si->count--;

			// Remove our shared information from the linked list if there's no more items for this database.
			if ( ( ( fileinfo * )lvi.lParam )->si->count == 0 )
			{
				free( ( ( fileinfo * )lvi.lParam )->si );
			}
			
			// Free our filename, then fileinfo structure.
			free( ( ( fileinfo * )lvi.lParam )->filename );
			free( ( fileinfo * )lvi.lParam );

			// Remove the list item.
			SendMessage( g_hWnd_list, LVM_DELETEITEM, index_array[ i ], 0 );
		}

		free( index_array );
	}

	skip_draw = false;	// Allow drawing again.

	update_menus( false );									// Enable the appropriate menu items.
	SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, FALSE, 0 );	// Reset the cursor.
	EnableWindow( g_hWnd_list, TRUE );						// Allow the listview to be interactive. Also forces a refresh to update the item count column.
	SetFocus( g_hWnd_list );								// Give focus back to the listview to allow shortcut keys.
	SetWindowText( g_hWnd_main, PROGRAM_CAPTION );			// Reset the window title.

	// Release the mutex if we're killing the thread.
	if ( shutdown_mutex != NULL )
	{
		ReleaseSemaphore( shutdown_mutex, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall show_hide_items( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	SetWindowText( g_hWnd_main, L"Thumbcache Viewer - Please wait..." );	// Update the window title.
	EnableWindow( g_hWnd_list, FALSE );										// Prevent any interaction with the listview while we're processing.
	SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, TRUE, 0 );					// SetCursor only works from the main thread. Set it to an arrow with hourglass.
	update_menus( true );													// Disable all processing menu items.

	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;

	int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );

	if ( hide_blank_entries == false )	// Display the blank entries.
	{
		// This will reinsert the blank entry at the end of the listview.
		blank_entries_linked_list *be = g_be;
		blank_entries_linked_list *del_be = NULL;
		g_be = NULL;
		while ( be != NULL )
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
			{
				g_be = be;	// Reset the global blank entries list to free in WM_DESTORY.

				break;
			}
			del_be = be;

			// Insert a row into our listview.
			lvi.iItem = item_count++;
			lvi.iSubItem = 0;
			lvi.lParam = ( LPARAM )be->fi;
			SendMessage( g_hWnd_list, LVM_INSERTITEM, 0, ( LPARAM )&lvi );

			be = be->next;
			free( del_be );	// Remove the entry from the linked list. We do this for easy managment in case the user decides to remove an item from the listview.
		}
	}
	else	// Hide the blank entries.
	{
		// Scroll to the first item.
		// This will reduce the time it takes to remove a large selection of items.
		// When we delete the item from the end of the listview, the control won't force a paint refresh (since the item's not likely to be visible)
		SendMessage( g_hWnd_list, LVM_ENSUREVISIBLE, 0, FALSE );

		// Start from the end and work our way to the beginning.
		for ( int i = item_count - 1; i >= 0; --i )
		{
			// Stop processing and exit the thread.
			if ( kill_thread == true )
			{
				break;
			}

			lvi.iItem = i;
			SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

			// If the list item is blank, then add it to the blank entry linked list.
			if ( ( ( fileinfo * )lvi.lParam )->size == 0 )
			{
				blank_entries_linked_list *be = ( blank_entries_linked_list * )malloc( sizeof( blank_entries_linked_list ) );
				be->fi = ( fileinfo * )lvi.lParam;
				be->next = g_be;

				g_be = be;

				// Remove the list item.
				SendMessage( g_hWnd_list, LVM_DELETEITEM, i, 0 );
			}
		}
	}

	update_menus( false );									// Enable the appropriate menu items.
	SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, FALSE, 0 );	// Reset the cursor.
	EnableWindow( g_hWnd_list, TRUE );						// Allow the listview to be interactive. Also forces a refresh to update the item count column.
	SetFocus( g_hWnd_list );								// Give focus back to the listview to allow shortcut keys. 
	SetWindowText( g_hWnd_main, PROGRAM_CAPTION );			// Reset the window title.

	// Release the mutex if we're killing the thread.
	if ( shutdown_mutex != NULL )
	{
		ReleaseSemaphore( shutdown_mutex, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall save_items( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	SetWindowText( g_hWnd_main, L"Thumbcache Viewer - Please wait..." );	// Update the window title.
	EnableWindow( g_hWnd_list, FALSE );										// Prevent any interaction with the listview while we're processing.
	SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, TRUE, 0 );					// SetCursor only works from the main thread. Set it to an arrow with hourglass.
	update_menus( true );													// Disable all processing menu items.

	save_param *save_type = ( save_param * )pArguments;

	wchar_t save_directory[ MAX_PATH ] = { 0 };
	// Get the directory path from the id list.
	SHGetPathFromIDList( save_type->lpiidl, save_directory );
	CoTaskMemFree( save_type->lpiidl );
	
	// Depending on what was selected, get the number of items we'll be saving.
	int save_items = ( save_type->save_all == true ? SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 ) : SendMessage( g_hWnd_list, LVM_GETSELECTEDCOUNT, 0, 0 ) );

	// Retrieve the lParam value from the selected listview item.
	LVITEM lvi = { NULL };
	lvi.mask = LVIF_PARAM;
	lvi.iItem = -1;	// Set this to -1 so that the LVM_GETNEXTITEM call can go through the list correctly.

	// Go through all the items we'll be saving.
	for ( int i = 0; i < save_items; ++i )
	{
		// Stop processing and exit the thread.
		if ( kill_thread == true )
		{
			break;
		}

		lvi.iItem = ( save_type->save_all == true ? i : SendMessage( g_hWnd_list, LVM_GETNEXTITEM, lvi.iItem, LVNI_SELECTED ) );
		SendMessage( g_hWnd_list, LVM_GETITEM, 0, ( LPARAM )&lvi );

		// Skip 0 byte files.
		if ( ( ( fileinfo * )lvi.lParam )->size != 0 )
		{
			// Create a buffer to read in our new bitmap.
			char *save_image = ( char * )malloc( sizeof( char ) * ( ( fileinfo * )lvi.lParam )->size );

			// Attempt to open a file for reading.
			HANDLE hFile = CreateFile( ( ( fileinfo * )lvi.lParam )->si->dbpath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
			if ( hFile != INVALID_HANDLE_VALUE )
			{
				DWORD read = 0;
				// Set our file pointer to the beginning of the database file.
				SetFilePointer( hFile, ( ( fileinfo * )lvi.lParam )->offset, 0, FILE_BEGIN );
				// Read the entire image into memory.
				ReadFile( hFile, save_image, ( ( fileinfo * )lvi.lParam )->size, &read, NULL );
				CloseHandle( hFile );

				// Directory + backslash + filename + extension + NULL character = ( 2 * MAX_PATH ) + 6
				wchar_t fullpath[ ( 2 * MAX_PATH ) + 6 ] = { 0 };
				swprintf_s( fullpath, ( 2 * MAX_PATH ) + 6, L"%s\\%.259s", save_directory, ( ( fileinfo * )lvi.lParam )->filename );

				// Attempt to open a file for saving.
				HANDLE hFile_save = CreateFile( fullpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
				if ( hFile_save != INVALID_HANDLE_VALUE )
				{
					// Write the buffer to our file. Only write what we've read.
					DWORD dwBytesWritten = 0;
					WriteFile( hFile_save, save_image, read, &dwBytesWritten, NULL );

					CloseHandle( hFile_save );
				}

				// See if the path was too long.
				if ( GetLastError() == ERROR_PATH_NOT_FOUND )
				{
					MessageBox( g_hWnd_main, L"One or more files could not be saved. Please check the filename and path.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
				}
			}
			// Free our buffer.
			free( save_image );
		}
	}

	free( save_type );

	update_menus( false );									// Enable the appropriate menu items.
	SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, FALSE, 0 );	// Reset the cursor.
	EnableWindow( g_hWnd_list, TRUE );						// Allow the listview to be interactive.
	SetFocus( g_hWnd_list );								// Give focus back to the listview to allow shortcut keys. 
	SetWindowText( g_hWnd_main, PROGRAM_CAPTION );			// Reset the window title.

	// Release the mutex if we're killing the thread.
	if ( shutdown_mutex != NULL )
	{
		ReleaseSemaphore( shutdown_mutex, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}

unsigned __stdcall read_database( void *pArguments )
{
	// This will block every other thread from entering until the first thread is complete.
	// Protects our global variables.
	EnterCriticalSection( &pe_cs );

	in_thread = true;

	SetWindowText( g_hWnd_main, L"Thumbcache Viewer - Please wait..." );	// Update the window title.
	EnableWindow( g_hWnd_list, FALSE );										// Prevent any interaction with the listview while we're processing.
	SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, TRUE, 0 );					// SetCursor only works from the main thread. Set it to an arrow with hourglass.
	update_menus( true );													// Disable all processing menu items.

	pathinfo *pi = ( pathinfo * )pArguments;

	int fname_length = 0;
	wchar_t *fname = pi->filepath + pi->offset;

	int filepath_length = wcslen( pi->filepath ) + 1;	// Include NULL character.
	
	bool construct_filepath = ( filepath_length > pi->offset ? false : true );

	wchar_t *filepath = NULL;

	// We're going to open each file in the path info.
	do
	{
		// Stop processing and exit the thread.
		if ( kill_thread == true )
		{
			break;
		}

		// Construct the filepath for each file.
		if ( construct_filepath == true )
		{
			fname_length = wcslen( fname ) + 1;	// Include '\' character

			filepath = ( wchar_t * )malloc( sizeof( wchar_t ) * ( filepath_length + fname_length ) );
			swprintf_s( filepath, filepath_length + fname_length, L"%s\\%s", pi->filepath, fname );

			// Move to the next file name.
			fname += fname_length;
		}
		else	// Copy the filepath.
		{
			filepath = ( wchar_t * )malloc( sizeof( wchar_t ) * filepath_length );
			wcscpy_s( filepath, filepath_length, pi->filepath );
		}

		// Attempt to open our database file.
		HANDLE hFile = CreateFile( filepath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		if ( hFile != INVALID_HANDLE_VALUE )
		{
			shared_info *si = NULL;
			DWORD read = 0;
			int item_count = SendMessage( g_hWnd_list, LVM_GETITEMCOUNT, 0, 0 );	// We don't need to call this for each item.

			database_header dh = { 0 };
			ReadFile( hFile, &dh, sizeof( database_header ), &read, NULL );

			// Make sure it's a thumbcache database and the stucture was filled correctly.
			if ( memcmp( dh.magic_identifier, "CMMM", 4 ) != 0 || read != sizeof( database_header ) )
			{
				CloseHandle( hFile );
				free( filepath );

				MessageBox( g_hWnd_main, L"The file is not a thumbcache database.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

				continue;
			}

			// Set the file pointer to the first possible cache entry. (Should be at an offset of 24 bytes)
			// current_position will keep track our our file pointer position before setting the file pointer. (ReadFile sets it as well)
			unsigned int current_position = 24;

			entry_begin = 1;						// By default, we'll start with the first entry.
			entry_end = dh.number_of_cache_entries;	// By default, we'll end with the last entry.

			// If we've hit our display limit, then prompt the user for a range of entries.
			if ( dh.number_of_cache_entries > MAX_ENTRIES )
			{
				// This mutex will be released when the user selects a range of entries.
				prompt_mutex = CreateSemaphore( NULL, 0, 1, NULL );

				cancelled_prompt = false;	// Assume they haven't cancelled the prompt.
				SendMessage( g_hWnd_prompt, WM_PROPAGATE, dh.number_of_cache_entries, 0 );

				// Wait for the user to select the range of entries.
				WaitForSingleObject( prompt_mutex, INFINITE );
				CloseHandle( prompt_mutex );
				prompt_mutex = NULL;

				// If the user cancelled the prompt, then exit the thread. We'll assume they don't want to load the database.
				if ( cancelled_prompt == true )
				{
					CloseHandle( hFile );
					free( filepath );

					continue;
				}
			}

			// If the user selected an ending value that's larger than the number of entries, then set it to the number of entries instead.
			if ( entry_end > dh.number_of_cache_entries )
			{
				entry_end = dh.number_of_cache_entries;
			}

			// Determine whether we're going to offset the entries we list.
			bool offset_beginning = ( entry_begin > 1 ? true : false );

			bool next_file = false;	// Go to the next database file.

			// Go through our database and attempt to extract each cache entry.
			for ( unsigned int i = 1; i <= entry_end; ++i )
			{
				// Stop processing and exit the thread.
				if ( kill_thread == true )
				{
					free( filepath );

					next_file = true;
					break;
				}

				// Set the file pointer to the end of the last cache entry.
				current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );
				if ( current_position == INVALID_SET_FILE_POINTER )
				{
					free( filepath );

					wchar_t msg[ 21 ] = { 0 };
					swprintf_s( msg, 21, L"Invalid cache entry." );
					MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

					next_file = true;
					break;
				}

				void *database_cache_entry = NULL;
				// Determine the type of database we're working with and store its content in the correct structure.
				if ( dh.version == WINDOWS_7 )
				{
					database_cache_entry = ( database_cache_entry_7 * )malloc( sizeof( database_cache_entry_7 ) );
					ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_7 ), &read, NULL );
					
					// Make sure it's a thumbcache database and the stucture was filled correctly.
					if ( read != sizeof( database_cache_entry_7 ) )
					{
						// EOF reached.
						free( database_cache_entry );
						free( filepath );

						next_file = true;
						break;
					}
					else if ( memcmp( ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
					{
						free( database_cache_entry );

						wchar_t msg[ 95 ] = { 0 };
						swprintf_s( msg, 95, L"Invalid cache entry located at %lu bytes.\r\n\r\nDo you want to scan for remaining entries?", current_position );
						if ( MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING | MB_YESNO ) == IDYES )
						{
							// Walk back to the end of the last cache entry.
							current_position = SetFilePointer( hFile, current_position - read, NULL, FILE_BEGIN );

							// If we found the beginning of the entry, attempt to read it again.
							if ( scan_memory( hFile, current_position ) == true )
							{
								--i;
								continue;
							}
						}

						free( filepath );

						next_file = true;
						break;
					}
				}
				else if ( dh.version == WINDOWS_VISTA )
				{
					database_cache_entry = ( database_cache_entry_vista * )malloc( sizeof( database_cache_entry_vista ) );
					ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_vista ), &read, NULL );
					// Make sure it's a thumbcache database and the stucture was filled correctly.
					if ( read != sizeof( database_cache_entry_vista ) )
					{
						// EOF reached.
						free( database_cache_entry );
						free( filepath );

						next_file = true;
						break;
					}
					else if ( memcmp( ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
					{
						free( database_cache_entry );

						wchar_t msg[ 95 ] = { 0 };
						swprintf_s( msg, 95, L"Invalid cache entry located at %lu bytes.\r\n\r\nDo you want to scan for remaining entries?", current_position );
						if ( MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING | MB_YESNO ) == IDYES )
						{
							// Walk back to the end of the last cache entry.
							current_position = SetFilePointer( hFile, current_position - read, NULL, FILE_BEGIN );

							// If we found the beginning of the entry, attempt to read it again.
							if ( scan_memory( hFile, current_position ) == true )
							{
								--i;
								continue;
							}
						}

						free( filepath );

						next_file = true;
						break;
					}
				}
				else if ( dh.version == WINDOWS_8 )
				{
					database_cache_entry = ( database_cache_entry_8 * )malloc( sizeof( database_cache_entry_8 ) );
					ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_8 ), &read, NULL );
					
					// Make sure it's a thumbcache database and the stucture was filled correctly.
					if ( read != sizeof( database_cache_entry_8 ) )
					{
						// EOF reached.
						free( database_cache_entry );
						free( filepath );

						next_file = true;
						break;
					}
					else if ( memcmp( ( ( database_cache_entry_8 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
					{
						free( database_cache_entry );

						wchar_t msg[ 95 ] = { 0 };
						swprintf_s( msg, 95, L"Invalid cache entry located at %lu bytes.\r\n\r\nDo you want to scan for remaining entries?", current_position );
						if ( MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING | MB_YESNO ) == IDYES )
						{
							// Walk back to the end of the last cache entry.
							current_position = SetFilePointer( hFile, current_position - read, NULL, FILE_BEGIN );

							// If we found the beginning of the entry, attempt to read it again.
							if ( scan_memory( hFile, current_position ) == true )
							{
								--i;
								continue;
							}
						}

						free( filepath );

						next_file = true;
						break;
					}
				}
				else	// If this is true, then the file isn't from Vista or 7 and not supported by this program.
				{
					free( filepath );

					MessageBox( g_hWnd_main, L"The file is not supported by this program.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

					next_file = true;
					break;
				}

				// Size of the cache entry.
				unsigned int cache_entry_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->cache_entry_size : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->cache_entry_size : ( ( database_cache_entry_vista * )database_cache_entry )->cache_entry_size ) );

				current_position += cache_entry_size;

				// If we've selected a range of values, then we're going to offset the file pointer.
				if ( offset_beginning == true )
				{
					// If we've reached the beginning of the range, then we're no longer going to offset the file pointer.
					if ( ( i + 1 ) == entry_begin )
					{
						offset_beginning = false;
					}

					// Free each database entry that we've skipped over.
					free( database_cache_entry );

					continue;
				}
				
				// Filename length should be the total number of bytes (excluding the NULL character) that the UTF-16 filename takes up. A realistic limit should be twice the size of MAX_PATH.
				unsigned int filename_length = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->filename_length : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->filename_length : ( ( database_cache_entry_vista * )database_cache_entry )->filename_length ) );

				// Skip blank filenames.
				if ( filename_length == 0 )
				{
					// Free each database entry that we've skipped over.
					free( database_cache_entry );

					continue;
				}

				// Since the database can store CLSIDs that extend beyond MAX_PATH, we'll have to set a larger truncation length. A length of 32767 would probably never be seen. 
				unsigned int filename_truncate_length = min( filename_length, ( sizeof( wchar_t ) * SHRT_MAX ) );
				
				// UTF-16 filename. Allocate the filename length plus 6 for the unicode extension and null character. This will get deleted before MainWndProc is destroyed. See WM_DESTROY in MainWndProc.
				wchar_t *filename = ( wchar_t * )malloc( filename_truncate_length + ( sizeof( wchar_t ) * 6 ) );
				memset( filename, 0, filename_truncate_length + ( sizeof( wchar_t ) * 6 ) );
				ReadFile( hFile, filename, filename_truncate_length, &read, NULL );
				if ( read == 0 )
				{
					free( filename );
					free( database_cache_entry );
					free( filepath );
					
					wchar_t msg[ 49 ] = { 0 };
					swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
					MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

					next_file = true;
					break;
				}

				unsigned int file_position = 0;

				// Adjust our file pointer if we truncated the filename. This really shouldn't happen unless someone tampered with the database, or it became corrupt.
				if ( filename_length > filename_truncate_length )
				{
					// Offset the file pointer and see if we've moved beyond the EOF.
					file_position = SetFilePointer( hFile, filename_length - filename_truncate_length, 0, FILE_CURRENT );
					if ( file_position == INVALID_SET_FILE_POINTER )
					{
						free( filename );
						free( database_cache_entry );
						free( filepath );

						wchar_t msg[ 49 ] = { 0 };
						swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
						MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
						
						next_file = true;
						break;
					}
				}

				// Padding before the data entry.
				unsigned int padding_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->padding_size : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->padding_size : ( ( database_cache_entry_vista * )database_cache_entry )->padding_size ) );

				// This will set our file pointer to the beginning of the data entry.
				file_position = SetFilePointer( hFile, padding_size, 0, FILE_CURRENT );
				if ( file_position == INVALID_SET_FILE_POINTER )
				{
					free( filename );
					free( database_cache_entry );
					free( filepath );

					wchar_t msg[ 49 ] = { 0 };
					swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
					MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

					next_file = true;
					break;
				}

				// Size of our image.
				unsigned int data_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_size : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->data_size : ( ( database_cache_entry_vista * )database_cache_entry )->data_size ) );

				// Create a new info structure to send to the listview item's lParam value.
				fileinfo *fi = ( fileinfo * )malloc( sizeof( fileinfo ) );
				fi->offset = file_position;
				fi->size = data_size;

				fi->entry_hash = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->entry_hash : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->entry_hash : ( ( database_cache_entry_vista * )database_cache_entry )->entry_hash ) );
				fi->data_checksum = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_checksum : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->data_checksum : ( ( database_cache_entry_vista * )database_cache_entry )->data_checksum ) );
				fi->header_checksum = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->header_checksum : ( ( dh.version == WINDOWS_8 ) ? ( ( database_cache_entry_8 * )database_cache_entry )->header_checksum : ( ( database_cache_entry_vista * )database_cache_entry )->header_checksum ) );

				// Read any data that exists and get its file extension.
				if ( data_size != 0 )
				{
					// Retrieve the data content header. Our longest identifier is 8 bytes.
					char *buf = ( char * )malloc( sizeof( char ) * 8 );
					ReadFile( hFile, buf, 8, &read, NULL );
					if ( read == 0 )
					{
						free( buf );
						free( fi );
						free( filename );
						free( database_cache_entry );
						free( filepath );

						wchar_t msg[ 49 ] = { 0 };
						swprintf_s( msg, 49, L"Invalid cache entry located at %lu bytes.", current_position );
						MessageBox( g_hWnd_main, msg, PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );

						next_file = true;
						break;
					}

					// Detect the file extension and copy it into the filename string.
					if ( memcmp( buf, FILE_TYPE_BMP, 2 ) == 0 )			// First 3 bytes
					{
						wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".bmp", 4 );
						fi->extension = 0;
					}
					else if ( memcmp( buf, FILE_TYPE_JPEG, 4 ) == 0 )	// First 4 bytes
					{
						wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".jpg", 4 );
						fi->extension = 1;
					}
					else if ( memcmp( buf, FILE_TYPE_PNG, 8 ) == 0 )	// First 8 bytes
					{
						wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".png", 4 );
						fi->extension = 2;
					}
					else if ( dh.version == WINDOWS_VISTA && ( ( database_cache_entry_vista * )database_cache_entry )->extension[ 0 ] != NULL )	// If it's a Windows Vista thumbcache file and we can't detect the extension, then use the one given.
					{
						wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 1, L".", 1 );
						wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ) + 1, 4, ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 );
						fi->extension = 3;	// Unknown extension
					}

					// Free our data buffer.
					free( buf );

					// This will set our file pointer to the end of the data entry.
					SetFilePointer( hFile, data_size - 8, 0, FILE_CURRENT );
				}
				else	// No data exists.
				{
					// Windows Vista thumbcache files should include the extension.
					if ( dh.version == WINDOWS_VISTA && ( ( database_cache_entry_vista * )database_cache_entry )->extension[ 0 ] != NULL )
					{
						wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 1, L".", 1 );
						wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ) + 1, 4, ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 );
					}

					fi->extension = 3;	// Unknown extension
				}

				fi->filename = filename;	// Gets deleted during shutdown.

				// Do this only once for each database, and only if we have an entry to add to the listview.
				if ( si == NULL )
				{
					// This information is shared between entries within the database.
					si = ( shared_info * )malloc( sizeof( shared_info ) );
					si->count = 0;
					si->system = dh.version;
					wcscpy_s( si->dbpath, MAX_PATH, filepath );
				}

				// Increase the number of items for our shared information.
				si->count++;

				// The operating system and database location is shared among each entry for the database. This will reduce the amount of memory used.
				fi->si = si;

				// Add blank entries to our blank entries linked list.
				if ( hide_blank_entries == true && fi->size == 0 )
				{
					blank_entries_linked_list *be = ( blank_entries_linked_list * )malloc( sizeof( blank_entries_linked_list ) );
					be->fi = fi;
					be->next = g_be;

					g_be = be;
				}
				else
				{
					// Insert a row into our listview.
					LVITEM lvi = { NULL };
					lvi.mask = LVIF_PARAM; // Our listview items will display the text contained the lParam value.
					lvi.iItem = item_count++;
					lvi.iSubItem = 0;
					lvi.lParam = ( LPARAM )fi;
					SendMessage( g_hWnd_list, LVM_INSERTITEM, 0, ( LPARAM )&lvi );
				}

				// Free our database cache entry.
				free( database_cache_entry );
			}
			// Close the input file.
			CloseHandle( hFile );

			if ( next_file == true )
			{
				continue;
			}
		}
		else
		{
			// If this occurs, then there's something wrong with the user's system.
			MessageBox( g_hWnd_main, L"The database file failed to open.", PROGRAM_CAPTION, MB_APPLMODAL | MB_ICONWARNING );
		}

		// Free the old filepath.
		free( filepath );
	}
	while ( construct_filepath == true && *fname != L'\0' );

	// Free the path info.
	free( pi->filepath );
	free( pi );

	update_menus( false );									// Enable the appropriate menu items.
	SendMessage( g_hWnd_main, WM_CHANGE_CURSOR, FALSE, 0 );	// Reset the cursor.
	EnableWindow( g_hWnd_list, TRUE );						// Allow the listview to be interactive.
	SetFocus( g_hWnd_list );								// Give focus back to the listview to allow shortcut keys. 
	SetWindowText( g_hWnd_main, PROGRAM_CAPTION );			// Reset the window title.

	// Release the mutex if we're killing the thread.
	if ( shutdown_mutex != NULL )
	{
		ReleaseSemaphore( shutdown_mutex, 1, NULL );
	}

	in_thread = false;

	// We're done. Let other threads continue.
	LeaveCriticalSection( &pe_cs );

	_endthreadex( 0 );
	return 0;
}
