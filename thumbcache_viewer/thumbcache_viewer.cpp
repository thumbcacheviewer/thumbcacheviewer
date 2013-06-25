/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2013 Eric Kutcher

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

// We want to get these objects before the window is shown.

// Object variables
HWND g_hWnd_main = NULL;	// Handle to our main window.
HWND g_hWnd_image = NULL;	// Handle to the image window.
HWND g_hWnd_scan = NULL;	// Handle to our scan window.

HFONT hFont = NULL;			// Handle to our font object.

HICON hIcon_bmp = NULL;		// Handle to the system's .bmp icon.
HICON hIcon_jpg = NULL;		// Handle to the system's .jpg icon.
HICON hIcon_png = NULL;		// Handle to the system's .png icon.

char cmd_line = 0;			// Show the main window and message prompts. -1 = Do nothing, 0 = GUI only, 1 = Command Line and GUI, 2 = Command Line and no GUI (save only).

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;

	// Initialize GDI+.
	Gdiplus::GdiplusStartup( &gdiplusToken, &gdiplusStartupInput, NULL );

	// Blocks our reading thread and various GUI operations.
	InitializeCriticalSection( &pe_cs );

	// Get the default message system font.
	NONCLIENTMETRICS ncm = { NULL };
	ncm.cbSize = sizeof( NONCLIENTMETRICS );
	SystemParametersInfoW( SPI_GETNONCLIENTMETRICS, sizeof( NONCLIENTMETRICS ), &ncm, 0 );

	// Set our global font to the LOGFONT value obtained from the system.
	hFont = CreateFontIndirect( &ncm.lfMessageFont );

	// Get the system icon for each of the three file types.
	SHFILEINFO shfi = { NULL }; 
	SHGetFileInfo( L".bmp", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof( shfi ), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES );
	hIcon_bmp = shfi.hIcon;
	SHGetFileInfo( L".png", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof( shfi ), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES );
	hIcon_png = shfi.hIcon;
	SHGetFileInfo( L".jpg", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof( shfi ), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES );
	hIcon_jpg = shfi.hIcon;

	// Initialize our window class.
	WNDCLASSEX wcex;
	wcex.cbSize			= sizeof( WNDCLASSEX );
	wcex.style          = CS_VREDRAW | CS_HREDRAW;
	wcex.lpfnWndProc    = MainWndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_ICON ) );
	wcex.hCursor        = LoadCursor( NULL, IDC_ARROW );
	wcex.hbrBackground  = ( HBRUSH )( COLOR_WINDOW );
	wcex.lpszMenuName   = NULL;
	wcex.lpszClassName  = L"thumbcache";
	wcex.hIconSm        = LoadIcon( wcex.hInstance, MAKEINTRESOURCE( IDI_APPLICATION ) );

	if ( !RegisterClassEx( &wcex ) )
	{
		MessageBox( NULL, L"Call to RegisterClassEx failed!", PROGRAM_CAPTION, MB_ICONWARNING );
		return 1;
	}

	wcex.lpfnWndProc    = ImageWndProc;
	wcex.lpszClassName  = L"image";

	if ( !RegisterClassEx( &wcex ) )
	{
		MessageBox( NULL, L"Call to RegisterClassEx failed!", PROGRAM_CAPTION, MB_ICONWARNING );
		return 1;
	}

	wcex.lpfnWndProc    = ScanWndProc;
	wcex.lpszClassName  = L"scan";

	if ( !RegisterClassEx( &wcex ) )
	{
		MessageBox( NULL, L"Call to RegisterClassEx failed!", PROGRAM_CAPTION, MB_ICONWARNING );
		return 1;
	}

	g_hWnd_main = CreateWindow( L"thumbcache", PROGRAM_CAPTION, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, ( ( GetSystemMetrics( SM_CXSCREEN ) - MIN_WIDTH ) / 2 ), ( ( GetSystemMetrics( SM_CYSCREEN ) - MIN_HEIGHT ) / 2 ), MIN_WIDTH, MIN_HEIGHT, NULL, NULL, NULL, NULL );

	if ( !g_hWnd_main )
	{
		MessageBox( NULL, L"Call to CreateWindow failed!", PROGRAM_CAPTION, MB_ICONWARNING );
		return 1;
	}

	g_hWnd_image = CreateWindow( L"image", PROGRAM_CAPTION, WS_OVERLAPPEDWINDOW, ( ( GetSystemMetrics( SM_CXSCREEN ) - MIN_WIDTH ) / 2 ), ( ( GetSystemMetrics( SM_CYSCREEN ) - MIN_HEIGHT ) / 2 ), MIN_HEIGHT, MIN_HEIGHT, NULL, NULL, NULL, NULL );

	if ( !g_hWnd_image )
	{
		MessageBox( NULL, L"Call to CreateWindow failed!", PROGRAM_CAPTION, MB_ICONWARNING );
		return 1;
	}

	g_hWnd_scan = CreateWindow( L"scan", L"Map File Paths to Entry Hashes", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_CLIPCHILDREN, ( ( GetSystemMetrics( SM_CXSCREEN ) - MIN_WIDTH ) / 2 ), ( ( GetSystemMetrics( SM_CYSCREEN ) - ( MIN_HEIGHT - 155 ) ) / 2 ), MIN_WIDTH, ( MIN_HEIGHT - 155 ), g_hWnd_main, NULL, NULL, NULL );

	if ( !g_hWnd_scan )
	{
		MessageBox( NULL, L"Call to CreateWindow failed!", PROGRAM_CAPTION, MB_ICONWARNING );
		return 1;
	}

	// See if we have any command-line parameters
	if ( lpCmdLine != NULL && lpCmdLine[ 0 ] != NULL )
	{
		// Count the number of parameters and split them into an array.
		int argCount = 0;
		LPWSTR *szArgList = CommandLineToArgvW( GetCommandLine(), &argCount );
		if ( szArgList != NULL )
		{
			// The first parameter is the path to the executable. Ignore it.
			if ( argCount > 1 )
			{
				// Allocate enough space to hold each parameter. They should all be full paths.
				pathinfo *pi = ( pathinfo * )malloc( sizeof( pathinfo ) );
				pi->offset = 0;
				pi->output_path = NULL;
				pi->filepath = ( wchar_t * )malloc( sizeof( wchar_t ) * ( ( MAX_PATH * ( argCount - 1 ) ) + 1 ) );
				wmemset( pi->filepath, 0, ( ( MAX_PATH * ( argCount - 1 ) ) + 1 ) );

				cmd_line = 1;	// Open the database(s) from the command-line.

				int filepath_offset = 0;
				for ( int i = 1; i < argCount; ++i )
				{
					int filepath_length = wcslen( szArgList[ i ] );

					// See if it's an output parameter.
					if ( filepath_length > 1 && szArgList[ i ][ 0 ] == L'-' && ( szArgList[ i ][ 1 ] == L'o' || szArgList[ i ][ 1 ] == L'O' ) )
					{
						// See if the next parameter exists. We'll assume it's the output directory.
						if ( i + 1 < argCount )
						{
							pi->output_path = _wcsdup( szArgList[ ++i ] );

							cmd_line = 2;	// Save the database(s) from the command-line. Do not display the main window or any prompts.
						}
					}
					else if ( filepath_length > 1 && szArgList[ i ][ 0 ] == L'-' && ( szArgList[ i ][ 1 ] == L'z' || szArgList[ i ][ 1 ] == L'Z' ) )
					{
						hide_blank_entries = true;

						// Put a check next to the menu item. g_hMenu is created in g_hWnd_main.
						CheckMenuItem( g_hMenu, MENU_HIDE_BLANK, MF_CHECKED );
					}
					else	// Copy the paths into the NULL separated filepath.
					{
						wmemcpy_s( pi->filepath + filepath_offset, ( ( MAX_PATH * ( argCount - 1 ) ) + 1 ) - filepath_offset, szArgList[ i ], filepath_length + 1 );
						filepath_offset += ( filepath_length + 1 );
					}
				}

				// Only read the database if there's a file to open.
				if ( pi->filepath[ 0 ] != NULL )
				{
					// filepath will be freed in the thread.
					CloseHandle( ( HANDLE )_beginthreadex( NULL, 0, &read_database, ( void * )pi, 0, NULL ) );
				}
				else
				{
					// Cleanup our parameters structure and exit the program.
					free( pi->output_path );
					free( pi->filepath );
					free( pi );

					cmd_line = -1;

					SendMessage( g_hWnd_main, WM_DESTROY_ALT, 0, 0 );
				}
			}

			// Free the parameter list.
			LocalFree( szArgList );
		}
	}

	if ( cmd_line == 0 || cmd_line == 1 )
	{
		ShowWindow( g_hWnd_main, SW_SHOW );
	}

	// Main message loop:
	MSG msg;
	while ( GetMessage( &msg, NULL, 0, 0 ) > 0 )
	{
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}

	// Delete our font.
	DeleteObject( hFont );

	// Delete our critical section.
	DeleteCriticalSection( &pe_cs );

	// Shutdown GDI+
	Gdiplus::GdiplusShutdown( gdiplusToken );

	return ( int )msg.wParam;
}
