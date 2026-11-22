/* HILOS.EXE - prueba de los hilos de verdad de WIN32.BIN (JUEGOS): lo que
 * hace el motor de un juego de 1998-2000 con ellos (el mezclador de sonido,
 * la red y la carga en otro hilo): CreateThread, ExitThread, esperar a uno
 * o a varios, Sleep(0), prioridades, CRITICAL_SECTION, mutex (también uno
 * abandonado), eventos (automáticos, manuales, PulseEvent), semáforos, TLS
 * y Interlocked*. Cada línea, «nombre: valor».
 *   HILOS          todo
 *   HILOS /SALIR   un hilo acaba el proceso (ExitProcess(33)) mientras el
 *                  principal espera sin fin
 *   HILOS /DEJA    el principal acaba con un hilo que no acaba nunca
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "j.h"

static volatile i32 atomico, sin_cerrojo, con_cerrojo, sigue, cuenta_sem, vueltas[2];
static CRITICAL_SECTION cs;
static HANDLE sem, ev_auto, ev_manual, mtx, listo;
static u32 tls;
static u32 ids[4];

static u32 WINAPI suma(void *p)
{
	int k, n = (int)p;
	ids[n] = GetCurrentThreadId();
	TlsSetValue(tls, (void *)(1000 + n));
	for (k = 0; k < 20000; k++) {
		InterlockedIncrement(&atomico);
		EnterCriticalSection(&cs);
		{
			i32 v = con_cerrojo;
			if ((k & 1023) == 0)
				Sleep(0);       /* (que otro lo pille a medias si no hubiera cerrojo) */
			con_cerrojo = v + 1;
		}
		LeaveCriticalSection(&cs);
	}
	if ((u32)TlsGetValue(tls) != 1000u + n)
		return 99;
	return 10 + n;
}
static u32 WINAPI sin_fin(void *p)
{
	(void)p;
	while (sigue)
		;                       /* sin llamar a nada: solo con hilos de verdad acaba */
	return 5;
}
static u32 WINAPI consumidor(void *p)
{
	int k;
	(void)p;
	for (k = 0; k < 10; k++) {
		WaitForSingleObject(sem, INFINITE);
		cuenta_sem++;
	}
	SetEvent(ev_auto);
	return 0;
}
static u32 WINAPI espera_manual(void *p)
{
	(void)p;
	WaitForSingleObject(ev_manual, INFINITE);
	InterlockedIncrement(&atomico);
	return 0;
}
static u32 WINAPI toma_y_se_va(void *p)
{
	(void)p;
	WaitForSingleObject(mtx, INFINITE);
	return 0;                   /* sin ReleaseMutex: abandonado */
}
static u32 WINAPI ping(void *p)
{
	int n = (int)p, k;
	for (k = 0; k < 200; k++) {
		vueltas[n]++;
		Sleep(0);
	}
	return 0;
}
static u32 WINAPI acaba_proceso(void *p)
{
	(void)p;
	Sleep(50);
	printf("hilo: ExitProcess(33)\n");
	ExitProcess(33);
	return 0;
}
/* TLS de la imagen (__declspec(thread) de VC6): la plantilla, el número y
 * el directorio que pone el enlazador (aquí, a mano: tools/elf2pe.py lo
 * apunta en el directorio 9 de la cabecera) */
struct del_hilo { int contador; char nombre[12]; };
static struct del_hilo plantilla __attribute__((section(".tls"), used)) = { 100, "plantilla" };
u32 _tls_index = 99;
static volatile i32 llamadas_tls[4];
static void WINAPI al_tls(void *m, u32 por_que, void *r) { (void)m; (void)r; if (por_que < 4) llamadas_tls[por_que]++; }
static void *funciones_tls[2] = { (void *)al_tls, 0 };
const u32 _tls_used[6] = { (u32)&plantilla, (u32)&plantilla + sizeof plantilla, (u32)&_tls_index, (u32)funciones_tls, 4, 0 };
static struct del_hilo *mio(void)
{
	void **t;
	__asm__ volatile("movl %%fs:0x2C, %0" : "=r"(t));
	return (struct del_hilo *)t[_tls_index];
}
static volatile i32 visto_en_hilo;
static u32 WINAPI usa_tls(void *p)
{
	(void)p;
	visto_en_hilo = mio()->contador;
	mio()->contador = 200;
	return mio()->contador;
}
static u32 WINAPI con_exitthread(void *p)
{
	(void)p;
	ExitThread(77);
	return 1;
}

