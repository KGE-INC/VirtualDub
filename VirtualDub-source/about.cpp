//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <math.h>
#include <windows.h>
#include <vfw.h>

#include "resource.h"
#include "vbitmap.h"
#include "auxdlg.h"
#include "oshelper.h"

#include "IAmpDecoder.h"

extern "C" unsigned long version_num;
extern "C" char version_time[];

extern HINSTANCE g_hInst;

///////////////////////////////////////////////////////////////////////////

static HDC g_hdcAboutDisplay;
static HBITMAP g_hbmAboutDisplay;
static HGDIOBJ g_hgoAboutDisplay;
static BITMAPINFOHEADER g_bihAboutDisplay;
static void *g_pvAboutDisplay;
static void *g_pvAboutDisplayBack;
static VBitmap g_vbAboutSrc, g_vbAboutDst;

///////////////////////////////////////////////////////////////////////////

struct TriPt {
	double x, y;
	double u, v;
};

typedef unsigned short Pixel16;

// Triangle setup based on Chris Hecker's GDM article on texture mapping.
// We define pixel centers on the display to be at integers.

void RenderTriangle(Pixel16 *dst, long dstpitch, Pixel16 *tex, TriPt *pt1, TriPt *pt2, TriPt *pt3) {
	TriPt *pt, *pl, *pr;

	// Find top point

	if (pt1->y < pt2->y)		// 1 < 2
		if (pt1->y < pt3->y) {	// 1 < 2,3
			pt = pt1;
			pr = pt2;
			pl = pt3;
		} else {				// 3 < 1 < 2
			pt = pt3;
			pr = pt1;
			pl = pt2;
		}
	else						// 2 < 1
		if (pt2->y < pt3->y) {	// 2 < 1,3
			pt = pt2;
			pr = pt3;
			pl = pt1;
		} else {				// 3 < 2 < 1
			pt = pt3;
			pr = pt1;
			pl = pt2;
		}

	if (pl->y == pt->y && pt->y == pr->y)
		return;

	// Compute gradients

	double A;
	double one_over_A;
	double dudx, dvdx;
	double dudy, dvdy;
	int dudxi, dvdxi;

	A = (pt->y - pl->y) * (pr->x - pl->x) - (pt->x - pl->x) * (pr->y - pl->y);
	one_over_A = 1.0 / A;
	dudx = ((pr->u - pl->u) * (pt->y - pl->y) - (pt->u - pl->u) * (pr->y - pl->y)) * one_over_A;
	dvdx = ((pr->v - pl->v) * (pt->y - pl->y) - (pt->v - pl->v) * (pr->y - pl->y)) * one_over_A;
	dudy = ((pr->u - pl->u) * (pt->x - pl->x) - (pt->u - pl->u) * (pr->x - pl->x)) * -one_over_A;
	dvdy = ((pr->v - pl->v) * (pt->x - pl->x) - (pt->v - pl->v) * (pr->x - pl->x)) * -one_over_A;

	dudxi = (long)(dudx * 16777216.0);
	dvdxi = (long)(dvdx * 16777216.0);
	
	// Compute edge walking parameters

	double dxl1=0, dxr1=0, dul1=0, dvl1=0;
	double dxl2=0, dxr2=0, dul2=0, dvl2=0;

	// Compute left-edge interpolation parameters for first half.

	if (pl->y != pt->y) {
		dxl1 = (pl->x - pt->x) / (pl->y - pt->y);

		dul1 = dudy + dxl1 * dudx;
		dvl1 = dvdy + dxl1 * dvdx;
	}

	// Compute right-edge interpolation parameters for first half.

	if (pr->y != pt->y) {
		dxr1 = (pr->x - pt->x) / (pr->y - pt->y);
	}

	// Reject backfaces.

	if (dxl1 >= dxr1)
		return;

	// Compute third-edge interpolation parameters.

	if (pr->y != pl->y) {
		dxl2 = (pr->x - pl->x) / (pr->y - pl->y);

		dul2 = dudy + dxl2 * dudx;
		dvl2 = dvdy + dxl2 * dvdx;

		dxr2 = dxl2;
	}

	// Initialize parameters for first half.
	//
	// We place pixel centers at (x+0.5, y+0.5).

	double xl, xr, ul, vl, yf;
	int y, y1, y2;

	// y_start < y+0.5 to include pixel y.

	y = floor(pt->y + 0.5);
	yf = (y+0.5) - pt->y;

	xl = pt->x + dxl1 * yf;
	xr = pt->x + dxr1 * yf;
	ul = pt->u + dul1 * yf;
	vl = pt->v + dvl1 * yf;

	// Initialize parameters for second half.

	double xl2, xr2, ul2, vl2;

	if (pl->y > pr->y) {		// Left edge is long side
		dxl2 = dxl1;
		dul2 = dul1;
		dvl2 = dvl1;

		y1 = floor(pr->y + 0.5);
		y2 = floor(pl->y + 0.5);

		yf = (y1+0.5) - pr->y;

		// Step left edge.

		xl2 = xl + dxl1 * (y1 - y);
		ul2 = ul + dul1 * (y1 - y);
		vl2 = vl + dvl1 * (y1 - y);

		// Prestep right edge.

		xr2 = pr->x + dxr2 * yf;
	} else {					// Right edge is long side
		dxr2 = dxr1;

		y1 = floor(pl->y + 0.5);
		y2 = floor(pr->y + 0.5);

		yf = (y1+0.5) - pl->y;

		// Prestep left edge.

		xl2 = pl->x + dxl2 * yf;
		ul2 = pl->u + dul2 * yf;
		vl2 = pl->v + dvl2 * yf;

		// Step right edge.

		xr2 = xr + dxr1 * (y1 - y);
	}

	// Rasterize!

	int u_correct=0, v_correct=0;

	if (dudx < 0)			u_correct = -1;
	else if (dudx > 0)		u_correct = 0;
	else if (dul1 < 0)		u_correct = -1;

	if (dvdx < 0)			v_correct = -1;
	else if (dvdx > 0)		v_correct = 0;
	else if (dvl1 < 0)		v_correct = -1;

/*	if (y < 0)
		y = 0;

	if (y2 > 160)
		y2 = 160;*/

	dst += dstpitch * y;

	while(y < y2) {
		if (y == y1) {
			xl = xl2;
			xr = xr2;
			ul = ul2;
			vl = vl2;
			dxl1 = dxl2;
			dxr1 = dxr2;
			dul1 = dul2;
			dvl1 = dvl2;
		}

		int x1, x2;
		double xf;
		int u, v;

		// x_left must be less than (x+0.5) to include pixel x.

		x1		= (int)floor(xl + 0.5);
		x2		= (int)floor(xr + 0.5);
		xf		= (x1+0.5) - xl;
		
		u		= ((int)((ul + xf * dudx)*16777216.0) /*>> mipmaplevel*/) + u_correct;
		v		= ((int)((vl + xf * dvdx)*16777216.0) /*>> mipmaplevel*/) + v_correct;

/*		if (x1<0) {
			x1=0;
			u -= dudxi * x1;
			v -= dvdxi * x1;
		}
		if (x2>160) x2=160;*/

		while(x1 < x2) {
//			dst[x1++] = tex[(u>>24) + (v>>24)*64];
			int A = tex[(u>>24) + (v>>24)*32];
			int B = dst[x1];

			dst[x1] = ((A&0x7bde)>>1) + ((B>>1)&0x3def) + (A&B&0x0421);
			++x1;

			u += dudxi;
			v += dvdxi;
		}

		dst += dstpitch;
		xl += dxl1;
		xr += dxr1;
		ul += dul1;
		vl += dvl1;

		++y;
	}
}

