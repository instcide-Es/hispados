/* win32min.h - lo justo del API Win32 para los programas de prueba de
 * Hispa-486 hechos con gcc -m32 y tools/elf2pe.py (sin las cabeceras de
 * Windows). Los nombres y los números son los documentados del API.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef WIN32MIN_H
#define WIN32MIN_H
typedef unsigned int u32;
typedef unsigned short u16;
typedef void *HANDLE;
typedef HANDLE HWND;
#define WINAPI __attribute__((stdcall))
#define NULL ((void *)0)
typedef struct { int left, top, right, bottom; } RECT;
typedef struct { int x, y; } POINT;
typedef struct { HANDLE hwnd; u32 message, wParam, lParam, time; POINT pt; } MSG;
typedef int WINAPI (*WNDPROC)(HANDLE, u32, u32, u32);
typedef struct {
  u32 style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HANDLE hInstance, hIcon, hCursor, hbrBackground;
  const char *lpszMenuName, *lpszClassName;
} WNDCLASSA;
typedef struct { HANDLE hdc; int fErase; RECT rcPaint; int fRestore, fIncUpdate; char rgbReserved[32]; } PAINTSTRUCT;
typedef struct { void *lpCreateParams; HANDLE hInstance, hMenu, hwndParent; int cy, cx, y, x; u32 style;
  const char *lpszName, *lpszClass; u32 dwExStyle; } CREATESTRUCTA;
typedef struct { u16 wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef struct { u32 dwFileAttributes, ftc[2], fta[2], ftw[2], nFileSizeHigh, nFileSizeLow, r0, r1;
  char cFileName[260]; char cAlternateFileName[14]; } WIN32_FIND_DATAA;

/* mensajes, estilos y avisos */
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_SETFOCUS 0x0007
#define WM_SETTEXT 0x000C
#define WM_GETTEXT 0x000D
#define WM_GETTEXTLENGTH 0x000E
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_KEYDOWN 0x0100
#define WM_CHAR 0x0102
#define WM_INITDIALOG 0x0110
#define WM_COMMAND 0x0111
#define WM_TIMER 0x0113
#define WM_LBUTTONDOWN 0x0201
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_OVERLAPPED 0
#define WS_CAPTION 0x00C00000
#define WS_SYSMENU 0x00080000
#define WS_MINIMIZEBOX 0x00020000
#define WS_CHILD 0x40000000
#define WS_VISIBLE 0x10000000
#define WS_DISABLED 0x08000000
#define WS_BORDER 0x00800000
#define WS_TABSTOP 0x00010000
#define WS_GROUP 0x00020000
#define WS_VSCROLL 0x00200000
#define CW_USEDEFAULT ((int)0x80000000)
#define BS_PUSHBUTTON 0
#define BS_DEFPUSHBUTTON 1
#define BS_CHECKBOX 2
#define BS_AUTOCHECKBOX 3
#define BS_RADIOBUTTON 4
#define BS_AUTORADIOBUTTON 9
#define BN_CLICKED 0
#define BM_GETCHECK 0x00F0
#define BM_SETCHECK 0x00F1
#define ES_MULTILINE 4
#define ES_AUTOHSCROLL 0x80
#define ES_READONLY 0x800
#define ES_RIGHT 2
#define EN_CHANGE 0x0300
#define EM_SETSEL 0x00B1
#define EM_GETLINECOUNT 0x00BA
#define SS_RIGHT 2
#define LBS_NOTIFY 1
#define LB_ADDSTRING 0x0180
#define LB_DELETESTRING 0x0182
#define LB_RESETCONTENT 0x0184
#define LB_SETCURSEL 0x0186
#define LB_GETCURSEL 0x0188
#define LB_GETTEXT 0x0189
#define LB_GETCOUNT 0x018B
#define LBN_SELCHANGE 1
#define LBN_DBLCLK 2
#define CBS_DROPDOWNLIST 3
#define CB_ADDSTRING 0x0143
#define CB_GETCOUNT 0x0146
#define CB_GETCURSEL 0x0147
#define CB_GETLBTEXT 0x0148
#define CB_RESETCONTENT 0x014B
#define CB_SETCURSEL 0x014E
#define CBN_SELCHANGE 1
#define MF_STRING 0
#define MF_GRAYED 1
#define MF_CHECKED 8
#define MF_UNCHECKED 0
#define MF_ENABLED 0
#define MF_POPUP 0x10
#define MF_SEPARATOR 0x800
#define MF_BYCOMMAND 0
#define MB_OK 0
#define MB_YESNO 4
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define COLOR_WINDOW 5
#define COLOR_BTNFACE 15
#define SW_SHOW 5
#define SW_HIDE 0
#define GWL_STYLE (-16)
#define GWL_ID (-12)
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define MAKEINTRESOURCEA(n) ((const char *)(u32)(u16)(n))
#define LOWORD(x) ((u32)(x) & 0xFFFF)
#define HIWORD(x) (((u32)(x) >> 16) & 0xFFFF)

