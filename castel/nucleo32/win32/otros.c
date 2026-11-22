/* otros.c - WIN32.BIN: lo poco que piden los programas de ADVAPI32 (el
 * registro, desde JUEGOS, en registro.c), SHELL32, VERSION y WINSPOOL.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"
#define API(t) t WINAPI
u32 WINAPI GetModuleFileNameA(HMODULE h, char *d, u32 n);
BOOL WINAPI CreateProcessA(const char *, char *, void *, void *, BOOL, u32, void *, const char *, void *, void *);

/* (JUEGOS) el registro: registro.c */
API(BOOL) GetUserNameA(char *d, u32 *n)
{
	char b[64];
	b[0] = 0;
	sc(68, 1, LIN(b), 0);
	if (!b[0])
		strcpy(b, "Usuario");
	{
		char c[64];
		a1252(c, b, sizeof c);
		copia_n(d, c, *n);
	}
	*n = strlen(d) + 1;
	return TRUE;
}
/* (HL) La seguridad de Windows NT: Hispa-Pentium es como un Windows 98, sin
 * seguridad en los objetos: los descriptores no tienen dueño ni DACL (es
 * decir, «todo permitido»), cambiarlos no hace nada y no falla; no hay
 * testigos (OpenProcessToken: ERROR_CALL_NOT_IMPLEMENTED, como en el 98),
 * pero ajustar privilegios dice que sí. Los SID y las ACL sí se hacen y se
 * miran de verdad (con la forma pública de cada uno). */
API(BOOL) OpenProcessToken(HANDLE p, u32 a, HANDLE *t) { (void)p; (void)a; if (t) *t = 0; pon_error(120); return FALSE; }
API(BOOL) OpenThreadToken(HANDLE p, u32 a, BOOL s, HANDLE *t) { (void)p; (void)a; (void)s; if (t) *t = 0; pon_error(1008); return FALSE; }   /* ERROR_NO_TOKEN */
API(BOOL) GetTokenInformation(HANDLE t, u32 c, void *d, u32 n, u32 *r) { (void)t; (void)c; (void)d; (void)n; if (r) *r = 0; pon_error(120); return FALSE; }
API(BOOL) AdjustTokenPrivileges(HANDLE t, BOOL todos, void *nuevos, u32 n, void *antes, u32 *r) { (void)t; (void)todos; (void)nuevos; (void)n; (void)antes; if (r) *r = 0; pon_error(0); return TRUE; }
static const char *const privilegios[] = { "", "", "SeCreateTokenPrivilege", "SeAssignPrimaryTokenPrivilege", "SeLockMemoryPrivilege",
	"SeIncreaseQuotaPrivilege", "SeMachineAccountPrivilege", "SeTcbPrivilege", "SeSecurityPrivilege", "SeTakeOwnershipPrivilege",
	"SeLoadDriverPrivilege", "SeSystemProfilePrivilege", "SeSystemtimePrivilege", "SeProfileSingleProcessPrivilege",
	"SeIncreaseBasePriorityPrivilege", "SeCreatePagefilePrivilege", "SeCreatePermanentPrivilege", "SeBackupPrivilege",
	"SeRestorePrivilege", "SeShutdownPrivilege", "SeDebugPrivilege", "SeAuditPrivilege", "SeSystemEnvironmentPrivilege",
	"SeChangeNotifyPrivilege", "SeRemoteShutdownPrivilege" };
