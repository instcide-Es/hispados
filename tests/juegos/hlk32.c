/* HLK32.EXE - (HL) prueba de las funciones de KERNEL32, USER32, GDI32,
 * WINSPOOL.DRV y ADVAPI32 que pedía Half-Life 1 y que WIN32.BIN no tenía
 * (WIN32 COMPRUEBA, 22/11/2026): OpenFile y las de 16 bits (_lopen,
 * _lcreat, _lread, _lwrite, _llseek), OpenFileMapping por su nombre,
 * SetEnvironmentVariableW, SetVolumeLabel, GetProcessVersion,
 * EnumSystemLocales; GetLastActivePopup, WaitForInputIdle, ExitWindowsEx,
 * DDE (que falla limpio) y los lParam de DDE; ScaleWindowExtEx,
 * ScaleViewportExtEx, Escape y GetBoundsRect; DocumentProperties; los
 * descriptores de seguridad, los SID, las ACL y los privilegios. Con
 * «/FATAL», FatalAppExit. Una línea por cosa: la prueba (parte 33) las mira.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hl.h"

static int nlocales, hay_es;
static BOOL WINAPI un_locale(char *l)
{
	nlocales++;
	if (!strcmp(l, "00000C0A"))
		hay_es = 1;
	return TRUE;
}

int main(int argc, char **argv)
{
	OFSTRUCT of;
	HANDLE h, m1, m2;
	char b[64];
	u32 n;
	if (argc > 1 && !strcmp(argv[1], "/FATAL")) {
		printf("fatal: antes\n");
		fflush(stdout);
		FatalAppExitA(0, "No se puede seguir (prueba)");
		printf("MAL: FatalAppExit ha vuelto\n");
		return 0;
	}
	/* ---- KERNEL32 ---- */
	h = _lcreat("HLK32.TMP", 0);
	n = _lwrite(h, "Half-Life 1998", 14);
	printf("lcreat: %s %u\n", h != (HANDLE)-1 ? "si" : "no", n);
	{ int a = _llseek(h, 5, 0), c = _llseek(h, 0, 2); printf("llseek: %d %d\n", a, c); }
	_lclose(h);
	h = _lopen("HLK32.TMP", 0);
	memset(b, 0, sizeof b);
	_llseek(h, 5, 0);
	n = _lread(h, b, 4);
	{ int sololee = _lwrite(h, "x", 1) == 0xFFFFFFFF; printf("lopen: %u [%s] %d\n", n, b, sololee); }
	_lclose(h);
	printf("lopen_nohay: %d\n", _lopen("NOHAY.TMP", 0) == (HANDLE)-1 ? -1 : 0);
	h = OpenFile("HLK32.TMP", &of, 0x4000);              /* OF_EXIST */
	printf("openfile_existe: %d %s\n", (int)h, strstr(of.ruta, "\\JUEGOS\\HLK32.TMP") ? "ruta entera" : of.ruta);
	h = OpenFile("NOHAY.TMP", &of, 0);
	printf("openfile_nohay: %d %u\n", (int)h, of.error);
	h = OpenFile("HLK32.TMP", &of, 0);                    /* OF_READ */
	memset(b, 0, sizeof b);
	n = _lread(h, b, 4);
	_lclose(h);
	printf("openfile_lee: %u [%s]\n", n, b);
	h = OpenFile("HLK32.TMP", &of, 0x200);                /* OF_DELETE */
	{ int ya = OpenFile("HLK32.TMP", &of, 0x4000) == (HANDLE)-1 ? -1 : 0; printf("openfile_borra: %d %d\n", (int)h, ya); }
	/* un mapa con nombre: el mismo para los dos */
	m1 = CreateFileMappingA((HANDLE)-1, 0, 4, 0, 4096, "HL_Compartido");
	{
		char *p1 = MapViewOfFile(m1, 2, 0, 0, 0), *p2;
		strcpy(p1, "visto por los dos");
		m2 = OpenFileMappingA(2, 0, "HL_Compartido");
		p2 = m2 ? MapViewOfFile(m2, 2, 0, 0, 0) : 0;
		printf("openfilemapping: %s [%s]\n", m2 ? "si" : "no", p2 ? p2 : "");
		CloseHandle(m1);
		printf("mapa_sigue: [%s]\n", p2 ? p2 : "");
		{
			HANDLE m3 = CreateFileMappingA((HANDLE)-1, 0, 4, 0, 4096, "HL_Compartido");
			printf("mapa_ya_existe: %u\n", GetLastError());
			CloseHandle(m3);
		}
		CloseHandle(m2);
		{ HANDLE x = OpenFileMappingA(2, 0, "HL_NoHay"); u32 e = GetLastError(); printf("openfilemapping_nohay: %s %u\n", x ? "MAL" : "nulo", e); }
	}
	{
		static const WCHAR n[] = { 'H', 'L', '_', 'V', 'A', 'R', 0 }, v[] = { 'v', 'a', 'l', 'v', 'e', 0 };
		memset(b, 0, sizeof b);
		printf("setenvw: %d", SetEnvironmentVariableW(n, v));
		GetEnvironmentVariableA("HL_VAR", b, sizeof b);
		printf(" [%s]\n", b);
	}
	{ int r = SetVolumeLabelA("C:\\", "HALFLIFE"); u32 e = GetLastError(); printf("setvolumelabel: %d %u\n", r, e); }
	printf("getprocessversion: %08x %08x\n", GetProcessVersion(0), GetProcessVersion(GetCurrentProcessId()));
	{ int r = EnumSystemLocalesA(un_locale, 2); printf("enumsystemlocales: %d %d %s\n", r, nlocales, hay_es ? "con el de Espa\xf1a" : "MAL"); }
	/* ---- USER32 ---- */
	printf("waitforinputidle: %u\n", WaitForInputIdle(GetCurrentProcess(), 1000));
	{ int r = ExitWindowsEx(1, 0); u32 e = GetLastError(); printf("exitwindowsex: %d %u\n", r, e); }
	{
		u32 id = 0, lo = 0, hi = 0;
		HANDLE s1, s2, d;
		i32 lp;
		{ u32 r = DdeInitializeA(&id, 0, 0, 0); printf("ddeinitialize: %u %s\n", r, id ? "con id" : "sin id"); }
		s1 = DdeCreateStringHandleA(id, "Servidor", 1004);
		s2 = DdeCreateStringHandleA(id, "Tema", 1004);
		memset(b, 0, sizeof b);
		{ u32 r = DdeQueryStringA(id, s1, b, sizeof b, 1004); printf("ddestring: %u [%s] %s\n", r, b, s1 != s2 ? "distintas" : "MAL"); }
		{ HANDLE c = DdeConnect(id, s1, s2, 0); u32 e = DdeGetLastError(id); printf("ddeconnect: %s %x\n", c ? "MAL" : "nulo", e); }
		printf("ddetransaction: %s\n", DdeClientTransaction("x", 1, 0, s2, 1, 0x4050, 1000, 0) ? "MAL" : "nulo");
		d = DdeCreateDataHandle(id, "datos de dde", 13, 0, 0, 1, 0);
		memset(b, 0, sizeof b);
		{ u32 r = DdeGetData(d, b, sizeof b, 0); printf("ddedata: %u [%s]\n", r, b); }
		DdeFreeDataHandle(d);
		DdeFreeStringHandle(id, s1);
		DdeFreeStringHandle(id, s2);
		printf("ddeuninitialize: %d\n", DdeUninitialize(id));
		lp = PackDDElParam(0x3E4, 0x1234, 0x56789ABC);    /* WM_DDE_ACK */
		{ int r = UnpackDDElParam(0x3E4, lp, &lo, &hi); printf("unpackddelparam: %d %x %x\n", r, lo, hi); }
		lp = ReuseDDElParam(lp, 0x3E4, 0x3E5, 7, 0x11223344);   /* a WM_DDE_DATA */
		UnpackDDElParam(0x3E5, lp, &lo, &hi);
		{ int r = FreeDDElParam(0x3E5, lp); printf("reuseddelparam: %x %x %d\n", lo, hi, r); }
	}
	printf("getlastactivepopup_nohay: %s\n", GetLastActivePopup((HWND)0x1234) ? "MAL" : "nulo");
	/* ---- GDI32 ---- */
	{
		HDC dc = CreateCompatibleDC(0);
		SIZE s = { 9, 9 };
		RECT r = { 1, 1, 1, 1 };
		int q = 1;
		{ int r = ScaleWindowExtEx(dc, 2, 1, 2, 1, &s); printf("scalewindowextex: %d %d %d\n", r, s.cx, s.cy); }
		s.cx = s.cy = 9;
		{ int r = ScaleViewportExtEx(dc, 1, 2, 1, 2, &s); printf("scaleviewportextex: %d %d %d\n", r, s.cx, s.cy); }
		q = 20;
		{ int a = Escape(dc, 8, 4, &q, 0), c = Escape(dc, 10, 0, 0, 0); printf("escape: %d %d\n", a, c); }    /* QUERYESCSUPPORT, STARTDOC */
		{ u32 x = GetBoundsRect(dc, &r, 0); printf("getboundsrect: %u %d %d\n", x, r.left, r.right); }
		DeleteDC(dc);
	}
	/* ---- WINSPOOL.DRV ---- */
	{ int r = DocumentPropertiesA(0, 0, "Impresora", 0, 0, 0); u32 e = GetLastError(); printf("documentproperties: %d %u\n", r, e); }
	/* ---- ADVAPI32: la seguridad ---- */
	{
		u8 sd[20], sd2[64], acl[128];
		void *o = (void *)1, *dacl = (void *)1, *sid = 0, *sid2 = 0;
		BOOL def = 1, hay = 1;
		u16 ctl = 9;
		u32 rev = 0, nec = 0, luid[2] = { 0, 0 };
		static const u8 nt[6] = { 0, 0, 0, 0, 0, 5 };
		HANDLE tok = (HANDLE)1;
		int r1, r2;
		r1 = InitializeSecurityDescriptor(sd, 1);
		printf("initializesd: %d %u %d\n", r1, GetSecurityDescriptorLength(sd), IsValidSecurityDescriptor(sd));
		r1 = GetSecurityDescriptorOwner(sd, &o, &def);
		printf("sd_owner: %d %s %d\n", r1, o ? "MAL" : "nulo", def);
		o = (void *)1;
		r1 = GetSecurityDescriptorGroup(sd, &o, &def);
		printf("sd_group: %d %s\n", r1, o ? "MAL" : "nulo");
		r1 = GetSecurityDescriptorDacl(sd, &hay, &dacl, &def);
		printf("sd_dacl: %d %d %s\n", r1, hay, dacl ? "MAL" : "nulo");
		hay = 1;
		r1 = GetSecurityDescriptorSacl(sd, &hay, &dacl, &def);
		printf("sd_sacl: %d %d\n", r1, hay);
		r1 = GetSecurityDescriptorControl(sd, &ctl, &rev);
		printf("sd_control: %d %u %u\n", r1, ctl, rev);
		printf("sd_pon_dacl: %d\n", SetSecurityDescriptorDacl(sd, 1, 0, 0));
		r1 = AllocateAndInitializeSid(nt, 2, 32, 544, 0, 0, 0, 0, 0, 0, &sid);
		printf("sid: %d %d %u\n", r1, IsValidSid(sid), GetLengthSid(sid));
		AllocateAndInitializeSid(nt, 2, 32, 544, 0, 0, 0, 0, 0, 0, &sid2);
		printf("equalsid: %d\n", EqualSid(sid, sid2));
		r1 = InitializeAcl(acl, sizeof acl, 2);
		r2 = IsValidAcl(acl);
		{ int r3 = AddAccessAllowedAce(acl, 2, 0x10000000, sid); printf("acl: %d %d %d %d\n", r1, r2, r3, acl[4]); }
		printf("acl_mala: %d\n", IsValidAcl(sd));
		FreeSid(sid);
		FreeSid(sid2);
		{
			u32 e;
			r1 = GetKernelObjectSecurity(GetCurrentProcess(), 4, sd2, 4, &nec);
			e = GetLastError();
			printf("kernelobjectsecurity: %d %u %u", r1, e, nec);
			r1 = GetKernelObjectSecurity(GetCurrentProcess(), 4, sd2, sizeof sd2, &nec);
			r2 = SetKernelObjectSecurity(GetCurrentProcess(), 4, sd2);
			printf(" %d %d\n", r1, r2);
			r1 = GetFileSecurityA("C:\\JUEGOS\\HLK32.EXE", 4, sd2, sizeof sd2, &nec);
			r2 = SetFileSecurityA("C:\\JUEGOS\\HLK32.EXE", 4, sd2);
			printf("filesecurity: %d %d\n", r1, r2);
			r1 = OpenProcessToken(GetCurrentProcess(), 0x28, &tok);
			e = GetLastError();
			printf("openprocesstoken: %d %u\n", r1, e);
			r1 = LookupPrivilegeValueA(0, "SeShutdownPrivilege", luid);
			r2 = AdjustTokenPrivileges(0, 0, 0, 0, 0, 0);
			e = GetLastError();
			printf("privilegios: %d %u %d %u\n", r1, luid[0], r2, e);
			r1 = LookupPrivilegeValueA(0, "SeNoHayPrivilege", luid);
			e = GetLastError();
			printf("privilegio_nohay: %d %u\n", r1, e);
		}
	}
	printf("FIN HLK32\n");
	return 0;
}
