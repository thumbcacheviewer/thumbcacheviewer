/*
    thumbcache_viewer will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2016 Eric Kutcher

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
#include "utilities.h"

#include <stdio.h>

#define IDT_TIMER		2001

#define MAX_SCALE_SIZE	25.0f

// Window variables
int cx_i = 0;				// Current x (left) position of the image window based on the mouse.
int cy_i = 0;				// Current y (top) position of the image window based on the mouse.

bool mouse_r_down = false;	// Toggled when the right mouse button is down.
bool mouse_l_down = false;	// Toggled when the right mouse button is down.

// Image variables
PAINTSTRUCT ps = { NULL };	// The paint structure used to draw on the image window.
float scale = 1.0f;			// Scale of the image.

POINT drag_rect = { 0 };	// The current position of gdi_image in the image window.
POINT old_pos = { 0 };		// The old position of gdi_image. Used to calculate the rate of change.

// Timer variables.
bool zoom = false;			// Toggled when we want to activate the timer and display the zoom text.
bool timer_active = false;	// Toggled when the timer is active and used to reset it.

LRESULT CALLBACK ImageWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch ( msg )
    {
		case WM_KEYDOWN:
		{
			switch ( wParam )
			{
				case VK_HOME:
				{
					RECT rc;
					GetClientRect( hWnd, &rc );

					// Center the image.
					drag_rect.x = ( ( long )gdi_image->GetWidth() - rc.right ) / 2;
					drag_rect.y = ( ( long )gdi_image->GetHeight() - rc.bottom ) / 2;

					// Redraw our image.
					InvalidateRect( hWnd, NULL, TRUE );
				}
				break;

				case VK_OEM_PLUS:
				{
					// Zoom in.
					scale += 0.5f;
					if ( scale < 1.0f )
					{
						scale = 1.0f;
					}
					else if ( scale > MAX_SCALE_SIZE )
					{
						scale = MAX_SCALE_SIZE;
					}

					timer_active = false;	// Allow the timer to reset.
					zoom = true;

					// Redraw our image.
					InvalidateRect( hWnd, NULL, TRUE );
					
				}
				break;

				case VK_OEM_MINUS:
				{
					// Zoom out.
					scale -= 0.5f;
					if ( scale < 1.0f )
					{
						scale = 1.0f;
					}
					else if ( scale > MAX_SCALE_SIZE )
					{
						scale = MAX_SCALE_SIZE;
					}

					timer_active = false;	// Allow the timer to reset.
					zoom = true;

					// Redraw our image.
					InvalidateRect( hWnd, NULL, TRUE );
				}
				break;

				case VK_LEFT:
				{
					// See if the control or shift key is down.
					if ( GetKeyState( VK_SHIFT ) & 0x8000 )
					{
						drag_rect.x += 25;
					}
					else if ( GetKeyState( VK_CONTROL ) & 0x8000 )
					{
						drag_rect.x += 5;
					}
					else
					{
						drag_rect.x += 1;
					}

					// Prevent the image from going more than its width offscreen.
					float left = ( gdi_image->GetWidth() * scale ) - ( ( gdi_image->GetWidth() / 2 ) * ( scale - 1 ) );
					if ( drag_rect.x >= left )
					{
						drag_rect.x = ( long )left;
					}

					// Redraw our image.
					InvalidateRect( hWnd, NULL, TRUE );
				}
				break;

				case VK_RIGHT:
				{
					RECT rc;
					GetClientRect( hWnd, &rc );

					// See if the control or shift key is down.
					if ( GetKeyState( VK_SHIFT ) & 0x8000 )
					{
						drag_rect.x -= 25;
					}
					else if ( GetKeyState( VK_CONTROL ) & 0x8000 )
					{
						drag_rect.x -= 5;
					}
					else
					{
						drag_rect.x -= 1;
					}

					// Prevent the image from going more than its width offscreen.
					float right = rc.right + ( ( gdi_image->GetWidth() / 2 ) * ( scale - 1 ) );
					if ( -drag_rect.x >=  right )
					{
						drag_rect.x = ( long )-right;
					}

					// Redraw our image.
					InvalidateRect( hWnd, NULL, TRUE );
				}
				break;

				case VK_UP:
				{
					// See if the control or shift key is down.
					if ( GetKeyState( VK_SHIFT ) & 0x8000 )
					{
						drag_rect.y += 25;
					}
					else if ( GetKeyState( VK_CONTROL ) & 0x8000 )
					{
						drag_rect.y += 5;
					}
					else
					{
						drag_rect.y += 1;
					}

					// Prevent the image from going more than its height offscreen.
					float top = ( gdi_image->GetHeight() * scale ) - ( ( gdi_image->GetHeight() / 2 ) * ( scale - 1 ) );
					if ( drag_rect.y >= top )
					{
						drag_rect.y = ( long )top;
					}

					// Redraw our image.
					InvalidateRect( hWnd, NULL, TRUE );
				}
				break;

				case VK_DOWN:
				{
					RECT rc;
					GetClientRect( hWnd, &rc );
				
					// See if the control or shift key is down.
					if ( GetKeyState( VK_SHIFT ) & 0x8000 )
					{
						drag_rect.y -= 25;
					}
					else if ( GetKeyState( VK_CONTROL ) & 0x8000 )
					{
						drag_rect.y -= 5;
					}
					else
					{
						drag_rect.y -= 1;
					}

					// Prevent the image from going more than its height offscreen.
					float bottom = rc.bottom + ( ( gdi_image->GetHeight() / 2 ) * ( scale - 1 ) );
					if ( -drag_rect.y >= bottom )
					{
						drag_rect.y = ( long )-bottom;
					}

					// Redraw our image.
					InvalidateRect( hWnd, NULL, TRUE );
				}
				break;
			}
			return 0;
		}
		break;

		case WM_MOUSEWHEEL:
		{
			// Increment, or decrement our scale based on the scrollwheel change.
			scale += 0.5f * ( GET_WHEEL_DELTA_WPARAM( wParam ) / WHEEL_DELTA );
			if ( scale < 1.0f )
			{
				scale = 1.0f;
			}
			else if ( scale > MAX_SCALE_SIZE )
			{
				scale = MAX_SCALE_SIZE;
			}

			timer_active = false;	// Allow the timer to reset.
			zoom = true;

			// Redraw our image.
			InvalidateRect( hWnd, NULL, TRUE );

			return 0;
		}
		break;

		case WM_MOUSEMOVE:
		{
			if ( wParam == MK_LBUTTON )
			{
				// Set our image position based on the rate of change between mouse movements.
				drag_rect.x += old_pos.x - LOWORD( lParam );
				drag_rect.y += old_pos.y - HIWORD( lParam );

				// This will make sure that the image can't go offscreen by more pixels than its width.
				float left = ( gdi_image->GetWidth() * scale ) - ( ( gdi_image->GetWidth() / 2 ) * ( scale - 1 ) );
				if ( drag_rect.x >= left )
				{
					drag_rect.x = ( long )left;
				}

				// This will make sure that the image can't go offscreen by more pixels than its height.
				float top = ( gdi_image->GetHeight() * scale ) - ( ( gdi_image->GetHeight() / 2 ) * ( scale - 1 ) );
				if ( drag_rect.y >= top )
				{
					drag_rect.y = ( long )top;
				}

				RECT rc;
				GetClientRect( hWnd, &rc );
				// This will make sure that the image can't go offscreen by more pixels than its width.
				float right = rc.right + ( ( gdi_image->GetWidth() / 2 ) * ( scale - 1 ) );
				if ( -drag_rect.x >=  right )
				{
					drag_rect.x = ( long )-right;
				}

				// This will make sure that the image can't go offscreen by more pixels than its height.
				float bottom = rc.bottom + ( ( gdi_image->GetHeight() / 2 ) * ( scale - 1 ) );
				if ( -drag_rect.y >= bottom )
				{
					drag_rect.y = ( long )-bottom;
				}

				// Redraw our image.
				InvalidateRect( hWnd, NULL, TRUE );

				// Set our old mouse position.
				old_pos.x = LOWORD( lParam );
				old_pos.y = HIWORD( lParam );
			}
			return 0;
		}
		break;

		case WM_LBUTTONDOWN:
		{
			mouse_l_down = true;

			// See if the right mouse button is down.
			if ( mouse_r_down == true )
			{
				// Zoom in.
				scale += 0.5f;
				if ( scale < 1.0f )
				{
					scale = 1.0f;
				}
				else if ( scale > MAX_SCALE_SIZE )
				{
					scale = MAX_SCALE_SIZE;
				}

				timer_active = false;	// Allow the timer to reset.
				zoom = true;

				// Redraw our image.
				InvalidateRect( hWnd, NULL, TRUE );

				// Don't save our current position.
				break;
			}

			// Save our initial mouse down position.
			old_pos.x = LOWORD( lParam );
			old_pos.y = HIWORD( lParam );

			return 0;
		}
		break;

		case WM_LBUTTONUP:
		{
			mouse_l_down = false;
			return 0;
		}
		break;

		case WM_RBUTTONDOWN:
		{
			mouse_r_down = true;

			// See if the left mouse button is down.
			if ( mouse_l_down == true )
			{
				// Zoom out.
				scale -= 0.5f;
				if ( scale < 1.0f )
				{
					scale = 1.0f;
				}
				else if ( scale > MAX_SCALE_SIZE )
				{
					scale = MAX_SCALE_SIZE;
				}

				timer_active = false;	// Allow the timer to reset.
				zoom = true;

				// Redraw our image.
				InvalidateRect( hWnd, NULL, TRUE );
			}
			return 0;
		}
		break;

		case WM_RBUTTONUP:
		{
			mouse_r_down = false;
			return 0;
		}
		break;

		case WM_MBUTTONDOWN:
		{
			RECT rc;
			GetClientRect( hWnd, &rc );

			// Center the image.
			drag_rect.x = ( ( long )gdi_image->GetWidth() - rc.right ) / 2;
			drag_rect.y = ( ( long )gdi_image->GetHeight() - rc.bottom ) / 2;

			// Force our window to repaint itself.
			InvalidateRect( hWnd, NULL, TRUE );
			return 0;
		}
		break;

		case WM_ERASEBKGND:
		{
			// We'll handle the background drawing.
			return TRUE;
		}
		break;

		case WM_PAINT:
		{
			HDC hDC = BeginPaint( hWnd, &ps );

			// Make sure we have an image to draw.
			if ( gdi_image != NULL )
			{
				RECT rc;
				GetClientRect( hWnd, &rc );

				// Create a memory buffer to draw to.
				HDC hdcMem = CreateCompatibleDC( hDC );

				// Create a bitmap in our memory buffer that's the size of our window.
				HBITMAP hbm = CreateCompatibleBitmap( hDC, rc.right, rc.bottom );
				HBITMAP ohbm = ( HBITMAP )SelectObject( hdcMem, hbm );
				DeleteObject( ohbm );
				DeleteObject( hbm );

				// Fill the memory background with the menu color
				HBRUSH background = CreateSolidBrush( ( COLORREF )GetSysColor( COLOR_MENU ) );
				FillRect( hdcMem, &rc, background );
				DeleteObject( background );

				// Create a graphics object for our memory buffer.
				Gdiplus::Graphics graphics( hdcMem );
					
				// Draw the image on screen.
				if ( scale > 1.0f )
				{
					// Scale the image.
					graphics.SetInterpolationMode( Gdiplus::InterpolationModeNearestNeighbor );
					graphics.ScaleTransform( scale, scale );
					// Whoa! This draws the image around its scaled center.
					graphics.DrawImage( gdi_image, ( ( -drag_rect.x - ( ( gdi_image->GetWidth() / 2 ) * ( scale - 1 ) ) ) / scale ), ( ( -drag_rect.y - ( ( gdi_image->GetHeight() / 2 ) * ( scale - 1 ) ) ) / scale ) );
				}
				else
				{
					graphics.DrawImage( gdi_image, -drag_rect.x, -drag_rect.y );
				}

				// If the image has just been zoomed, display some text about it.
				if ( zoom == true )
				{
					// Prevents InvalidateRect calls from interfering with an active timer.
					if ( timer_active == false )
					{
						timer_active = true;
						SetTimer( hWnd, IDT_TIMER, 5000, ( TIMERPROC )TimerProc );
					}

					char buf[ 20 ] = { 0 };
					sprintf_s( buf, 20, " Zoom level: %4.1fx ", scale );

					HFONT ohf = ( HFONT )SelectObject( hdcMem, hFont );
					DeleteObject( ohf );

					SetBkColor( hdcMem, ( COLORREF )GetSysColor( COLOR_MENU ) );
					SetTextColor( hdcMem, RGB( 0x00, 0x00, 0x00 ) );
					DrawTextA( hdcMem, buf, -1, &rc, DT_SINGLELINE );
				}

				// Draw our memory buffer to the main device context.
				BitBlt( hDC, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY );
				DeleteDC( hdcMem );
			}

			EndPaint( hWnd, &ps );

			return 0;
		}
		break;

		case WM_GETMINMAXINFO:
		{
			// Set the minimum dimensions that the window can be sized to.
			( ( MINMAXINFO * )lParam )->ptMinTrackSize.x = MIN_HEIGHT;
			( ( MINMAXINFO * )lParam )->ptMinTrackSize.y = MIN_HEIGHT;

			return 0;
		}
		break;

		case WM_SYSCOMMAND:
		{
			// Capture the minimize call before the window gets minimized.
			if ( wParam == SC_MINIMIZE )
			{
				// Save its dimensions.
				GetClientRect( hWnd, &last_dim );
			}
			// Continue processing this, and all other system calls.
			return DefWindowProc( hWnd, msg, wParam, lParam );
		}
		break;

		case WM_ENTERSIZEMOVE:
		{
			//Get the current position of our window before it gets moved.
			RECT rc;
			GetWindowRect( hWnd, &rc );

			POINT cur_pos;
			GetCursorPos( &cur_pos );
			cx_i = cur_pos.x - rc.left;
			cy_i = cur_pos.y - rc.top;

			return 0;
		}
		break;

		case WM_SIZE:
		{
			if ( wParam == SIZE_RESTORED )
			{
				// This will handle the case when the user moves the main window while the image window is minimized.
				if ( is_attached == true )
				{
					RECT rc;
					GetWindowRect( hWnd, &rc );
					// Call the WM_MOVING callback with the current position of our window. Set wParam to -1 to tell WM_MOVING that we don't want to calculate the cursor position.
					SendMessage( hWnd, WM_MOVING, -1, ( LPARAM )&rc );
				}
				break;
			}

			if ( wParam == SIZE_MAXIMIZED )
			{
				// We're no longer attached to the main window if we're maximized.
				is_attached = false;
				skip_main = false;
			}
			return 0;
		}
		break;

		case WM_MOVING:
		{
			POINT cur_pos;
			RECT wa;
			RECT *rc = ( RECT * )lParam;

			// WM_MOVING doesn't use wParam, but we set it in the WM_SIZE callback above so that rc isn't influenced by the cursor position.
			if ( wParam != -1 )
			{
				GetCursorPos( &cur_pos );
				OffsetRect( rc, cur_pos.x - ( rc->left + cx_i ), cur_pos.y - ( rc->top + cy_i ) );
			}

			GetWindowRect( g_hWnd_main, &wa );

			is_attached = false;
			if( is_close( rc->right, wa.left ) )		// Attach to left side of main window.
			{
				// Allow it to snap only to the dimensions of the main window.
				if ( ( rc->bottom > wa.top ) && ( rc->top < wa.bottom ) )
				{
					OffsetRect( rc, wa.left - rc->right, 0 );
					is_attached = true;
				}
			}
			else if ( is_close( wa.right, rc->left ) )	// Attach to right side of main window.
			{
				// Allow it to snap only to the dimensions of the main window.
				if ( ( rc->bottom > wa.top ) && ( rc->top < wa.bottom ) )
				{
					OffsetRect( rc, wa.right - rc->left, 0 );
					is_attached = true;
				}
			}

			if( is_close( rc->bottom, wa.top ) )		// Attach to top of main window.
			{
				// Allow it to snap only to the dimensions of the main window.
				if ( ( rc->left < wa.right ) && ( rc->right > wa.left ) )
				{
					OffsetRect( rc, 0, wa.top - rc->bottom );
					is_attached = true;
				}
			}
			else if ( is_close( wa.bottom, rc->top ) )	// Attach to bottom of main window.
			{
				// Allow it to snap only to the dimensions of the main window.
				if ( ( rc->left < wa.right ) && ( rc->right > wa.left ) )
				{
					OffsetRect( rc, 0, wa.bottom - rc->top );
					is_attached = true;
				}
			}

			// Allow our image window to attach to the edge of the desktop, but only if it's not already attached to the main window.
			if ( is_attached == false )
			{
				// The image window is no longer attached, so the main window can test if it attaches to this window.
				skip_main = false;

				SystemParametersInfo( SPI_GETWORKAREA, 0, &wa, 0 );

				if( is_close( rc->left, wa.left ) )			// Attach to left side of the desktop.
				{
					OffsetRect( rc, wa.left - rc->left, 0 );
				}
				else if ( is_close( wa.right, rc->right ) )	// Attach to right side of the desktop.
				{
					OffsetRect( rc, wa.right - rc->right, 0 );
				}

				if( is_close( rc->top, wa.top ) )				// Attach to top of the desktop.
				{
					OffsetRect( rc, 0, wa.top - rc->top );
				}
				else if ( is_close( wa.bottom, rc->bottom ) )	// Attach to bottom of the desktop.
				{
					OffsetRect( rc, 0, wa.bottom - rc->bottom );
				}
			}
			return TRUE;
		}
		break;

		case WM_CLOSE:
		{
			// We're no longer attached to the main window if we're closed.
			is_attached = false;
			skip_main = false;

			// Hide the window if we lose focus.
			ShowWindow( hWnd, SW_HIDE );
			return 0;
		}
		break;

		default:
		{
			return DefWindowProc( hWnd, msg, wParam, lParam );
		}
		break;
	}
	return TRUE;
}

VOID CALLBACK TimerProc( HWND hWnd, UINT msg, UINT idTimer, DWORD dwTime )
{ 
	zoom = false;

	// Redraw our image.
	InvalidateRect( hWnd, NULL, TRUE );

	// Stop our timer.
	timer_active = false;
	KillTimer( hWnd, IDT_TIMER );
}