API(BOOL) LookupPrivilegeValueA(const char *sis, const char *n, u32 *luid)
{
	u32 k;
	(void)sis;
	for (k = 2; n && k < sizeof privilegios / sizeof privilegios[0]; k++)
		if (!stricmp_(privilegios[k], n)) {
			luid[0] = k;
			luid[1] = 0;
			return TRUE;
		}
	pon_error(1313);           /* ERROR_NO_SUCH_PRIVILEGE */
	return FALSE;
}
API(BOOL) LookupPrivilegeNameA(const char *sis, const u32 *luid, char *d, u32 *n)
{
	const char *p;
	u32 l;
	(void)sis;
	if (!luid || luid[0] < 2 || luid[0] >= sizeof privilegios / sizeof privilegios[0]) {
		pon_error(1313);
		return FALSE;
	}
	p = privilegios[luid[0]];
	l = strlen(p);
	if (!d || *n < l + 1) {
		*n = l + 1;
		pon_error(122);
		return FALSE;
	}
	strcpy(d, p);
	*n = l;
	return TRUE;
}
/* los SID: revisión 1, cuántas subautoridades, la autoridad (6) y ellas */
API(BOOL) AllocateAndInitializeSid(const u8 *a, u8 n, u32 s0, u32 s1, u32 s2, u32 s3, u32 s4, u32 s5, u32 s6, u32 s7, void **r)
{
	u32 sub[8] = { s0, s1, s2, s3, s4, s5, s6, s7 };
	u8 *s;
	if (!a || n > 8 || !r) {
		pon_error(87);
		return FALSE;
	}
	s = m_cero(8 + 4 * n);
	if (!s) {
		pon_error(8);
		return FALSE;
	}
	s[0] = 1;
	s[1] = n;
	memcpy(s + 2, a, 6);
	memcpy(s + 8, sub, 4 * n);
	*r = s;
	return TRUE;
}
API(void *) FreeSid(void *s) { if (s) m_soltar(s); return 0; }
API(BOOL) IsValidSid(const u8 *s) { return s && s[0] == 1 && s[1] <= 15; }
API(u32) GetLengthSid(const u8 *s) { return IsValidSid(s) ? 8 + 4u * s[1] : 0; }
API(BOOL) EqualSid(const u8 *a, const u8 *b) { return IsValidSid(a) && IsValidSid(b) && a[1] == b[1] && !memcmp(a, b, 8 + 4 * a[1]); }
API(BOOL) CopySid(u32 n, u8 *d, const u8 *s) { u32 l = GetLengthSid(s); if (!l || n < l) { pon_error(122); return FALSE; } memcpy(d, s, l); return TRUE; }
API(u32 *) GetSidSubAuthority(u8 *s, u32 i) { return (u32 *)(s + 8 + 4 * i); }
API(u8 *) GetSidSubAuthorityCount(u8 *s) { return s + 1; }
API(void *) GetSidIdentifierAuthority(u8 *s) { return s + 2; }
API(u32) GetSidLengthRequired(u8 n) { return 8 + 4u * n; }
API(BOOL) InitializeSid(u8 *s, const u8 *a, u8 n) { if (!s || !a || n > 15) return FALSE; s[0] = 1; s[1] = n; memcpy(s + 2, a, 6); memset(s + 8, 0, 4 * n); return TRUE; }
API(BOOL) CheckTokenMembership(HANDLE t, void *s, BOOL *es) { (void)t; (void)s; if (es) *es = TRUE; return TRUE; }   /* (todos lo son todo) */
/* las ACL: la cabecera (revisión, tamaño, cuántas) y sus ACE detrás */
API(BOOL) InitializeAcl(u8 *a, u32 n, u32 rev)
{
	if (!a || n < 8 || (rev != 2 && rev != 4)) {
		pon_error(87);
		return FALSE;
	}
	memset(a, 0, 8);
	a[0] = (u8)rev;
	*(u16 *)(a + 2) = (u16)n;
	return TRUE;
}
API(BOOL) IsValidAcl(const u8 *a) { return a && (a[0] == 2 || a[0] == 4) && *(const u16 *)(a + 2) >= 8; }
static BOOL ace_mas(u8 *a, u8 tipo, u8 fl, u32 masc, const u8 *sid)
{
	u32 l = GetLengthSid(sid), pos = 8, k, n;
	if (!IsValidAcl(a) || !l) {
		pon_error(87);
		return FALSE;
	}
	n = *(u16 *)(a + 4);
	for (k = 0; k < n; k++)
		pos += *(u16 *)(a + pos + 2);
	if (pos + 8 + l > *(u16 *)(a + 2)) {
		pon_error(1344);       /* ERROR_ALLOTTED_SPACE_EXCEEDED */
		return FALSE;
	}
	a[pos] = tipo;
	a[pos + 1] = fl;
	*(u16 *)(a + pos + 2) = (u16)(8 + l);
	*(u32 *)(a + pos + 4) = masc;
	memcpy(a + pos + 8, sid, l);
	*(u16 *)(a + 4) = (u16)(n + 1);
	return TRUE;
}
API(BOOL) AddAccessAllowedAce(u8 *a, u32 rev, u32 m, const u8 *s) { (void)rev; return ace_mas(a, 0, 0, m, s); }
API(BOOL) AddAccessDeniedAce(u8 *a, u32 rev, u32 m, const u8 *s) { (void)rev; return ace_mas(a, 1, 0, m, s); }
API(BOOL) GetAclInformation(const u8 *a, u32 *d, u32 n, u32 c)
{
	if (!IsValidAcl(a) || !d)
		return FALSE;
	if (c == 1 && n >= 4) {            /* AclRevisionInformation */
		d[0] = a[0];
		return TRUE;
	}
	if (c == 2 && n >= 12) {           /* AclSizeInformation: cuántas, usado, libre */
		u32 k, pos = 8;
		for (k = 0; k < *(const u16 *)(a + 4); k++)
			pos += *(const u16 *)(a + pos + 2);
		d[0] = *(const u16 *)(a + 4);
		d[1] = pos;
		d[2] = *(const u16 *)(a + 2) - pos;
		return TRUE;
	}
	pon_error(87);
	return FALSE;
}
/* los descriptores (SECURITY_DESCRIPTOR de 20 octetos): sin dueño, sin
 * grupo y sin DACL: todo permitido */
