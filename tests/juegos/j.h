/* j.h - lo común de las pruebas de «lo que pide un juego» (JUEGOS,
 * docs/pentium/WIN32.md): las declaraciones de Win32 que usan, con los
 * nombres y los números de la documentación pública de Microsoft. Se
 * compilan con gcc -m32 -march=i586 y tools/elf2pe.py (con MSVCRT.DLL y el
 * arranque de w32/crt0.c): castel/nucleo32/win32/juegos.sh.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef J_H
#define J_H
typedef unsigned int u32;
typedef int i32;
typedef unsigned short u16;
typedef short i16;
typedef unsigned char u8;
typedef unsigned long long u64;
typedef long long i64;
typedef void *HANDLE;
typedef HANDLE HWND, HMODULE, HKEY, HDC, HINSTANCE;
typedef int BOOL;
typedef u32 (__attribute__((stdcall)) *HILO_F)(void *);
#define WINAPI __attribute__((stdcall))
#define INFINITE 0xFFFFFFFFu
#define NULL ((void *)0)

/* ---- MSVCRT ---- */
typedef struct { char *p; int n; char *b; int f, fd, ch, tam; char *t; } FILE;
extern FILE _iob[];
#define stdout (&_iob[1])
int printf(const char *, ...);
int sprintf(char *, const char *, ...);
int _snprintf(char *, u32, const char *, ...);
int sscanf(const char *, const char *, ...);
int fflush(FILE *);
FILE *fopen(const char *, const char *);
int fclose(FILE *);
u32 fread(void *, u32, u32, FILE *);
u32 fwrite(const void *, u32, u32, FILE *);
int fprintf(FILE *, const char *, ...);
int fgetc(FILE *);
char *fgets(char *, int, FILE *);
int fseek(FILE *, i32, int);
i32 ftell(FILE *);
int remove(const char *);
void *malloc(u32);
void free(void *);
void *memset(void *, int, u32);
void *memcpy(void *, const void *, u32);
int memcmp(const void *, const void *, u32);
u32 strlen(const char *);
int strcmp(const char *, const char *);
char *strcpy(char *, const char *);
char *strstr(const char *, const char *);
int atexit(void (*)(void));
void exit(int);
void qsort(void *, u32, u32, int (*)(const void *, const void *));
i32 time(i32 *);
double atof(const char *);
double sqrt(double);
struct _finddata_t { u32 attrib; i32 time_create, time_access, time_write; u32 size; char name[260]; };
i32 _findfirst(const char *, struct _finddata_t *);
int _findnext(i32, struct _finddata_t *);
int _findclose(i32);
typedef int jmp_buf[16];
int _setjmp(jmp_buf);
int _setjmp3(jmp_buf, int, ...);
void longjmp(jmp_buf, int);
u32 _beginthreadex(void *, u32, u32 (WINAPI *)(void *), void *, u32, u32 *);
u32 _beginthread(void (*)(void *), u32, void *);
void _endthreadex(u32);