int main(int argc, char **argv)
{
	HANDLE h[4], t;
	u32 c, r, k, t0;
	if (argc > 1 && !strcmp(argv[1], "/SALIR")) {
		t = CreateThread(0, 0, acaba_proceso, 0, 0, 0);
		WaitForSingleObject(CreateEventA(0, 1, 0, 0), INFINITE);
		printf("MAL: sigue\n");
		return 1;
	}
	if (argc > 1 && !strcmp(argv[1], "/DEJA")) {
		sigue = 1;
		CreateThread(0, 0, sin_fin, 0, 0, 0);
		Sleep(30);
		printf("deja: el principal acaba con un hilo que no acaba\n");
		return 4;
	}
	InitializeCriticalSection(&cs);
	tls = TlsAlloc();
	TlsSetValue(tls, (void *)7);
	/* 1. cuatro hilos que suman a la vez */
	for (k = 0; k < 4; k++)
		h[k] = CreateThread(0, 0, suma, (void *)k, 0, 0);
	r = WaitForMultipleObjects(4, h, 1, 20000);
	printf("esperar_a_todos: %u\n", r);
	printf("interlocked: %d\n", atomico);
	printf("critical_section: %d\n", con_cerrojo);
	for (k = 0, c = 0; k < 4; k++) {
		u32 e;
		GetExitCodeThread(h[k], &e);
		c += e;
		CloseHandle(h[k]);
	}
	printf("codigos: %u\n", c);
	printf("ids_distintos: %s\n", ids[0] != ids[1] && ids[1] != ids[2] && ids[0] != GetCurrentThreadId() && ids[3] ? "si" : "no");
	printf("tls_principal: %u\n", (u32)TlsGetValue(tls));
	/* 2. un hilo que no llama a nada: corre a la vez que el principal */
	sigue = 1;
	t = CreateThread(0, 0, sin_fin, 0, 0, 0);
	r = WaitForSingleObject(t, 100);
	printf("sin_fin_sigue: %s\n", r == 258 ? "si" : "no");
	sigue = 0;
	r = WaitForSingleObject(t, 5000);
	GetExitCodeThread(t, &c);
	printf("sin_fin_acaba: %u %u\n", r, c);
	CloseHandle(t);
	/* 3. semáforo y evento automático */
	sem = CreateSemaphoreA(0, 0, 10, 0);
	ev_auto = CreateEventA(0, 0, 0, 0);
	t = CreateThread(0, 0, consumidor, 0, 0, 0);
	for (k = 0; k < 10; k++) {
		ReleaseSemaphore(sem, 1, 0);
		if (k == 4)
			Sleep(20);
	}
	r = WaitForSingleObject(ev_auto, 5000);
	printf("semaforo: %u %d\n", r, cuenta_sem);
	printf("evento_auto_se_apaga: %u\n", WaitForSingleObject(ev_auto, 0));
	r = ReleaseSemaphore(sem, 11, 0);
	printf("semaforo_demasiado: %u %u\n", r, GetLastError());
	CloseHandle(t);
	/* 4. evento manual: deja pasar a todos */
	ev_manual = CreateEventA(0, 1, 0, 0);
	atomico = 0;
	for (k = 0; k < 3; k++)
		h[k] = CreateThread(0, 0, espera_manual, 0, 0, 0);
	Sleep(30);
	printf("manual_antes: %d\n", atomico);
	SetEvent(ev_manual);
	r = WaitForMultipleObjects(3, h, 1, 5000);
	printf("manual_despues: %u %d\n", r, atomico);
	printf("manual_sigue_puesto: %u\n", WaitForSingleObject(ev_manual, 0));
	/* 5. esperar con tiempo */
	t0 = GetTickCount();
	r = WaitForSingleObject(CreateEventA(0, 0, 0, 0), 60);
	c = GetTickCount() - t0;
	printf("espera_60ms: %u %s\n", r, c >= 50 && c < 400 ? "bien" : "mal");
	/* 6. mutex: con dueño, recursivo, de otro, abandonado; con nombre */
	mtx = CreateMutexA(0, 1, "MutexDePrueba");
	printf("mutex_nuevo: %u\n", GetLastError());
	printf("mutex_recursivo: %u\n", WaitForSingleObject(mtx, 0));
	r = ReleaseMutex(mtx);
	c = ReleaseMutex(mtx);
	k = ReleaseMutex(mtx);
	printf("mutex_suelta: %u %u %u %u\n", r, c, k, GetLastError());
	{
		HANDLE m2 = CreateMutexA(0, 0, "MutexDePrueba");
		printf("mutex_mismo_nombre: %u %s\n", GetLastError(), m2 == mtx ? "el mismo" : "otro");
		printf("mutex_abre: %s\n", OpenMutexA(0x1F0001, 0, "MutexDePrueba") == mtx ? "si" : "no");
	}
	t = CreateThread(0, 0, toma_y_se_va, 0, 0, 0);
	WaitForSingleObject(t, 5000);
	Sleep(30);
	printf("mutex_abandonado: %x\n", WaitForSingleObject(mtx, 1000));
	ReleaseMutex(mtx);
	/* 7. Sleep(0) cede: dos hilos que se turnan */
	h[0] = CreateThread(0, 0, ping, (void *)0, 0, 0);
	h[1] = CreateThread(0, 0, ping, (void *)1, 0, 0);
	r = WaitForMultipleObjects(2, h, 1, 10000);
	printf("sleep0: %u %d %d\n", r, vueltas[0], vueltas[1]);
	/* 8. prioridades, dormido y ExitThread */
	t = CreateThread(0, 0, con_exitthread, 0, 4, &c);
	r = SetThreadPriority(t, 15);
	printf("prioridad: %u %d\n", r, GetThreadPriority(t));
	printf("dormido: %u\n", WaitForSingleObject(t, 30));
	ResumeThread(t);
	r = WaitForSingleObject(t, 5000);
	GetExitCodeThread(t, &c);
	printf("exitthread: %u %u\n", r, c);
	/* 9. TLS de la imagen: cada hilo, su copia de la plantilla */
	printf("tls_imagen: %u %d %s\n", _tls_index, mio()->contador, mio()->nombre);
	mio()->contador = 5;
	t = CreateThread(0, 0, usa_tls, 0, 0, 0);
	WaitForSingleObject(t, 5000);
	GetExitCodeThread(t, &c);
	Sleep(20);
	printf("tls_imagen_hilo: %d %u %d\n", visto_en_hilo, c, mio()->contador);
	printf("tls_funciones: %d %d %d\n", llamadas_tls[1], llamadas_tls[2] > 0, llamadas_tls[3] > 0);
	printf("FIN HILOS\n");
	return 0;
}