API(BOOL) InitializeSecurityDescriptor(u8 *d, u32 rev)
{
	if (!d || rev != 1) {
		pon_error(1338);       /* ERROR_INVALID_SECURITY_DESCR */
		return FALSE;
	}
	memset(d, 0, 20);
	d[0] = 1;
	return TRUE;
}
API(BOOL) IsValidSecurityDescriptor(const u8 *d) { return d && d[0] == 1; }
API(u32) GetSecurityDescriptorLength(const u8 *d) { return IsValidSecurityDescriptor(d) ? 20 : 0; }
API(BOOL) GetSecurityDescriptorControl(const u8 *d, u16 *c, u32 *rev)
{
	if (!IsValidSecurityDescriptor(d)) {
		pon_error(1338);
		return FALSE;
	}
	if (c)
		*c = *(const u16 *)(d + 2);
	if (rev)
		*rev = 1;
	return TRUE;
}
API(BOOL) SetSecurityDescriptorControl(u8 *d, u16 m, u16 v) { if (!IsValidSecurityDescriptor(d)) return FALSE; *(u16 *)(d + 2) = (u16)((*(u16 *)(d + 2) & ~m) | (v & m)); return TRUE; }
API(BOOL) GetSecurityDescriptorOwner(const u8 *d, void **o, BOOL *def) { if (!IsValidSecurityDescriptor(d)) { pon_error(1338); return FALSE; } if (o) *o = 0; if (def) *def = FALSE; return TRUE; }
API(BOOL) GetSecurityDescriptorGroup(const u8 *d, void **o, BOOL *def) { return GetSecurityDescriptorOwner(d, o, def); }
API(BOOL) GetSecurityDescriptorDacl(const u8 *d, BOOL *hay, void **a, BOOL *def) { if (!IsValidSecurityDescriptor(d)) { pon_error(1338); return FALSE; } if (hay) *hay = FALSE; if (a) *a = 0; if (def) *def = FALSE; return TRUE; }
API(BOOL) GetSecurityDescriptorSacl(const u8 *d, BOOL *hay, void **a, BOOL *def) { return GetSecurityDescriptorDacl(d, hay, a, def); }
API(BOOL) SetSecurityDescriptorDacl(u8 *d, BOOL hay, void *a, BOOL def) { (void)hay; (void)a; (void)def; return IsValidSecurityDescriptor(d); }
API(BOOL) SetSecurityDescriptorSacl(u8 *d, BOOL hay, void *a, BOOL def) { (void)hay; (void)a; (void)def; return IsValidSecurityDescriptor(d); }
API(BOOL) SetSecurityDescriptorOwner(u8 *d, void *o, BOOL def) { (void)o; (void)def; return IsValidSecurityDescriptor(d); }
API(BOOL) SetSecurityDescriptorGroup(u8 *d, void *o, BOOL def) { (void)o; (void)def; return IsValidSecurityDescriptor(d); }
/* la de un objeto o un archivo: un descriptor vacío (todo permitido) */
static BOOL da_descriptor(u8 *d, u32 n, u32 *nec)
{
	if (nec)
		*nec = 20;
	if (!d || n < 20) {
		pon_error(122);
		return FALSE;
	}
	return InitializeSecurityDescriptor(d, 1);
}
API(BOOL) GetKernelObjectSecurity(HANDLE h, u32 que, u8 *d, u32 n, u32 *nec) { (void)h; (void)que; return da_descriptor(d, n, nec); }
API(BOOL) SetKernelObjectSecurity(HANDLE h, u32 que, const u8 *d) { (void)h; (void)que; (void)d; return TRUE; }
API(BOOL) GetFileSecurityA(const char *f, u32 que, u8 *d, u32 n, u32 *nec) { (void)f; (void)que; return da_descriptor(d, n, nec); }
API(BOOL) SetFileSecurityA(const char *f, u32 que, const u8 *d) { (void)f; (void)que; (void)d; return TRUE; }
API(BOOL) GetUserObjectSecurity(HANDLE h, u32 *que, u8 *d, u32 n, u32 *nec) { (void)h; (void)que; return da_descriptor(d, n, nec); }
API(BOOL) SetUserObjectSecurity(HANDLE h, u32 *que, const u8 *d) { (void)h; (void)que; (void)d; return TRUE; }

