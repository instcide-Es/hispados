/* hl.h - (HL) lo común de las pruebas de lo que pedía Half-Life 1 al pasar
 * WIN32 COMPRUEBA (22/11/2026): las declaraciones de Win32 que faltaban,
 * con los nombres y los números de la documentación pública de Microsoft.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HL_H
#define HL_H
#include "j.h"
typedef u16 WCHAR;
#define TRUE 1
#define FALSE 0
/* KERNEL32 */
typedef struct { u8 n, fijo; u16 error, r1, r2; char ruta[128]; } OFSTRUCT;
HANDLE WINAPI OpenFile(const char *, OFSTRUCT *, u32);
HANDLE WINAPI _lopen(const char *, int);
HANDLE WINAPI _lcreat(const char *, int);
u32 WINAPI _lread(HANDLE, void *, u32);
u32 WINAPI _lwrite(HANDLE, const char *, u32);
i32 WINAPI _llseek(HANDLE, i32, int);
u32 WINAPI _lclose(HANDLE);
HANDLE WINAPI CreateFileMappingA(HANDLE, void *, u32, u32, u32, const char *);
HANDLE WINAPI OpenFileMappingA(u32, BOOL, const char *);
void *WINAPI MapViewOfFile(HANDLE, u32, u32, u32, u32);
BOOL WINAPI UnmapViewOfFile(const void *);
BOOL WINAPI SetEnvironmentVariableW(const WCHAR *, const WCHAR *);
u32 WINAPI GetEnvironmentVariableA(const char *, char *, u32);
BOOL WINAPI SetVolumeLabelA(const char *, const char *);
u32 WINAPI GetProcessVersion(u32);
void WINAPI FatalAppExitA(u32, const char *);
BOOL WINAPI EnumSystemLocalesA(BOOL (WINAPI *)(char *), u32);
BOOL WINAPI DeleteFileA(const char *);
u32 WINAPI ntohl(u32);
/* USER32 */
HWND WINAPI GetLastActivePopup(HWND);
int WINAPI ExcludeUpdateRgn(HDC, HWND);
u32 WINAPI WaitForInputIdle(HANDLE, u32);
BOOL WINAPI ExitWindowsEx(u32, u32);
u32 WINAPI DdeInitializeA(u32 *, void *, u32, u32);
BOOL WINAPI DdeUninitialize(u32);
HANDLE WINAPI DdeCreateStringHandleA(u32, const char *, int);
u32 WINAPI DdeQueryStringA(u32, HANDLE, char *, u32, int);
BOOL WINAPI DdeFreeStringHandle(u32, HANDLE);
HANDLE WINAPI DdeConnect(u32, HANDLE, HANDLE, void *);
u32 WINAPI DdeGetLastError(u32);
HANDLE WINAPI DdeCreateDataHandle(u32, const void *, u32, u32, HANDLE, u32, u32);
u32 WINAPI DdeGetData(HANDLE, void *, u32, u32);
BOOL WINAPI DdeFreeDataHandle(HANDLE);
HANDLE WINAPI DdeClientTransaction(void *, u32, HANDLE, HANDLE, u32, u32, u32, u32 *);
i32 WINAPI PackDDElParam(u32, u32, u32);
BOOL WINAPI UnpackDDElParam(u32, i32, u32 *, u32 *);
i32 WINAPI ReuseDDElParam(i32, u32, u32, u32, u32);
BOOL WINAPI FreeDDElParam(u32, i32);
BOOL WINAPI InvalidateRect(HWND, const RECT *, BOOL);
BOOL WINAPI ValidateRect(HWND, const RECT *);
/* GDI32 */
typedef struct { i32 cx, cy; } SIZE;
HDC WINAPI CreateCompatibleDC(HDC);
BOOL WINAPI DeleteDC(HDC);
BOOL WINAPI ScaleWindowExtEx(HDC, int, int, int, int, SIZE *);
BOOL WINAPI ScaleViewportExtEx(HDC, int, int, int, int, SIZE *);
int WINAPI Escape(HDC, int, int, const void *, void *);
u32 WINAPI GetBoundsRect(HDC, RECT *, u32);
int WINAPI GetClipBox(HDC, RECT *);
/* WINSPOOL.DRV */
i32 WINAPI DocumentPropertiesA(HWND, HANDLE, char *, void *, void *, u32);
/* ADVAPI32: la seguridad */
BOOL WINAPI InitializeSecurityDescriptor(void *, u32);
u32 WINAPI GetSecurityDescriptorLength(void *);
BOOL WINAPI IsValidSecurityDescriptor(void *);
BOOL WINAPI GetSecurityDescriptorOwner(void *, void **, BOOL *);
BOOL WINAPI GetSecurityDescriptorGroup(void *, void **, BOOL *);
BOOL WINAPI GetSecurityDescriptorDacl(void *, BOOL *, void **, BOOL *);
BOOL WINAPI GetSecurityDescriptorSacl(void *, BOOL *, void **, BOOL *);
BOOL WINAPI GetSecurityDescriptorControl(void *, u16 *, u32 *);
BOOL WINAPI SetSecurityDescriptorDacl(void *, BOOL, void *, BOOL);
BOOL WINAPI IsValidSid(void *);
BOOL WINAPI IsValidAcl(void *);
BOOL WINAPI InitializeAcl(void *, u32, u32);
BOOL WINAPI AddAccessAllowedAce(void *, u32, u32, void *);
BOOL WINAPI AllocateAndInitializeSid(const u8 *, u8, u32, u32, u32, u32, u32, u32, u32, u32, void **);
void *WINAPI FreeSid(void *);
u32 WINAPI GetLengthSid(void *);
BOOL WINAPI EqualSid(void *, void *);
BOOL WINAPI GetKernelObjectSecurity(HANDLE, u32, void *, u32, u32 *);
BOOL WINAPI SetKernelObjectSecurity(HANDLE, u32, void *);
BOOL WINAPI GetFileSecurityA(const char *, u32, void *, u32, u32 *);
BOOL WINAPI SetFileSecurityA(const char *, u32, void *);
BOOL WINAPI OpenProcessToken(HANDLE, u32, HANDLE *);
BOOL WINAPI AdjustTokenPrivileges(HANDLE, BOOL, void *, u32, void *, u32 *);
BOOL WINAPI LookupPrivilegeValueA(const char *, const char *, u32 *);
HANDLE WINAPI GetCurrentProcess(void);
/* WINMM */
typedef struct { u32 fl, fcc; void *proc; u32 err; HANDLE tarea; i32 cb; u8 *buf, *sig, *fin_l, *fin_e; i32 boff, doff; u32 info[3], r1, r2; HANDLE h; } MMIOINFO;
typedef struct { u32 id, tam, tipo, datos, fl; } MMCKINFO;
#define FCC(a, b, c, d) ((u32)(a) | (u32)(b) << 8 | (u32)(c) << 16 | (u32)(d) << 24)
HANDLE WINAPI mmioOpenA(char *, MMIOINFO *, u32);
u32 WINAPI mmioClose(HANDLE, u32);
i32 WINAPI mmioRead(HANDLE, char *, i32);
i32 WINAPI mmioWrite(HANDLE, const char *, i32);
i32 WINAPI mmioSeek(HANDLE, i32, int);
u32 WINAPI mmioDescend(HANDLE, MMCKINFO *, const MMCKINFO *, u32);
u32 WINAPI mmioAscend(HANDLE, MMCKINFO *, u32);
u32 WINAPI mmioCreateChunk(HANDLE, MMCKINFO *, u32);
u32 WINAPI mmioGetInfo(HANDLE, MMIOINFO *, u32);
u32 WINAPI mmioSetInfo(HANDLE, const MMIOINFO *, u32);
u32 WINAPI mmioAdvance(HANDLE, MMIOINFO *, u32);
u32 WINAPI mmioStringToFOURCCA(const char *, u32);
typedef struct { u16 wMid, wPid; u32 ver; char nombre[32]; u32 soporte, ndest; } MIXERCAPSA;
typedef struct { u32 tam, destino, fuente, linea, fl, usuario, tipo, canales, conexiones, controles; char corto[16], nombre[64];
                 struct { u32 tipo, id; u16 wMid, wPid; u32 ver; char nombre[32]; } dest; } MIXERLINEA;