/* KERNEL32 */
void WINAPI ExitProcess(u32);
HANDLE WINAPI GetModuleHandleA(const char *);
HANDLE WINAPI LoadLibraryA(const char *);
void *WINAPI GetProcAddress(HANDLE, const char *);
HANDLE WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(HANDLE, u32, u32);
void *WINAPI HeapReAlloc(HANDLE, u32, void *, u32);
int WINAPI HeapFree(HANDLE, u32, void *);
u32 WINAPI HeapSize(HANDLE, u32, const void *);
void *WINAPI VirtualAlloc(void *, u32, u32, u32);
int WINAPI VirtualFree(void *, u32, u32);
HANDLE WINAPI CreateFileA(const char *, u32, u32, void *, u32, u32, HANDLE);
int WINAPI WriteFile(HANDLE, const void *, u32, u32 *, void *);
int WINAPI ReadFile(HANDLE, void *, u32, u32 *, void *);
u32 WINAPI SetFilePointer(HANDLE, int, int *, u32);
u32 WINAPI GetFileSize(HANDLE, u32 *);
int WINAPI CloseHandle(HANDLE);
int WINAPI DeleteFileA(const char *);
HANDLE WINAPI FindFirstFileA(const char *, WIN32_FIND_DATAA *);
int WINAPI FindNextFileA(HANDLE, WIN32_FIND_DATAA *);
int WINAPI FindClose(HANDLE);
void WINAPI GetLocalTime(SYSTEMTIME *);
u32 WINAPI GetTickCount(void);
void WINAPI Sleep(u32);
u32 WINAPI GetEnvironmentVariableA(const char *, char *, u32);
u32 WINAPI TlsAlloc(void);
int WINAPI TlsSetValue(u32, void *);
void *WINAPI TlsGetValue(u32);
int WINAPI TlsFree(u32);
u32 WINAPI GetLastError(void);
u32 WINAPI GetModuleFileNameA(HANDLE, char *, u32);
char *WINAPI GetCommandLineA(void);
u32 WINAPI GetCurrentDirectoryA(u32, char *);
u32 WINAPI GetVersion(void);
int WINAPI lstrlenA(const char *);
char *WINAPI lstrcpyA(char *, const char *);
char *WINAPI lstrcatA(char *, const char *);
int WINAPI lstrcmpA(const char *, const char *);

/* USER32 */
u32 WINAPI RegisterClassA(const WNDCLASSA *);
HANDLE WINAPI CreateWindowExA(u32, const char *, const char *, u32, int, int, int, int, HANDLE, HANDLE, HANDLE, void *);
int WINAPI ShowWindow(HANDLE, int);
int WINAPI UpdateWindow(HANDLE);
int WINAPI DestroyWindow(HANDLE);
int WINAPI DefWindowProcA(HANDLE, u32, u32, u32);
int WINAPI GetMessageA(MSG *, HANDLE, u32, u32);
int WINAPI TranslateMessage(const MSG *);
int WINAPI DispatchMessageA(const MSG *);
int WINAPI IsDialogMessageA(HANDLE, MSG *);
void WINAPI PostQuitMessage(int);
int WINAPI PostMessageA(HANDLE, u32, u32, u32);
int WINAPI SendMessageA(HANDLE, u32, u32, u32);
int WINAPI GetClientRect(HANDLE, RECT *);
int WINAPI GetWindowRect(HANDLE, RECT *);
int WINAPI InvalidateRect(HANDLE, const RECT *, int);
int WINAPI MoveWindow(HANDLE, int, int, int, int, int);
int WINAPI SetWindowTextA(HANDLE, const char *);
int WINAPI GetWindowTextA(HANDLE, char *, int);
int WINAPI GetWindowTextLengthA(HANDLE);
int WINAPI GetWindowLongA(HANDLE, int);
int WINAPI SetWindowLongA(HANDLE, int, int);
u32 WINAPI SetTimer(HANDLE, u32, u32, void *);
int WINAPI KillTimer(HANDLE, u32);
int WINAPI MessageBoxA(HANDLE, const char *, const char *, u32);
HANDLE WINAPI LoadCursorA(HANDLE, const char *);
HANDLE WINAPI LoadIconA(HANDLE, const char *);
int WINAPI LoadStringA(HANDLE, u32, char *, int);
HANDLE WINAPI LoadMenuA(HANDLE, const char *);
int WINAPI GetSystemMetrics(int);
HANDLE WINAPI GetDC(HANDLE);
int WINAPI ReleaseDC(HANDLE, HANDLE);
HANDLE WINAPI BeginPaint(HANDLE, PAINTSTRUCT *);
int WINAPI EndPaint(HANDLE, const PAINTSTRUCT *);
int WINAPI FillRect(HANDLE, const RECT *, HANDLE);
int WINAPI DrawTextA(HANDLE, const char *, int, RECT *, u32);
int wsprintfA(char *, const char *, ...);
HANDLE WINAPI GetDlgItem(HANDLE, int);
int WINAPI GetDlgCtrlID(HANDLE);
int WINAPI SetDlgItemTextA(HANDLE, int, const char *);
u32 WINAPI GetDlgItemTextA(HANDLE, int, char *, int);
int WINAPI SetDlgItemInt(HANDLE, int, u32, int);
u32 WINAPI GetDlgItemInt(HANDLE, int, int *, int);
int WINAPI SendDlgItemMessageA(HANDLE, int, u32, u32, u32);
int WINAPI CheckDlgButton(HANDLE, int, u32);
u32 WINAPI IsDlgButtonChecked(HANDLE, int);
int WINAPI CheckRadioButton(HANDLE, int, int, int);
int WINAPI EnableWindow(HANDLE, int);
int WINAPI IsWindowEnabled(HANDLE);
HANDLE WINAPI SetFocus(HANDLE);
HANDLE WINAPI GetFocus(void);
HANDLE WINAPI GetParent(HANDLE);
HANDLE WINAPI CreateMenu(void);
HANDLE WINAPI CreatePopupMenu(void);
int WINAPI AppendMenuA(HANDLE, u32, u32, const char *);
int WINAPI SetMenu(HANDLE, HANDLE);
HANDLE WINAPI GetMenu(HANDLE);
int WINAPI DrawMenuBar(HANDLE);
int WINAPI DestroyMenu(HANDLE);
u32 WINAPI CheckMenuItem(HANDLE, u32, u32);
int WINAPI EnableMenuItem(HANDLE, u32, u32);
u32 WINAPI GetMenuState(HANDLE, u32, u32);
typedef int WINAPI (*DLGPROC)(HANDLE, u32, u32, u32);
int WINAPI DialogBoxParamA(HANDLE, const char *, HANDLE, DLGPROC, u32);
HANDLE WINAPI CreateDialogParamA(HANDLE, const char *, HANDLE, DLGPROC, u32);
int WINAPI EndDialog(HANDLE, int);

/* GDI32 */
int WINAPI TextOutA(HANDLE, int, int, const char *, int);
u32 WINAPI SetTextColor(HANDLE, u32);
u32 WINAPI SetBkColor(HANDLE, u32);
int WINAPI SetBkMode(HANDLE, int);
HANDLE WINAPI GetStockObject(int);
HANDLE WINAPI CreateSolidBrush(u32);
HANDLE WINAPI SelectObject(HANDLE, HANDLE);
int WINAPI DeleteObject(HANDLE);
int WINAPI Rectangle(HANDLE, int, int, int, int);
#endif