/* == SHELL32 == */
API(void) DragAcceptFiles(HWND h, BOOL a) { (void)h; (void)a; }
API(u32) DragQueryFile(HANDLE d, u32 i, char *b, u32 n) { (void)d; (void)i; (void)b; (void)n; return 0; }
API(u32) DragQueryFileA(HANDLE d, u32 i, char *b, u32 n) { (void)d; (void)i; (void)b; (void)n; return 0; }
API(void) DragFinish(HANDLE d) { (void)d; }
API(HINSTANCE) ShellExecuteA(HWND h, const char *op, const char *f, const char *p, const char *dir, i32 v)
{
	char l[400];
	(void)h; (void)op; (void)v;
	fmt(l, sizeof l, "%s%s%s", f ? f : "", p ? " " : "", p ? p : "");
	if (CreateProcessA(0, l, 0, 0, 0, 0, 0, dir, 0, 0))
		return (HINSTANCE)33;
	return (HINSTANCE)2;
}
API(void *) SHBrowseForFolderA(void *b) { (void)b; return 0; }
API(BOOL) SHGetPathFromIDListA(void *l, char *d) { (void)l; d[0] = 0; return FALSE; }
API(i32) SHGetSpecialFolderPathA(HWND h, char *d, i32 c, BOOL cr) { (void)h; (void)c; (void)cr; strcpy(d, carpeta_w32); return TRUE; }
API(i32) SHGetFolderPathA(HWND h, i32 c, HANDLE t, u32 f, char *d) { (void)h; (void)c; (void)t; (void)f; strcpy(d, carpeta_w32); if (strlen(d) > 3) d[strlen(d) - 1] = 0; return 0; }
API(HICON) ExtractIconA(HINSTANCE i, const char *f, u32 n) { (void)i; (void)f; (void)n; return 0; }
API(u32) SHGetFileInfoA(const char *p, u32 a, void *i, u32 n, u32 f) { (void)p; (void)a; (void)f; memset(i, 0, n); return 0; }
API(BOOL) Shell_NotifyIconA(u32 m, void *d) { (void)m; (void)d; return TRUE; }
API(void) SHChangeNotify(i32 e, u32 f, const void *a, const void *b) { (void)e; (void)f; (void)a; (void)b; }