/* ---- KERNEL32 ---- */
void WINAPI ExitProcess(u32);
u32 WINAPI GetLastError(void);
void WINAPI SetLastError(u32);
HMODULE WINAPI GetModuleHandleA(const char *);
void *WINAPI GetProcAddress(HMODULE, const char *);
HMODULE WINAPI LoadLibraryA(const char *);
BOOL WINAPI FreeLibrary(HMODULE);
u32 WINAPI GetModuleFileNameA(HMODULE, char *, u32);
char *WINAPI GetCommandLineA(void);
BOOL WINAPI CloseHandle(HANDLE);
HANDLE WINAPI CreateThread(void *, u32, HILO_F, void *, u32, u32 *);
void WINAPI ExitThread(u32);
BOOL WINAPI TerminateThread(HANDLE, u32);
u32 WINAPI ResumeThread(HANDLE);
BOOL WINAPI GetExitCodeThread(HANDLE, u32 *);
BOOL WINAPI SetThreadPriority(HANDLE, int);
int WINAPI GetThreadPriority(HANDLE);
HANDLE WINAPI GetCurrentThread(void);
u32 WINAPI GetCurrentThreadId(void);
u32 WINAPI GetCurrentProcessId(void);
void WINAPI Sleep(u32);
BOOL WINAPI SwitchToThread(void);
u32 WINAPI WaitForSingleObject(HANDLE, u32);
u32 WINAPI WaitForMultipleObjects(u32, const HANDLE *, BOOL, u32);
typedef struct { void *DebugInfo; i32 LockCount, RecursionCount; HANDLE OwningThread, LockSemaphore; u32 SpinCount; } CRITICAL_SECTION;
void WINAPI InitializeCriticalSection(CRITICAL_SECTION *);
void WINAPI EnterCriticalSection(CRITICAL_SECTION *);
BOOL WINAPI TryEnterCriticalSection(CRITICAL_SECTION *);
void WINAPI LeaveCriticalSection(CRITICAL_SECTION *);
void WINAPI DeleteCriticalSection(CRITICAL_SECTION *);
HANDLE WINAPI CreateMutexA(void *, BOOL, const char *);
HANDLE WINAPI OpenMutexA(u32, BOOL, const char *);
BOOL WINAPI ReleaseMutex(HANDLE);
HANDLE WINAPI CreateEventA(void *, BOOL, BOOL, const char *);
HANDLE WINAPI OpenEventA(u32, BOOL, const char *);
BOOL WINAPI SetEvent(HANDLE);
BOOL WINAPI ResetEvent(HANDLE);
BOOL WINAPI PulseEvent(HANDLE);
HANDLE WINAPI CreateSemaphoreA(void *, i32, i32, const char *);
BOOL WINAPI ReleaseSemaphore(HANDLE, i32, i32 *);
i32 WINAPI InterlockedIncrement(volatile i32 *);
i32 WINAPI InterlockedDecrement(volatile i32 *);
i32 WINAPI InterlockedExchange(volatile i32 *, i32);
i32 WINAPI InterlockedExchangeAdd(volatile i32 *, i32);
i32 WINAPI InterlockedCompareExchange(volatile i32 *, i32, i32);
u32 WINAPI TlsAlloc(void);
BOOL WINAPI TlsFree(u32);
void *WINAPI TlsGetValue(u32);
BOOL WINAPI TlsSetValue(u32, void *);
u32 WINAPI GetTickCount(void);
BOOL WINAPI QueryPerformanceCounter(u64 *);
BOOL WINAPI QueryPerformanceFrequency(u64 *);
typedef struct { u32 tam, may, men, build, plat; char csd[128]; } OSVERSIONINFOA;
BOOL WINAPI GetVersionExA(OSVERSIONINFOA *);
u32 WINAPI GetVersion(void);
void WINAPI RaiseException(u32, u32, u32, const u32 *);
typedef struct EXCEPTION_RECORD { u32 ExceptionCode, ExceptionFlags; struct EXCEPTION_RECORD *ExceptionRecord; void *ExceptionAddress; u32 NumberParameters; u32 ExceptionInformation[15]; } EXCEPTION_RECORD;
typedef struct { u32 ControlWord, StatusWord, TagWord, ErrorOffset, ErrorSelector, DataOffset, DataSelector; u8 RegisterArea[80]; u32 Cr0NpxState; } FLOATING_SAVE_AREA;
typedef struct {
	u32 ContextFlags, Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
	FLOATING_SAVE_AREA FloatSave;
	u32 SegGs, SegFs, SegEs, SegDs, Edi, Esi, Ebx, Edx, Ecx, Eax, Ebp, Eip, SegCs, EFlags, Esp, SegSs;
	u8 ExtendedRegisters[512];
} CONTEXT;
typedef struct { EXCEPTION_RECORD *ExceptionRecord; CONTEXT *ContextRecord; } EXCEPTION_POINTERS;
typedef i32 (WINAPI *FILTRO_F)(EXCEPTION_POINTERS *);
FILTRO_F WINAPI SetUnhandledExceptionFilter(FILTRO_F);
void WINAPI RtlUnwind(void *, void *, EXCEPTION_RECORD *, void *);
BOOL WINAPI IsBadReadPtr(const void *, u32);
BOOL WINAPI IsBadWritePtr(void *, u32);
void *WINAPI VirtualAlloc(void *, u32, u32, u32);
BOOL WINAPI VirtualFree(void *, u32, u32);
BOOL WINAPI VirtualProtect(void *, u32, u32, u32 *);
HANDLE WINAPI CreateFileA(const char *, u32, u32, void *, u32, u32, HANDLE);
BOOL WINAPI ReadFile(HANDLE, void *, u32, u32 *, void *);
BOOL WINAPI WriteFile(HANDLE, const void *, u32, u32 *, void *);
u32 WINAPI GetCurrentDirectoryA(u32, char *);
BOOL WINAPI SetCurrentDirectoryA(const char *);

/* ---- ADVAPI32: el registro ---- */
#define HKEY_CLASSES_ROOT ((HKEY)0x80000000)
#define HKEY_CURRENT_USER ((HKEY)0x80000001)
#define HKEY_LOCAL_MACHINE ((HKEY)0x80000002)
#define HKEY_USERS ((HKEY)0x80000003)
#define REG_SZ 1
#define REG_EXPAND_SZ 2
#define REG_BINARY 3
#define REG_DWORD 4
#define KEY_ALL_ACCESS 0xF003F
#define KEY_READ 0x20019
i32 WINAPI RegOpenKeyExA(HKEY, const char *, u32, u32, HKEY *);
i32 WINAPI RegOpenKeyA(HKEY, const char *, HKEY *);
i32 WINAPI RegCreateKeyExA(HKEY, const char *, u32, char *, u32, u32, void *, HKEY *, u32 *);
i32 WINAPI RegCreateKeyA(HKEY, const char *, HKEY *);
i32 WINAPI RegCloseKey(HKEY);
i32 WINAPI RegQueryValueExA(HKEY, const char *, u32 *, u32 *, u8 *, u32 *);
i32 WINAPI RegSetValueExA(HKEY, const char *, u32, u32, const u8 *, u32);
i32 WINAPI RegQueryValueA(HKEY, const char *, char *, i32 *);
i32 WINAPI RegSetValueA(HKEY, const char *, u32, const char *, u32);
i32 WINAPI RegEnumKeyExA(HKEY, u32, char *, u32 *, u32 *, char *, u32 *, void *);
i32 WINAPI RegEnumKeyA(HKEY, u32, char *, u32);
i32 WINAPI RegEnumValueA(HKEY, u32, char *, u32 *, u32 *, u32 *, u8 *, u32 *);
i32 WINAPI RegDeleteValueA(HKEY, const char *);
i32 WINAPI RegDeleteKeyA(HKEY, const char *);
i32 WINAPI RegQueryInfoKeyA(HKEY, char *, u32 *, u32 *, u32 *, u32 *, u32 *, u32 *, u32 *, u32 *, u32 *, void *);
i32 WINAPI RegFlushKey(HKEY);

/* ---- USER32 ---- */
typedef struct { i32 left, top, right, bottom; } RECT;
typedef struct { i32 x, y; } POINT;
typedef struct { HWND hwnd; u32 message; u32 wParam; i32 lParam; u32 time; POINT pt; } MSG;
typedef i32 (WINAPI *WNDPROC)(HWND, u32, u32, i32);
typedef struct { u32 style; WNDPROC proc; int ce, we; HINSTANCE inst; HANDLE icono, cursor, fondo; const char *menu, *clase; } WNDCLASSA;
u16 WINAPI RegisterClassA(const WNDCLASSA *);
HWND WINAPI CreateWindowExA(u32, const char *, const char *, u32, int, int, int, int, HWND, HANDLE, HINSTANCE, void *);
BOOL WINAPI ShowWindow(HWND, int);
BOOL WINAPI UpdateWindow(HWND);
BOOL WINAPI DestroyWindow(HWND);
i32 WINAPI DefWindowProcA(HWND, u32, u32, i32);
BOOL WINAPI PeekMessageA(MSG *, HWND, u32, u32, u32);
BOOL WINAPI GetMessageA(MSG *, HWND, u32, u32);
BOOL WINAPI TranslateMessage(const MSG *);
i32 WINAPI DispatchMessageA(const MSG *);
void WINAPI PostQuitMessage(int);
BOOL WINAPI PostMessageA(HWND, u32, u32, i32);
int WINAPI ShowCursor(BOOL);
BOOL WINAPI SetCursorPos(int, int);
BOOL WINAPI GetCursorPos(POINT *);
BOOL WINAPI ClipCursor(const RECT *);
BOOL WINAPI GetClipCursor(RECT *);
u16 WINAPI GetAsyncKeyState(int);
u16 WINAPI GetKeyState(int);
int WINAPI GetSystemMetrics(int);
BOOL WINAPI GetClientRect(HWND, RECT *);
BOOL WINAPI GetWindowRect(HWND, RECT *);
BOOL WINAPI ClientToScreen(HWND, POINT *);
HWND WINAPI SetCapture(HWND);
BOOL WINAPI ReleaseCapture(void);
HWND WINAPI SetFocus(HWND);
HWND WINAPI GetForegroundWindow(void);
BOOL WINAPI SetForegroundWindow(HWND);
BOOL WINAPI SetWindowPos(HWND, HWND, int, int, int, int, u32);
int WINAPI MessageBoxA(HWND, const char *, const char *, u32);
typedef struct { char nombre[32]; u16 v1, v2, tam, extra; u32 campos, orient_o_x, y_o_algo; u16 color_o; char resto[42]; u32 bits, ancho, alto, flags, frec; } DEVMODEA_J;
i32 WINAPI ChangeDisplaySettingsA(void *, u32);
BOOL WINAPI EnumDisplaySettingsA(const char *, u32, void *);
HDC WINAPI GetDC(HWND);
int WINAPI ReleaseDC(HWND, HDC);
#define WM_DESTROY 2
#define WM_CLOSE 0x10
#define WM_KEYDOWN 0x100
#define WM_KEYUP 0x101
#define WM_CHAR 0x102
#define WM_MOUSEMOVE 0x200
#define WM_LBUTTONDOWN 0x201
#define WM_TIMER 0x113
#define WM_ACTIVATE 6
#define PM_REMOVE 1
#define WS_POPUP 0x80000000u
#define WS_VISIBLE 0x10000000u
#define WS_OVERLAPPEDWINDOW 0x00CF0000u

/* ---- WINMM ---- */
u32 WINAPI timeGetTime(void);
u32 WINAPI timeBeginPeriod(u32);
u32 WINAPI timeEndPeriod(u32);
u32 WINAPI mciSendStringA(const char *, char *, u32, HWND);
u32 WINAPI mciSendCommandA(u32, u32, u32, void *);
int WINAPI mciGetErrorStringA(u32, char *, u32);
typedef struct { u16 fmt, canales; u32 hz, octetos_s; u16 bloque, bits, extra; } WAVEFORMATEX;
typedef struct WAVEHDR { char *datos; u32 largo, grabados, usuario, flags, bucles; struct WAVEHDR *sig; u32 reservado; } WAVEHDR;
u32 WINAPI waveOutGetNumDevs(void);
u32 WINAPI waveOutOpen(HANDLE *, u32, const WAVEFORMATEX *, u32, u32, u32);
u32 WINAPI waveOutPrepareHeader(HANDLE, WAVEHDR *, u32);
u32 WINAPI waveOutUnprepareHeader(HANDLE, WAVEHDR *, u32);
u32 WINAPI waveOutWrite(HANDLE, WAVEHDR *, u32);
u32 WINAPI waveOutReset(HANDLE);
u32 WINAPI waveOutClose(HANDLE);
u32 WINAPI waveOutGetPosition(HANDLE, void *, u32);
u32 WINAPI midiOutGetNumDevs(void);
u32 WINAPI midiOutOpen(HANDLE *, u32, u32, u32, u32);
u32 WINAPI midiOutShortMsg(HANDLE, u32);
u32 WINAPI midiOutClose(HANDLE);

/* ---- WSOCK32 ---- */
typedef struct { u16 familia, puerto; u32 ip; char cero[8]; } SOCKADDR_IN;
typedef struct { u32 n; u32 s[64]; } FD_SET_J;
typedef struct { i32 seg, useg; } TIMEVAL;
typedef struct { char *nombre; char **alias; i16 tipo, largo; char **dirs; } HOSTENT;
typedef struct { u16 version, alta; char desc[257], estado[129]; u16 max_s, max_udp; char *info; } WSADATA;
int WINAPI WSAStartup(u16, WSADATA *);
int WINAPI WSACleanup(void);
int WINAPI WSAGetLastError(void);
u32 WINAPI socket(int, int, int);
int WINAPI bind(u32, const void *, int);
int WINAPI sendto(u32, const char *, int, int, const void *, int);
int WINAPI recvfrom(u32, char *, int, int, void *, int *);
int WINAPI select(int, FD_SET_J *, FD_SET_J *, FD_SET_J *, const TIMEVAL *);
int WINAPI ioctlsocket(u32, i32, u32 *);
int WINAPI closesocket(u32);
int WINAPI setsockopt(u32, int, int, const char *, int);
int WINAPI getsockname(u32, void *, int *);
HOSTENT *WINAPI gethostbyname(const char *);
int WINAPI gethostname(char *, int);
u16 WINAPI htons(u16);
u16 WINAPI ntohs(u16);
u32 WINAPI htonl(u32);
u32 WINAPI inet_addr(const char *);
char *WINAPI inet_ntoa(u32);
#define FIONBIO ((i32)0x8004667E)

static inline void pon_fs0(void *r) { __asm__ volatile("movl %0, %%fs:0" : : "r"(r) : "memory"); }
static inline void *lee_fs0(void) { void *r; __asm__ volatile("movl %%fs:0, %0" : "=r"(r)); return r; }
#endif
