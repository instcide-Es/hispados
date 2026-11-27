; =============================================================================
;  vdso64.asm - Hispa-64, etapa 10 f (docs/64/HISPA-64.md, sección 23): el
;  vDSO, la página de código que el núcleo pone en cada programa (AT_SYSINFO_EHDR
;  en su vector auxiliar) para leer la hora sin llamar al sistema, como el de
;  Linux: musl busca en ella «__vdso_clock_gettime» (versión LINUX_2.6; sin
;  versiones aquí: musl entonces no las mira) y, si está, clock_gettime,
;  gettimeofday y time van por ella.
;
;  Es un ELF de posición libre (ET_DYN) de una página, hecho a mano: la
;  cabecera, dos segmentos (PT_LOAD y PT_DYNAMIC), lo dinámico (DT_HASH,
;  DT_STRTAB, DT_SYMTAB, DT_STRSZ y DT_SYMENT: lo que miran __vdsosym de musl
;  y su cargador dinámico, que la toma como una biblioteca más), la tabla de
;  dispersión, los símbolos, sus nombres y el código. La página siguiente es
;  la de los datos (de solo leer para el programa; la escribe el núcleo,
;  Vdso64):
;     0  sec      nat32: impar mientras el núcleo la cambia
;     8  con_tsc  0: no se puede (el reloj del núcleo no es el TSC): a la llamada
;    16  tsc0     el TSC en el instante ns0
;    24  ns0      CLOCK_MONOTONIC en tsc0
;    32  mult     ns = ns0 + ((tsc - tsc0) * mult) >> 32 (lo mismo que el núcleo)
;    40  desfase  CLOCK_REALTIME - CLOCK_MONOTONIC
;    48  con_tscp [etapa 10 g] 1: el procesador tiene rdtscp y el núcleo ha
;                 puesto en IA32_TSC_AUX el número de cada uno (getcpu)
;  [etapa 10 g] Y __vdso_getcpu (sched_getcpu de musl), como el de Linux: el
;  procesador con rdtscp (en ECX: el número; el nodo, de 12 en adelante, 0),
;  sin llamar al sistema; sin rdtscp, la llamada (getcpu, 309). Del reloj de
;  KVM sin el TSC estable (pvclock sin PVCLOCK_TSC_STABLE_BIT) el vDSO de
;  Linux tampoco lee: llama al sistema (vread_pvclock da U64_MAX), porque cada
;  procesador tiene el suyo y lo que asegura que no vaya hacia atrás de uno a
;  otro es el «cmpxchg» del núcleo con lo último dado, que desde el programa no
;  se puede; con el bit, el núcleo ya va con el TSC (y el vDSO también).
;  Se compila con NASM (nasm -f bin) y sus octetos van en vdso64.cas (la
;  batería mira que sean los mismos).
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
bits 64
org 0
DATOS equ 0x1000

cabecera:
    db 0x7F, "ELF", 2, 1, 1, 0          ; ELF64, «little endian», versión 1, System V
    dq 0
    dw 3                                ; ET_DYN
    dw 62                               ; x86-64
    dd 1
    dq 0                                ; sin entrada
    dq segmentos - $$                   ; e_phoff
    dq 0                                ; sin secciones
    dd 0
    dw 64                               ; e_ehsize
    dw 56                               ; e_phentsize
    dw 2                                ; e_phnum
    dw 64                               ; e_shentsize
    dw 0                                ; e_shnum
    dw 0                                ; e_shstrndx

segmentos:
    ; PT_LOAD: la página entera, de leer y ejecutar
    dd 1, 5
    dq 0, 0, 0
    dq fin_ - $$, fin_ - $$
    dq 0x1000
    ; PT_DYNAMIC
    dd 2, 4
    dq dinamico - $$, dinamico - $$, dinamico - $$
    dq fin_dinamico - dinamico, fin_dinamico - dinamico
    dq 8

dinamico:
    dq 4, dispersion - $$               ; DT_HASH
    dq 5, nombres - $$                  ; DT_STRTAB
    dq 6, simbolos - $$                 ; DT_SYMTAB
    dq 10, fin_nombres - nombres        ; DT_STRSZ
    dq 11, 24                           ; DT_SYMENT
    dq 0, 0                             ; DT_NULL
fin_dinamico:

dispersion:
    ; (un solo cubo: la cadena va del último símbolo al primero)
    dd 1, 5                             ; nbucket, nchain
    dd 4                                ; el cubo 0
    dd 0, 0, 1, 2, 3                    ; las cadenas

    align 8