/* == VERSION == */
API(u32) GetFileVersionInfoSizeA(const char *f, u32 *h) { (void)f; if (h) *h = 0; return 0; }
API(BOOL) GetFileVersionInfoA(const char *f, u32 h, u32 n, void *d) { (void)f; (void)h; (void)n; (void)d; return FALSE; }
API(BOOL) VerQueryValueA(const void *b, const char *s, void **d, u32 *n) { (void)b; (void)s; *d = 0; *n = 0; return FALSE; }

/* == WINSPOOL == */
API(BOOL) EnumPrintersA(u32 f, char *n, u32 l, u8 *b, u32 c, u32 *nec, u32 *r) { (void)f; (void)n; (void)l; (void)b; (void)c; *nec = 0; *r = 0; return TRUE; }
API(BOOL) OpenPrinterA(char *n, HANDLE *h, void *d) { (void)n; (void)d; *h = 0; return FALSE; }
API(BOOL) ClosePrinter(HANDLE h) { (void)h; return TRUE; }
/* (HL) WINSPOOL: no hay impresoras (DocumentProperties dice que esa no está) */
API(i32) DocumentPropertiesA(HWND h, HANDLE p, char *n, void *sal, void *ent, u32 modo) { (void)h; (void)p; (void)n; (void)sal; (void)ent; (void)modo; pon_error(1801); return -1; }   /* ERROR_INVALID_PRINTER_NAME */
API(i32) DocumentPropertiesW(HWND h, HANDLE p, WCHAR *n, void *sal, void *ent, u32 modo) { (void)h; (void)p; (void)n; (void)sal; (void)ent; (void)modo; pon_error(1801); return -1; }
API(i32) DeviceCapabilitiesA(const char *d, const char *p, u16 c, char *s, const void *m) { (void)d; (void)p; (void)c; (void)s; (void)m; pon_error(1801); return -1; }
API(i32) AdvancedDocumentPropertiesA(HWND h, HANDLE p, char *n, void *sal, void *ent) { (void)h; (void)p; (void)n; (void)sal; (void)ent; return 0; }
API(BOOL) GetPrinterA(HANDLE p, u32 n, u8 *b, u32 c, u32 *nec) { (void)p; (void)n; (void)b; (void)c; if (nec) *nec = 0; pon_error(6); return FALSE; }
API(BOOL) GetDefaultPrinterA(char *b, u32 *n) { (void)b; if (n) *n = 0; pon_error(2); return FALSE; }   /* ERROR_FILE_NOT_FOUND: no hay */
API(BOOL) EnumPrintersW(u32 f, WCHAR *n, u32 l, u8 *b, u32 c, u32 *nec, u32 *r) { (void)f; (void)n; (void)l; (void)b; (void)c; *nec = 0; *r = 0; return TRUE; }
API(BOOL) OpenPrinterW(WCHAR *n, HANDLE *h, void *d) { (void)n; (void)d; *h = 0; pon_error(1801); return FALSE; }
API(u32) StartDocPrinterA(HANDLE p, u32 n, void *d) { (void)p; (void)n; (void)d; pon_error(6); return 0; }
API(BOOL) EndDocPrinter(HANDLE p) { (void)p; pon_error(6); return FALSE; }
API(BOOL) StartPagePrinter(HANDLE p) { (void)p; pon_error(6); return FALSE; }
API(BOOL) EndPagePrinter(HANDLE p) { (void)p; pon_error(6); return FALSE; }
API(BOOL) WritePrinter(HANDLE p, void *b, u32 n, u32 *e) { (void)p; (void)b; (void)n; if (e) *e = 0; pon_error(6); return FALSE; }