typedef struct { u32 tam, id, tipo, fl, n; char corto[16], nombre[64]; i32 min, max; u32 r1[4]; u32 pasos; u32 r2[5]; } MIXERCONTROLA;
typedef struct { u32 tam, linea, idtipo, n, tamc; MIXERCONTROLA *c; } MIXERLINECONTROLSA;
typedef struct { u32 tam, id, canales, mult, tamd; void *d; } MIXERCONTROLDETAILS;
u32 WINAPI mixerGetNumDevs(void);
u32 WINAPI mixerOpen(HANDLE *, u32, u32, u32, u32);
u32 WINAPI mixerClose(HANDLE);
u32 WINAPI mixerGetDevCapsA(u32, MIXERCAPSA *, u32);
u32 WINAPI mixerGetLineInfoA(HANDLE, MIXERLINEA *, u32);
u32 WINAPI mixerGetLineControlsA(HANDLE, MIXERLINECONTROLSA *, u32);
u32 WINAPI mixerGetControlDetailsA(HANDLE, MIXERCONTROLDETAILS *, u32);
u32 WINAPI mixerSetControlDetails(HANDLE, MIXERCONTROLDETAILS *, u32);
u32 WINAPI mciGetDeviceIDA(const char *);
u32 WINAPI waveInGetNumDevs(void);
u32 WINAPI waveInOpen(HANDLE *, u32, const void *, u32, u32, u32);
u32 WINAPI waveInGetDevCapsA(u32, void *, u32);
u32 WINAPI waveInGetErrorTextA(u32, char *, u32);
u32 WINAPI midiOutGetDevCapsA(u32, void *, u32);
u32 WINAPI auxGetNumDevs(void);
u32 WINAPI auxGetDevCapsA(u32, void *, u32);
u32 WINAPI auxSetVolume(u32, u32);
/* WININET */
HANDLE WINAPI InternetOpenA(const char *, u32, const char *, const char *, u32);
HANDLE WINAPI InternetOpenUrlA(HANDLE, const char *, const char *, u32, u32, u32);
HANDLE WINAPI InternetConnectA(HANDLE, const char *, u16, const char *, const char *, u32, u32, u32);
HANDLE WINAPI HttpOpenRequestA(HANDLE, const char *, const char *, const char *, const char *, const char **, u32, u32);
BOOL WINAPI HttpSendRequestA(HANDLE, const char *, u32, void *, u32);
BOOL WINAPI HttpQueryInfoA(HANDLE, u32, void *, u32 *, u32 *);
BOOL WINAPI InternetReadFile(HANDLE, void *, u32, u32 *);
BOOL WINAPI InternetCloseHandle(HANDLE);
BOOL WINAPI InternetGetConnectedState(u32 *, u32);
typedef struct { u32 tam; char *esquema; u32 nesquema; i32 tipo; char *host; u32 nhost; u16 puerto; char *usuario; u32 nusuario;
                 char *clave; u32 nclave; char *camino; u32 ncamino; char *extra; u32 nextra; } URL_COMPONENTSA;
BOOL WINAPI InternetCrackUrlA(const char *, u32, u32, URL_COMPONENTSA *);
#endif