///////////////////////////////////////////////////////////////////////////

#pragma optimize("t", off)
#pragma optimize("s", on)

static BOOL CALLBACK HideAllButOKCANCELProc(HWND hwnd, LPARAM lParam) {
	UINT id = GetWindowLong(hwnd, GWL_ID);

	if (id != IDOK && id != IDCANCEL)
		ShowWindow(hwnd, SW_HIDE);

	return TRUE;
}

static void CALLBACK AboutTimerProc(UINT uID, UINT, DWORD dwUser, DWORD, DWORD) {
	PostMessage((HWND)dwUser, WM_APP+0, 0, 0);
}

BOOL APIENTRY AboutDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
	static bool bTimerSet;
	static bool bRender;
	static MMRESULT mmTimer;
	static Pixel16 *tex;
	static RECT rBounce;
	static RECT rDirtyLast;
	static int xpos, ypos, xvel, yvel;
	static double vx[8][3];
	static const int faces[6][4] = {
		{ 0, 1, 4, 5 },
		{ 2, 6, 3, 7 },
		{ 0, 4, 2, 6 },
		{ 1, 3, 5, 7 },
		{ 0, 2, 1, 3 },
		{ 4, 5, 6, 7 },
	};

    switch (message)
    {
        case WM_INITDIALOG:
			{
				char buf[128];

				wsprintf(buf, "Build %d/"
#ifdef _DEBUG
					"debug"
#else
					"release"
#endif
					" (%s)", version_num, version_time);

				SetDlgItemText(hDlg, IDC_FINALS_SUCK, buf);

				HRSRC hrsrc;

				if (hrsrc = FindResource(NULL, MAKEINTRESOURCE(IDR_CREDITS), "STUFF")) {
					HGLOBAL hGlobal;
					if (hGlobal = LoadResource(NULL, hrsrc)) {
						const char *pData, *pLimit;

						if (pData = (const char *)LockResource(hGlobal)) {
							HWND hwndItem = GetDlgItem(hDlg, IDC_CREDITS);
							const INT tab = 80;

							pLimit = pData + SizeofResource(NULL, hrsrc);

							SendMessage(hwndItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
							SendMessage(hwndItem, LB_SETTABSTOPS, 1, (LPARAM)&tab);

							while(pData < pLimit) {
								char *t = buf;

								while(pData < pLimit && *pData!='\r' && *pData!='\n')
									*t++ = *pData++;

								while(pData < pLimit && (*pData=='\r' || *pData=='\n'))
									++pData;

								*t = 0;

								if (t > buf)
									SendMessage(GetDlgItem(hDlg, IDC_CREDITS), LB_ADDSTRING, 0, (LPARAM)buf);
							}

							FreeResource(hGlobal);
						}
						FreeResource(hGlobal);
					}
				}

				IAMPDecoder *iad = CreateAMPDecoder();

				if (iad) {
					wsprintf(buf, "MPEG audio decoder: %s", iad->GetAmpVersionString());
					delete iad;
					SetDlgItemText(hDlg, IDC_MP3_DECODER, buf);
				}

				// Showtime!  Invalidate the entire window, force an update, and show the window.

				ShowWindow(hDlg, SW_SHOW);
				InvalidateRect(hDlg, NULL, TRUE);
				UpdateWindow(hDlg);

				// Grab the client area.

				HDC hdc;
				RECT r;

				GetClientRect(hDlg, &r);

				g_bihAboutDisplay.biSize			= sizeof(BITMAPINFOHEADER);
				g_bihAboutDisplay.biWidth			= r.right;
				g_bihAboutDisplay.biHeight			= r.bottom;
				g_bihAboutDisplay.biBitCount		= 16;
				g_bihAboutDisplay.biPlanes			= 1;
				g_bihAboutDisplay.biCompression		= BI_RGB;
				g_bihAboutDisplay.biXPelsPerMeter	= 80;
				g_bihAboutDisplay.biYPelsPerMeter	= 80;
				g_bihAboutDisplay.biClrUsed			= 0;
				g_bihAboutDisplay.biClrImportant	= 0;

				if (hdc = GetDC(hDlg)) {
					if (g_hdcAboutDisplay = CreateCompatibleDC(hdc)) {
						if (g_hbmAboutDisplay = CreateDIBSection(g_hdcAboutDisplay, (const BITMAPINFO *)&g_bihAboutDisplay, DIB_RGB_COLORS, &g_pvAboutDisplay, NULL, 0)) {
							g_hgoAboutDisplay = SelectObject(g_hdcAboutDisplay, g_hbmAboutDisplay);

							BitBlt(g_hdcAboutDisplay, 0, 0, r.right, r.bottom, hdc, 0, 0, SRCCOPY);
							GdiFlush();

							if (tex = new Pixel16[32*32]) {
								g_pvAboutDisplayBack = malloc(((r.right+3)&~3)*2*r.bottom);

								if (g_pvAboutDisplayBack) {
									g_vbAboutSrc.init(g_pvAboutDisplayBack, r.right, r.bottom, 16);
									g_vbAboutDst.init(g_pvAboutDisplay, &g_bihAboutDisplay);
									g_vbAboutSrc.BitBlt(0, 0, &g_vbAboutDst, 0, 0, -1, -1);

									// Hide all controls.

									EnumChildWindows(hDlg, HideAllButOKCANCELProc, 0);

									// Grab the VirtualDub icon.

									HICON hico = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_VIRTUALDUB));

									DrawIcon(g_hdcAboutDisplay, 0, 0, hico);

									GdiFlush();

									VBitmap(tex, 32, 32, 16).BitBlt(0, 0, &g_vbAboutDst, 0, 0, 32, 32);

									g_vbAboutDst.BitBlt(0, 0, &g_vbAboutSrc, 0, 0, 32, 32);

									// Initialize cube vertices.

									{
										int i;
										double rs;

										if (r.right > r.bottom)
											rs = r.bottom / (3.6*3.0);
										else
											rs = r.right / (3.6*3.0);

										for(i=0; i<8; i++) {
											vx[i][0] = i&1 ? -rs : +rs;
											vx[i][1] = i&2 ? -rs : +rs;
											vx[i][2] = i&4 ? -rs : +rs;
										}

										rBounce.left = rBounce.top = (int)ceil(rs*1.8);
										rBounce.right = r.right - rBounce.left;
										rBounce.bottom = r.bottom - rBounce.top;

										xpos = rand()%(rBounce.right-rBounce.left) + rBounce.left;
										ypos = rand()%(rBounce.bottom-rBounce.top) + rBounce.top;
										xvel = (rand()&2)-1;
										yvel = (rand()&2)-1;

										rDirtyLast.top = rDirtyLast.left = 0;
										rDirtyLast.right = rDirtyLast.bottom = 0;
									}

									InvalidateRect(hDlg, NULL, TRUE);

									if (TIMERR_NOERROR == timeBeginPeriod(10)) {
										bTimerSet = true;
										bRender = true;

										mmTimer = timeSetEvent(10, 10, AboutTimerProc, (DWORD)hDlg, TIME_PERIODIC|TIME_CALLBACK_FUNCTION);
									}
								}
							}
						}
					}
				}

			}
            return (TRUE);

        case WM_COMMAND:                      
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
            {
				if (tex) {
					free(tex);
					tex = 0;
				}

				if (g_pvAboutDisplayBack) {
					free(g_pvAboutDisplayBack);
					g_pvAboutDisplayBack = 0;
				}

				if (g_hbmAboutDisplay) {
					DeleteObject(SelectObject(g_hdcAboutDisplay, g_hgoAboutDisplay));
					g_hbmAboutDisplay = NULL;
				}

				if (g_hdcAboutDisplay) {
					DeleteDC(g_hdcAboutDisplay);
					g_hdcAboutDisplay = NULL;
				}

				if (mmTimer)
					timeKillEvent(mmTimer);

				if (bTimerSet)
					timeEndPeriod(33);

                EndDialog(hDlg, TRUE);  
                return TRUE;
            }
            break;

		case WM_ERASEBKGND:
			if (g_pvAboutDisplayBack) {
				SetWindowLong(hDlg, DWL_MSGRESULT, 0);
				return TRUE;
			} else
				return FALSE;

		case WM_PAINT:
			if (g_pvAboutDisplayBack) {
				HDC hdc;
				PAINTSTRUCT ps;

				if (hdc = BeginPaint(hDlg, &ps)) {
					BitBlt(hdc, 0, 0, g_vbAboutDst.w, g_vbAboutDst.h,
						g_hdcAboutDisplay, 0, 0, SRCCOPY);

					bRender = true;

					EndPaint(hDlg, &ps);
				}
				return TRUE;
			}
			return FALSE;

		case WM_APP:
			if (g_pvAboutDisplayBack && bRender) {
				static double theta = 0.0;
				double xt, yt, zt;

				bRender = false;

				xt = sin(theta) / 80.0;
				yt = sin(theta + 3.1415926535 * 2.0 / 3.0) / 80.0;
				zt = sin(theta + 3.1415926535 * 4.0 / 3.0) / 80.0;
				theta = theta + 0.005;

				xpos += xvel;
				ypos += yvel;

				if (xpos<rBounce.left) { xpos = rBounce.left; xvel = +1; }
				if (xpos>rBounce.right) { xpos = rBounce.right; xvel = -1; }
				if (ypos<rBounce.top) { ypos = rBounce.top; yvel = +1; }
				if (ypos>rBounce.bottom) { ypos = rBounce.bottom; yvel = -1; }

				RECT rDirty = { 0x7fffffff, 0x7fffffff, -1, -1 };
				RECT rDirty2;

				for(int i=0; i<8; i++) {
					double x0 = vx[i][0];
					double y0 = vx[i][1];
					double z0 = vx[i][2];

					double x1 = x0 * cos(zt) - y0 * sin(zt);
					double y1 = x0 * sin(zt) + y0 * cos(zt);
					double z1 = z0;

					double x2 = x1 * cos(yt) - z1 * sin(yt);
					double y2 = y1;
					double z2 = x1 * sin(yt) + z1 * cos(yt);

					double x3 = x2;
					double y3 = y2 * cos(xt) - z2 * sin(xt);
					double z3 = y2 * sin(xt) + z2 * cos(xt);

					vx[i][0] = x3;
					vx[i][1] = y3;
					vx[i][2] = z3;

					int ix1 = (int)floor(x3);
					int ix2 = (int)ceil(x3);
					int iy1 = (int)floor(y3);
					int iy2 = (int)ceil(y3);

					if (rDirty.left   > ix1) rDirty.left   = ix1;
					if (rDirty.right  < ix1) rDirty.right  = ix1;
					if (rDirty.top    > iy1) rDirty.top    = iy1;
					if (rDirty.bottom < iy1) rDirty.bottom = iy1;
				}

				OffsetRect(&rDirty, xpos, ypos);
				UnionRect(&rDirty2, &rDirty, &rDirtyLast);
				rDirtyLast = rDirty;

				++rDirty2.right;
				++rDirty2.bottom;

				GdiFlush();

				g_vbAboutDst.BitBlt(rDirty2.left, rDirty2.top, &g_vbAboutSrc, rDirty2.left, rDirty2.top,
					rDirty2.right+1-rDirty2.left, rDirty2.bottom+1-rDirty2.top);

				TriPt v[4];

				v[0].u =  0;	v[0].v = 0;
				v[1].u = 32;	v[1].v = 0;
				v[2].u =  0;	v[2].v = 32;
				v[3].u = 32;	v[3].v = 32;

				for(int f=0; f<6; f++) {
					v[0].x = vx[faces[f][0]][0] + xpos;
					v[0].y = vx[faces[f][0]][1] + ypos;
					v[1].x = vx[faces[f][1]][0] + xpos;
					v[1].y = vx[faces[f][1]][1] + ypos;
					v[2].x = vx[faces[f][2]][0] + xpos;
					v[2].y = vx[faces[f][2]][1] + ypos;
					v[3].x = vx[faces[f][3]][0] + xpos;
					v[3].y = vx[faces[f][3]][1] + ypos;

					RenderTriangle((Pixel16 *)g_vbAboutDst.Address32(0,0), -g_vbAboutDst.pitch/2, tex, v+0, v+1, v+2);
					RenderTriangle((Pixel16 *)g_vbAboutDst.Address32(0,0), -g_vbAboutDst.pitch/2, tex, v+2, v+1, v+3);
				}

				InvalidateRect(hDlg, &rDirty2, FALSE);
			}
			return TRUE;
    }
    return FALSE;
}