simbolos:
    times 24 db 0                       ; el 0, vacío
%macro SIMBOLO 2
    dd %1 - nombres
    db 0x12, 0                          ; STB_GLOBAL, STT_FUNC; visible
    dw 1                                ; (una sección cualquiera: no la «indefinida»)
    dq %2 - $$
    dq %2_fin - %2
%endmacro
    SIMBOLO n_cgt, clock_gettime
    SIMBOLO n_gtod, gettimeofday
    SIMBOLO n_time, time_
    SIMBOLO n_cpu, getcpu

nombres:
    db 0
n_cgt:  db "__vdso_clock_gettime", 0
n_gtod: db "__vdso_gettimeofday", 0
n_time: db "__vdso_time", 0
n_cpu:  db "__vdso_getcpu", 0
fin_nombres:

    align 16
; ns (en rax) del reloj edi (0 REALTIME, 1 MONOTONIC, 4 MONOTONIC_RAW,
; 5 REALTIME_COARSE, 6 MONOTONIC_COARSE, 7 BOOTTIME); CF=1 si hay que llamar al
; sistema. Usa rax, rcx, rdx, r8, r9.
lee_ns:
    cmp edi, 7
    ja .no
    mov eax, 0xF3                       ; los relojes 0, 1, 4, 5, 6 y 7
    bt eax, edi
    jnc .no
    lea r8, [rel cabecera + DATOS]
.otra:
    mov r9d, [r8]
    test r9d, 1
    jnz .espera
    cmp qword [r8 + 8], 0
    je .no
    lfence
    rdtsc
    shl rdx, 32
    or rax, rdx
    sub rax, [r8 + 16]
    jae .positivo
    xor eax, eax                        ; (por detrás de tsc0: el mismo instante)
.positivo:
    mul qword [r8 + 32]
    shrd rax, rdx, 32
    add rax, [r8 + 24]
    test edi, edi
    jz .real
    cmp edi, 5
    jne .hecho
.real:
    add rax, [r8 + 40]
.hecho:
    cmp r9d, [r8]
    jne .otra
    clc
    ret
.espera:
    pause
    jmp .otra
.no:
    stc
    ret

; int __vdso_clock_gettime(clockid_t, struct timespec *)
clock_gettime:
    call lee_ns
    jc .llama
    xor edx, edx
    mov ecx, 1000000000
    div rcx
    mov [rsi], rax
    mov [rsi + 8], rdx
    xor eax, eax
    ret
.llama:
    mov eax, 228                        ; clock_gettime
    syscall
    ret
clock_gettime_fin:

; int __vdso_gettimeofday(struct timeval *, struct timezone *)
gettimeofday:
    test rdi, rdi
    jz .zona
    push rdi
    push rsi
    xor edi, edi
    call lee_ns
    pop rsi
    pop rdi
    jc .llama
    xor edx, edx
    mov ecx, 1000000000
    div rcx
    mov [rdi], rax
    mov rax, rdx
    xor edx, edx
    mov ecx, 1000
    div rcx
    mov [rdi + 8], rax
.zona:
    test rsi, rsi
    jz .bien
    mov qword [rsi], 0                  ; (la zona, siempre 0: como Linux)
.bien:
    xor eax, eax
    ret
.llama:
    mov eax, 96                         ; gettimeofday
    syscall
    ret
gettimeofday_fin:

; time_t __vdso_time(time_t *)
time_:
    push rdi
    xor edi, edi
    call lee_ns
    pop rdi
    jc .llama
    xor edx, edx
    mov ecx, 1000000000
    div rcx
    test rdi, rdi
    jz .fin
    mov [rdi], rax
.fin:
    ret
.llama:
    mov eax, 201                        ; time
    syscall
    ret
time__fin:

; [etapa 10 g] long __vdso_getcpu(unsigned *cpu, unsigned *node, void *cache)
getcpu:
    lea r8, [rel cabecera + DATOS]
    cmp qword [r8 + 48], 0
    je .llama
    rdtscp
    mov eax, ecx
    test rdi, rdi
    jz .nodo
    and ecx, 0xFFF
    mov [rdi], ecx
.nodo:
    test rsi, rsi
    jz .bien
    shr eax, 12
    mov [rsi], eax
.bien:
    xor eax, eax
    ret
.llama:
    mov eax, 309                        ; getcpu
    syscall
    ret
getcpu_fin:

fin_:
