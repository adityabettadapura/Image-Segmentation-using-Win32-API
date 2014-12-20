
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>
#include <windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <process.h>	/* needed for multithreading */
#include "resource.h"
#include "globals.h"

void Regions();
void QueuePaintFill();
BOOL CALLBACK PredicateDlgProc(HWND, UINT, WPARAM, LPARAM);
int intensity = DEFAULT_INTENSITY, distance = DEFAULT_DISTANCE;
int r, c, paint_over_label;
unsigned char *labels;
int new_label;
int new_colour;
int *coordinate;
int majorcount;
int predicate1, predicate2;
int playmode, usermode = 0;
int reloadimage = 0;
int delayloop;


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPTSTR lpCmdLine, int nCmdShow)

{
	MSG			msg;
	HWND		hWnd;
	WNDCLASS	wc;

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC)WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, "ID_PLUS_ICON");
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = "ID_MAIN_MENU";
	wc.lpszClassName = "PLUS";

	if (!RegisterClass(&wc))
		return(FALSE);

	hWnd = CreateWindow("PLUS", "plus program",
		WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
		CW_USEDEFAULT, 0, 600, 600, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		return(FALSE);

	ShowScrollBar(hWnd, SB_BOTH, FALSE);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	MainWnd = hWnd;

	ShowPixelCoords = 0;
	ShowPlayMode = 0;
	ShowUserMode = 0;

	strcpy(filename, "");
	OriginalImage = NULL;
	ROWS = COLS = 0;

	InvalidateRect(hWnd, NULL, TRUE);
	UpdateWindow(hWnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return(msg.wParam);
}




LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg,
	WPARAM wParam, LPARAM lParam)

{
	HMENU				hMenu;
	OPENFILENAME		ofn;
	FILE				*fpt;
	HDC					hDC;
	char				header[320], text[320];
	int					BYTES, xPos, yPos;
	CHOOSECOLOR cc;                 // common dialog box structure 
	static COLORREF acrCustClr[16]; // array of custom colors 
	HBRUSH hbrush;                  // brush handle
	static DWORD rgbCurrent;        // initial color selection
	int ret;
	int r2, c2;

	switch (uMsg)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_SHOWPIXELCOORDS:
			ShowPixelCoords = (ShowPixelCoords + 1) % 2;
			PaintImage();
			break;
		case ID_FILE_LOAD:
			if (OriginalImage != NULL)
			{
				free(OriginalImage);
				OriginalImage = NULL;
			}
			memset(&(ofn), 0, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.lpstrFile = filename;
			filename[0] = 0;
			ofn.nMaxFile = MAX_FILENAME_CHARS;
			ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
			ofn.lpstrFilter = "PPM files\0*.ppm\0All files\0*.*\0\0";
			if (!(GetOpenFileName(&ofn)) || filename[0] == '\0')
				break;		/* user cancelled load */
			if ((fpt = fopen(filename, "rb")) == NULL)
			{
				MessageBox(NULL, "Unable to open file", filename, MB_OK | MB_APPLMODAL);
				break;
			}
			fscanf(fpt, "%s %d %d %d", header, &COLS, &ROWS, &BYTES);
			if (strcmp(header, "P5") != 0 || BYTES != 255)
			{
				MessageBox(NULL, "Not a PPM (P5 greyscale) image", filename, MB_OK | MB_APPLMODAL);
				fclose(fpt);
				break;
			}
			OriginalImage = (unsigned char *)calloc(ROWS*COLS, 1);
			coordinate = (int *)calloc(ROWS*COLS, sizeof(int));
			header[0] = fgetc(fpt);	/* whitespace character after header */
			fread(OriginalImage, 1, ROWS*COLS, fpt);
			fclose(fpt);
			SetWindowText(hWnd, filename);
			PaintImage();
			break;

		case ID_FILE_CLEARIMAGE:
			fpt = fopen(filename, "rb");
			fscanf(fpt, "%s %d %d %d", header, &COLS, &ROWS, &BYTES);
			OriginalImage = (unsigned char *)calloc(ROWS*COLS, 1);
			header[0] = fgetc(fpt);	/* whitespace character after header */
			fread(OriginalImage, 1, ROWS*COLS, fpt);
			fclose(fpt);
			majorcount = 0;
			ShowPlayMode = 0;
			ShowUserMode = 0;
			ThreadRunning = 0;
			playmode = 0;
			usermode = 0;
			reloadimage = 1;

			KillTimer(MainWnd, TIMER_SECOND);
			PaintImage();
			break;

		case ID_SETTINGS_COLOUR:
			// Initialize CHOOSECOLOR 
			ZeroMemory(&cc, sizeof(cc));
			cc.lStructSize = sizeof(cc);
			cc.hwndOwner = hWnd;
			cc.lpCustColors = (LPDWORD)acrCustClr;
			cc.rgbResult = rgbCurrent;
			cc.Flags = CC_FULLOPEN | CC_RGBINIT;

			if (ChooseColor(&cc) == TRUE)
			{
				hbrush = CreateSolidBrush(cc.rgbResult);
				rgbCurrent = cc.rgbResult;
				new_colour = rgbCurrent;
			}
			break;

		case ID_SETTINGS_PREDICATE:
			ret = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), hWnd, PredicateDlgProc);
			PaintImage();
			break;

		case ID_MODE_PLAYMODE:
			playmode = 1;
			delayloop = 0;
			reloadimage = 0;
			ShowPlayMode = (ShowPlayMode + 1) % 2;
			ShowUserMode = 0;
			PaintImage();
			break;
			break;

		case ID_MODE_USERMODE:
			ShowUserMode = (ShowUserMode + 1) % 2;
			ShowPlayMode = 0;
			playmode = 0;
			stepevent = CreateEvent(
				NULL,               // default security attributes
				FALSE,               // manual-reset event
				FALSE,              // initial state is nonsignaled
				TEXT("J-Event")  // object name
				);
			usermode = 1;
			reloadimage = 0;
			break;

		case ID_FILE_QUIT:
			DestroyWindow(hWnd);
			break;
		}
		break;
	case WM_SIZE:		  /* could be used to detect when window size changes */
		PaintImage();
		restorecolour(coordinate, majorcount);
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_PAINT:
		PaintImage();
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:

		c = LOWORD(lParam);
		r = HIWORD(lParam);
		hDC = GetDC(MainWnd);
		paint_over_label = GetRValue(GetPixel(hDC, c, r));
		new_label = 90;
		ReleaseDC(MainWnd, hDC);
		_beginthread(QueuePaintFill, 0, hWnd);
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_MOUSEMOVE:
		if (ShowPixelCoords == 1)
		{
			xPos = LOWORD(lParam);
			yPos = HIWORD(lParam);
			if (xPos >= 0 && xPos < COLS  &&  yPos >= 0 && yPos < ROWS)
			{
				sprintf(text, "%d,%d=>%d     ", xPos, yPos, OriginalImage[yPos*COLS + xPos]);
				hDC = GetDC(MainWnd);
				TextOut(hDC, 0, 0, text, strlen(text));		/* draw text on the window */
				SetPixel(hDC, xPos, yPos, RGB(255, 0, 0));	/* color the cursor position red */
				ReleaseDC(MainWnd, hDC);
			}
		}
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_KEYDOWN:
		if (wParam == 's' || wParam == 'S')
			PostMessage(MainWnd, WM_COMMAND, ID_SHOWPIXELCOORDS, 0);	  /* send message to self */
		if ((TCHAR)wParam == '1')
		{
			TimerRow = TimerCol = 0;
			SetTimer(MainWnd, TIMER_SECOND, 10, NULL);	/* start up 10 ms timer */
		}
		if ((TCHAR)wParam == '2')
		{
			KillTimer(MainWnd, TIMER_SECOND);			/* halt timer, stopping generation of WM_TIME events */
			PaintImage();								/* redraw original image, erasing animation */
		}
		if ((TCHAR)wParam == '3')
		{
			ThreadRunning = 1;
			_beginthread(AnimationThread, 0, MainWnd);	/* start up a child thread to do other work while this thread continues GUI */
		}
		if ((TCHAR)wParam == '4')
		{
			ThreadRunning = 0;							/* this is used to stop the child thread (see its code below) */
		}
		if ((TCHAR)wParam == 'J' || (TCHAR)wParam == 'j' && playmode == 0)
		{
			usermode = 1;
		}

		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_TIMER:	  /* this event gets triggered every time the timer goes off */
		hDC = GetDC(MainWnd);
		SetPixel(hDC, TimerCol, TimerRow, RGB(0, 0, 255));	/* color the animation pixel blue */
		ReleaseDC(MainWnd, hDC);
		TimerRow++;
		TimerCol += 2;
		break;
	case WM_HSCROLL:	  /* this event could be used to change what part of the image to draw */
		PaintImage();	  /* direct PaintImage calls eliminate flicker; the alternative is InvalidateRect(hWnd,NULL,TRUE); UpdateWindow(hWnd); */
		restorecolour(coordinate, majorcount);
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_VSCROLL:	  /* this event could be used to change what part of the image to draw */
		PaintImage();
		restorecolour(coordinate, majorcount);
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return(DefWindowProc(hWnd, uMsg, wParam, lParam));
		break;
	}

	hMenu = GetMenu(MainWnd);
	if (ShowPixelCoords == 1)
		CheckMenuItem(hMenu, ID_SHOWPIXELCOORDS, MF_CHECKED);	/* you can also call EnableMenuItem() to grey(disable) an option */
	else
		CheckMenuItem(hMenu, ID_SHOWPIXELCOORDS, MF_UNCHECKED);

	if (ShowPlayMode == 1)
		CheckMenuItem(hMenu, ID_MODE_PLAYMODE, MF_CHECKED);	/* you can also call EnableMenuItem() to grey(disable) an option */
	else
		CheckMenuItem(hMenu, ID_MODE_PLAYMODE, MF_UNCHECKED);
	
	if (ShowUserMode == 1)
		CheckMenuItem(hMenu, ID_MODE_USERMODE, MF_CHECKED);	/* you can also call EnableMenuItem() to grey(disable) an option */
	else
		CheckMenuItem(hMenu, ID_MODE_USERMODE, MF_UNCHECKED);
	
	DrawMenuBar(hWnd);

	return(0L);
}




void PaintImage()
{
	PAINTSTRUCT			Painter;
	HDC					hDC;
	BITMAPINFOHEADER	bm_info_header;
	BITMAPINFO			*bm_info;
	int					i, r, c, DISPLAY_ROWS, DISPLAY_COLS;
	unsigned char		*DisplayImage;

	if (OriginalImage == NULL)
		return;		/* no image to draw */

	/* Windows pads to 4-byte boundaries.  We have to round the size up to 4 in each dimension, filling with black. */
	DISPLAY_ROWS = ROWS;
	DISPLAY_COLS = COLS;
	if (DISPLAY_ROWS % 4 != 0)
		DISPLAY_ROWS = (DISPLAY_ROWS / 4 + 1) * 4;
	if (DISPLAY_COLS % 4 != 0)
		DISPLAY_COLS = (DISPLAY_COLS / 4 + 1) * 4;
	DisplayImage = (unsigned char *)calloc(DISPLAY_ROWS*DISPLAY_COLS, 1);
	for (r = 0; r < ROWS; r++)
		for (c = 0; c < COLS; c++)
			DisplayImage[r*DISPLAY_COLS + c] = OriginalImage[r*COLS + c];

	BeginPaint(MainWnd, &Painter);
	hDC = GetDC(MainWnd);
	bm_info_header.biSize = sizeof(BITMAPINFOHEADER);
	bm_info_header.biWidth = DISPLAY_COLS;
	bm_info_header.biHeight = -DISPLAY_ROWS;
	bm_info_header.biPlanes = 1;
	bm_info_header.biBitCount = 8;
	bm_info_header.biCompression = BI_RGB;
	bm_info_header.biSizeImage = 0;
	bm_info_header.biXPelsPerMeter = 0;
	bm_info_header.biYPelsPerMeter = 0;
	bm_info_header.biClrUsed = 256;
	bm_info_header.biClrImportant = 256;
	bm_info = (BITMAPINFO *)calloc(1, sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD));
	bm_info->bmiHeader = bm_info_header;
	for (i = 0; i < 256; i++)
	{
		bm_info->bmiColors[i].rgbBlue = bm_info->bmiColors[i].rgbGreen = bm_info->bmiColors[i].rgbRed = i;
		bm_info->bmiColors[i].rgbReserved = 0;
	}

	SetDIBitsToDevice(hDC, 0, 0, DISPLAY_COLS, DISPLAY_ROWS, 0, 0,
		0, /* first scan line */
		DISPLAY_ROWS, /* number of scan lines */
		DisplayImage, bm_info, DIB_RGB_COLORS);
	ReleaseDC(MainWnd, hDC);
	EndPaint(MainWnd, &Painter);

	free(DisplayImage);
	free(bm_info);
}




void AnimationThread(HWND AnimationWindowHandle)

{
	HDC		hDC;
	char	text[300];

	ThreadRow = ThreadCol = 0;
	while (ThreadRunning == 1)
	{
		hDC = GetDC(MainWnd);
		SetPixel(hDC, ThreadCol, ThreadRow, RGB(0, 255, 0));	/* color the animation pixel green */
		sprintf(text, "%d,%d     ", ThreadRow, ThreadCol);
		TextOut(hDC, 300, 0, text, strlen(text));		/* draw text on the window */
		ReleaseDC(MainWnd, hDC);
		ThreadRow += 3;
		ThreadCol++;
		Sleep(100);		/* pause 100 ms */
	}
}

BOOL CALLBACK PredicateDlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	//int distance = 0, intensity = 0;
	BOOL flag1, flag2;
	switch (Message)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hWnd, IDOK);
			intensity = GetDlgItemInt(hWnd, IDC_EDIT1, &flag1, FALSE);
			distance = GetDlgItemInt(hWnd, IDC_EDIT2, &flag2, FALSE);
			//Check for intensity
			if (flag1)
			{
				if ((intensity < 0) || (intensity > 255))
				{
					MessageBox(hWnd, "Please enter correct intensity value", "Error", MB_OK | MB_ICONWARNING);
				}
				else
					predicate1 = 1;
			}
			//else
			//MessageBox(hWnd, "Error! No intensity value entered", "Error", MB_ICONEXCLAMATION | MB_OK);
			//intensity = DEFAULT_INTENSITY;

			//check for distance
			if (flag2)
			{
				if (distance < 0)
				{
					MessageBox(hWnd, "Please enter valid distance from centroid", "Error", MB_OK | MB_ICONWARNING);
				}
				else
					predicate2 = 1;
			}
			//else
			//MessageBox(hWnd, "Error! No distance value entered", "Error", MB_ICONEXCLAMATION | MB_OK);
			//	distance = 100;
			break;
		case IDCANCEL:
			EndDialog(hWnd, IDCANCEL);
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void QueuePaintFill(HWND AnimationThreadHandle)
{
	int	r2, c2;
	int	queue[MAX_QUEUE], qh, qt;
	int *indices = (int *)calloc(ROWS*COLS, sizeof(int));;
	HDC hDC;
	int centroidrow = 0, centroidcol = 0;
	int colnum = 0, rownum = 0;
	int xdist = 0, ydist = 0;
	int absdist = 0;
	int count = 0;
	double avg=0;

	indices[0] = r*COLS + c;
	//count = 0;
	if (OriginalImage[r*COLS + c] != paint_over_label)
		return;
	OriginalImage[r*COLS + c] = new_label;
	hDC = GetDC(MainWnd);
	SetPixel(hDC, c, r, new_colour);
	coordinate[majorcount++] = indices[0];
	ReleaseDC(MainWnd, hDC);

	if (indices != NULL)
		indices[0] = r*COLS + c;
	queue[0] = r*COLS + c;
	qh = 1;	/* queue head */
	qt = 0;	/* queue tail */
	count = 1;
	restorecolour(coordinate, majorcount);
	for (int r2 = -3; r2 <= 3; r2++)
	{
		for (int c2 = -3; c2 <= 3; c2++)
		{
			avg += (double)(OriginalImage[(r + r2)*COLS + (c + c2)]);
		}
	}
	avg = avg / 49;
	//while (qt != qh)
	do
	{
		for (r2 = -1; r2 <= 1; r2++)
			for (c2 = -1; c2 <= 1; c2++)
			{
			if (r2 == 0 && c2 == 0)
				continue;
			if ((queue[qt] / COLS + r2) < 0 || (queue[qt] / COLS + r2) >= ROWS ||
				(queue[qt] % COLS + c2) < 0 || (queue[qt] % COLS + c2) >= COLS)
				continue;

			if (predicate2 == 1)
			{
				if (OriginalImage[(queue[qt] / COLS + r2)*COLS + queue[qt] % COLS + c2] != paint_over_label)
					continue;
			}
			
			if (predicate1 == 1)
			{
				if (abs(OriginalImage[(queue[qt] / COLS + r2)*COLS + queue[qt] % COLS + c2] - avg) > intensity)
					continue;
			}
			

			colnum += queue[qt] % COLS + c2;
	 		rownum += queue[qt] / COLS + r2;

			centroidcol = colnum / count;
			centroidrow = rownum / count;

			xdist = centroidcol - queue[qt] % COLS + c2;
			ydist = centroidrow - queue[qt] / COLS + r2;

			absdist = abs(sqrt(SQR(xdist) + SQR(ydist)));

			if (predicate2 == 1)
			{
				if (absdist >  distance)
				{
					colnum -= queue[qt] % COLS + c2;
					rownum -= queue[qt] / COLS + r2;
					continue;
				}
			}
	
			OriginalImage[(queue[qt] / COLS + r2)*COLS + queue[qt] % COLS + c2] = new_label;
			hDC = GetDC(MainWnd);
			SetPixel(hDC, queue[qt] % COLS + c2, (queue[qt] / COLS + r2), new_colour);
			ReleaseDC(MainWnd, hDC);

			if (indices != NULL)
				indices[count] = (queue[qt] / COLS + r2)*COLS + queue[qt] % COLS + c2;
			coordinate[majorcount++] = indices[count];

			/*hDC = GetDC(MainWnd);
			SetPixel(hDC, centroidcol, centroidrow, RGB(255,255,255));
			ReleaseDC(MainWnd, hDC);
*/

			(count)++;
			queue[qh] = (queue[qt] / COLS + r2)*COLS + queue[qt] % COLS + c2;
			qh = (qh + 1) % MAX_QUEUE;
			if (qh == qt)
			{
				printf("Max queue size exceeded\n");
				//exit(0);
			}
			}
		qt = (qt + 1) % MAX_QUEUE;
		//PaintImage();
		//Sleep(1);
		if (playmode)
		{
			Sleep(1);
		}
		if (usermode)
		{
			delayloop = 1;
		}
		if (delayloop)
		{
			usermode = 0;
			DWORD wait;
			while (!playmode && !usermode)
			{
				wait = WaitForInputIdle(stepevent, 100);
				switch (wait)
				{
				case 0:
					usermode = 1;
					break;
				}
				if (reloadimage == 1)
				{
					break;
				}
			}
		}
	} while (qt != qh && (playmode || usermode));

}

void restorecolour(int *coordinate, int majorcount)
{
	int xpos = 0, ypos = 0;
	HDC hDC = GetDC(MainWnd);
	if (majorcount == 0)
	{
		ReleaseDC(MainWnd, hDC);
		return;
	}
	for (int i = 0; i < majorcount; i++)
	{
		xpos = coordinate[i] / COLS;
		ypos = coordinate[i] % COLS;
		SetPixel(hDC, ypos, xpos, new_colour);
	}
	ReleaseDC(MainWnd, hDC);

}