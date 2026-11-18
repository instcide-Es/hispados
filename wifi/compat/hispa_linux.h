/*
 * hispa_linux.h - Lo que los fuentes de brcmsmac y bcma (wifi/linux/) esperan
 * del núcleo de Linux, hecho aquí para Hispa-486: tipos, listas, memoria,
 * paquetes (sk_buff), esperas, PCI, DMA y la parte de mac80211/cfg80211 que
 * usa el controlador. Todas las cabeceras <linux/...> y <net/...> de
 * wifi/compat/include/ acaban en este archivo.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#ifndef HISPA_LINUX_H
#define HISPA_LINUX_H

#include <stdarg.h>
#include <stddef.h>

/* ---------------------------------------------------------------- tipos */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef short s16;
typedef int s32;
typedef long long s64;
typedef u8 __u8;
typedef u16 __u16;
typedef u32 __u32;
typedef u64 __u64;
typedef s8 __s8;
typedef s16 __s16;
typedef s32 __s32;
typedef u8 uint8_t;
typedef u16 uint16_t;
typedef u32 uint32_t;
typedef u64 uint64_t;
typedef s8 int8_t;
typedef s16 int16_t;
typedef s32 int32_t;
typedef s64 int64_t;
typedef u16 __le16;
typedef u32 __le32;
typedef u64 __le64;
typedef u16 __be16;
typedef u32 __be32;
typedef u16 __sum16;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef _Bool bool;
#define true 1
#define false 0
typedef u32 dma_addr_t;
typedef u32 phys_addr_t;
typedef u32 resource_size_t;
typedef unsigned int gfp_t;
typedef long ssize_t;
typedef long long loff_t;
typedef unsigned long uintptr_t;
typedef int irqreturn_t;
typedef unsigned int fmode_t;
typedef unsigned short umode_t;
typedef struct { int counter; } atomic_t;
typedef struct { int x; } spinlock_t;
typedef s64 ktime_t;
struct mutex { int x; };
struct completion { int x; };
typedef struct { int x; } wait_queue_head_t;
struct dentry;
struct module;
struct seq_file;
struct file_operations;
struct net_device;
struct device_node;
struct platform_device;
struct lock_class_key { int x; };

#define __iomem
#define __user
#define __force
#define __bitwise
#define __must_check
#define __init
#define __exit
#define __devinit
#define __initdata
#define __read_mostly
#define __always_inline inline __attribute__((always_inline))
#define noinline __attribute__((noinline))
#define __packed __attribute__((packed))
#define __aligned(x) __attribute__((aligned(x)))
#define __printf(a, b) __attribute__((format(printf, a, b)))
#define __maybe_unused __attribute__((unused))
#define __always_unused __attribute__((unused))
#define __attribute_const__ __attribute__((const))
#define __acquires(x)
#define __releases(x)
#define __cold
#define fallthrough __attribute__((fallthrough))
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define barrier() __asm__ __volatile__("" : : : "memory")
#define cpu_relax() barrier()
#define KBUILD_MODNAME "brcmsmac"
#define THIS_MODULE ((struct module *)0)
#define NULL ((void *)0)

#define BITS_PER_LONG 32
#define BITS_PER_BYTE 8
#define BIT(n) (1UL << (n))
#define BIT_ULL(n) (1ULL << (n))
#define GENMASK(h, l) (((~0UL) << (l)) & (~0UL >> (31 - (h))))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define sizeof_field(t, f) (sizeof(((t *)0)->f))
#define offsetofend(t, f) (offsetof(t, f) + sizeof_field(t, f))
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))
#define min(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a < _b ? _a : _b; })
#define max(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })
#define min_t(t, a, b) ({ t _a = (a); t _b = (b); _a < _b ? _a : _b; })
#define max_t(t, a, b) ({ t _a = (a); t _b = (b); _a > _b ? _a : _b; })
#define clamp(v, lo, hi) min(max(v, lo), hi)
#define clamp_t(t, v, lo, hi) min_t(t, max_t(t, v, lo), hi)
#define clamp_val(v, lo, hi) clamp_t(__typeof__(v), v, lo, hi)
#define swap(a, b) do { __typeof__(a) _t = (a); (a) = (b); (b) = _t; } while (0)
#define abs(x) ({ __typeof__(x) _x = (x); _x < 0 ? -_x : _x; })
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define rounddown(x, y) ((x) - ((x) % (y)))
#define round_up(x, y) ALIGN(x, y)
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define DIV_ROUND_CLOSEST(x, d) (((x) + ((d) / 2)) / (d))
#define IS_ENABLED(x) 0
#define BUILD_BUG_ON(c) ((void)sizeof(char[1 - 2 * !!(c)]))
#define upper_32_bits(n) ((u32)(((u64)(n)) >> 32))
#define lower_32_bits(n) ((u32)(n))
#define PTR_ALIGN(p, a) ((__typeof__(p))ALIGN((unsigned long)(p), (a)))
#define DECLARE_BITMAP(n, bits) unsigned long n[((bits) + 31) / 32]
#define U8_MAX 0xff
#define U16_MAX 0xffff
#define U32_MAX 0xffffffffU
#define INT_MAX 0x7fffffff
#define INT_MIN (-INT_MAX - 1)
#define UINT_MAX 0xffffffffU
#define S8_MAX 127
#define S8_MIN (-128)
#define S16_MAX 32767
#define S16_MIN (-32768)
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/* errores */
#define EPERM 1
#define ENOENT 2
#define EIO 5
#define ENXIO 6
#define E2BIG 7
#define EAGAIN 11
#define ENOMEM 12
#define EFAULT 14
#define EBUSY 16
#define EEXIST 17
#define ENODEV 19
#define EINVAL 22
#define ENOSPC 28
#define ERANGE 34
#define ENOSYS 38
#define ENODATA 61
#define EIDRM 43
#define ESPIPE 29
#define ENOMSG 42
#define EBADF 9
#define ENOSR 63
#define ERFKILL 132
#define ENOLINK 67
#define ENOTCONN 107
#define EBADE 52
#define EILSEQ 84
#define EOPNOTSUPP 95
#define ENOTSUPP 524
#define ETIMEDOUT 110
#define EALREADY 114
#define EINPROGRESS 115
#define ENOMEDIUM 123
#define EPROTO 71
#define EBADMSG 74
#define ENOBUFS 105
#define EADDRNOTAVAIL 99
#define MAX_ERRNO 4095
#define IS_ERR_VALUE(x) ((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)
static inline void *ERR_PTR(long e) { return (void *)e; }
static inline long PTR_ERR(const void *p) { return (long)p; }
static inline bool IS_ERR(const void *p) { return IS_ERR_VALUE(p); }
static inline bool IS_ERR_OR_NULL(const void *p) { return !p || IS_ERR_VALUE(p); }

/* orden de los octetos: x86, de menor a mayor */
#define cpu_to_le16(x) ((__le16)(x))
#define cpu_to_le32(x) ((__le32)(x))
#define cpu_to_le64(x) ((__le64)(x))
#define le16_to_cpu(x) ((u16)(x))
#define le32_to_cpu(x) ((u32)(x))
#define le64_to_cpu(x) ((u64)(x))
#define le16_to_cpup(p) (*(const u16 *)(p))
#define le32_to_cpup(p) (*(const u32 *)(p))
#define swab16(x) ((u16)__builtin_bswap16(x))
#define swab32(x) ((u32)__builtin_bswap32(x))
#define cpu_to_be16(x) swab16(x)
#define cpu_to_be32(x) swab32(x)
#define be16_to_cpu(x) swab16(x)
#define be32_to_cpu(x) swab32(x)
#define htons(x) swab16(x)
#define ntohs(x) swab16(x)
#define htonl(x) swab32(x)
#define ntohl(x) swab32(x)
static inline void le16_add_cpu(__le16 *v, u16 n) { *v += n; }
static inline u16 get_unaligned_le16(const void *p) { const u8 *b = p; return b[0] | (b[1] << 8); }
static inline u32 get_unaligned_le32(const void *p) { const u8 *b = p; return b[0] | (b[1] << 8) | (b[2] << 16) | ((u32)b[3] << 24); }
static inline u16 get_unaligned_be16(const void *p) { const u8 *b = p; return b[1] | (b[0] << 8); }
static inline u32 get_unaligned_be32(const void *p) { const u8 *b = p; return b[3] | (b[2] << 8) | (b[1] << 16) | ((u32)b[0] << 24); }
static inline void put_unaligned_le16(u16 v, void *p) { u8 *b = p; b[0] = v; b[1] = v >> 8; }
static inline void put_unaligned_le32(u32 v, void *p) { u8 *b = p; b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24; }
static inline void put_unaligned_be16(u16 v, void *p) { u8 *b = p; b[1] = v; b[0] = v >> 8; }
static inline void put_unaligned_be32(u32 v, void *p) { u8 *b = p; b[3] = v; b[2] = v >> 8; b[1] = v >> 16; b[0] = v >> 24; }
#define get_unaligned(p) (*(p))
#define put_unaligned(v, p) (*(p) = (v))

/* ----------------------------------------------------- textos y memoria */
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *d, const char *s);
char *strncpy(char *d, const char *s, size_t n);
size_t strlcpy(char *d, const char *s, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
int vsnprintf(char *b, size_t n, const char *f, va_list ap);
int snprintf(char *b, size_t n, const char *f, ...) __printf(3, 4);
int scnprintf(char *b, size_t n, const char *f, ...) __printf(3, 4);
int sprintf(char *b, const char *f, ...) __printf(2, 3);
unsigned long simple_strtoul(const char *s, char **e, unsigned int base);
static inline int isalpha(int c) { return ((c | 32) >= 'a' && (c | 32) <= 'z'); }
static inline int isdigit(int c) { return c >= '0' && c <= '9'; }
static inline int isprint(int c) { return c >= 32 && c < 127; }
static inline int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

/* registro (wifi/nucleo/base.c): nivel 0 error ... 3 detalle */
void hl_log(int nivel, const char *f, ...) __printf(2, 3);
void hl_vlog(int nivel, const char *f, va_list ap);
int printk(const char *f, ...) __printf(1, 2);
#define KERN_ERR ""
#define KERN_WARNING ""
#define KERN_INFO ""
#define KERN_DEBUG ""
#define KERN_CONT ""
#define KERN_NOTICE ""
#define pr_fmt(f) f
#define pr_err(f, a...) hl_log(0, f, ##a)
#define pr_warn(f, a...) hl_log(1, f, ##a)
#define pr_warning pr_warn
#define pr_notice(f, a...) hl_log(2, f, ##a)
#define pr_info(f, a...) hl_log(2, f, ##a)
#define pr_debug(f, a...) hl_log(3, f, ##a)
#define pr_cont(f, a...) hl_log(2, f, ##a)
#define pr_err_ratelimited pr_err
#define dev_err(d, f, a...) hl_log(0, f, ##a)
#define dev_warn(d, f, a...) hl_log(1, f, ##a)
#define dev_info(d, f, a...) hl_log(2, f, ##a)
#define dev_dbg(d, f, a...) hl_log(3, f, ##a)
#define dev_name(d) "wifi"
#define net_ratelimit() 1
#define printk_ratelimit() 1
#define print_hex_dump_bytes(a...) do { } while (0)
void hl_aviso(const char *archivo, int linea, const char *que);
void hl_linea(void);
#define WARN_ON(c) ({ int _c = !!(c); if (_c) hl_aviso(__FILE__, __LINE__, #c); _c; })
#define WARN_ON_ONCE(c) WARN_ON(c)
#define WARN(c, f, a...) ({ int _c = !!(c); if (_c) { hl_aviso(__FILE__, __LINE__, #c); hl_log(1, f, ##a); hl_linea(); } _c; })
#define WARN_ONCE WARN
#define BUG_ON(c) do { if (c) hl_aviso(__FILE__, __LINE__, "BUG: " #c); } while (0)
#define BUG() hl_aviso(__FILE__, __LINE__, "BUG")
#define might_sleep() do { } while (0)
#define dump_stack() do { } while (0)

#define GFP_KERNEL 0
#define GFP_ATOMIC 1
#define GFP_DMA 2
#define __GFP_ZERO 0x100
#define __GFP_NOWARN 0
void *hl_malloc(size_t n);
void *hl_zalloc(size_t n);
void hl_free(const void *p);
static inline void *kmalloc(size_t n, gfp_t g) { return (g & __GFP_ZERO) ? hl_zalloc(n) : hl_malloc(n); }
static inline void *kzalloc(size_t n, gfp_t g) { return hl_zalloc(n); }
static inline void *kcalloc(size_t n, size_t m, gfp_t g) { return hl_zalloc(n * m); }
static inline void *kmalloc_array(size_t n, size_t m, gfp_t g) { return hl_malloc(n * m); }
static inline void kfree(const void *p) { hl_free(p); }
static inline void kvfree(const void *p) { hl_free(p); }
static inline void *vmalloc(size_t n) { return hl_malloc(n); }
static inline void *vzalloc(size_t n) { return hl_zalloc(n); }
static inline void vfree(const void *p) { hl_free(p); }
void *kmemdup(const void *s, size_t n, gfp_t g);
#define devm_kzalloc(d, n, g) hl_zalloc(n)
#define struct_size(p, m, n) (sizeof(*(p)) + sizeof(*(p)->m) * (n))

/* ---------------------------------------------------------------- listas */
struct list_head { struct list_head *next, *prev; };
#define LIST_HEAD_INIT(n) { &(n), &(n) }
#define LIST_HEAD(n) struct list_head n = LIST_HEAD_INIT(n)
static inline void INIT_LIST_HEAD(struct list_head *l) { l->next = l; l->prev = l; }
static inline void __list_add(struct list_head *n, struct list_head *p, struct list_head *x)
{ x->prev = n; n->next = x; n->prev = p; p->next = n; }
static inline void list_add(struct list_head *n, struct list_head *h) { __list_add(n, h, h->next); }
static inline void list_add_tail(struct list_head *n, struct list_head *h) { __list_add(n, h->prev, h); }
static inline void list_del(struct list_head *e) { e->next->prev = e->prev; e->prev->next = e->next; e->next = e->prev = NULL; }
static inline void list_del_init(struct list_head *e) { e->next->prev = e->prev; e->prev->next = e->next; INIT_LIST_HEAD(e); }
static inline int list_empty(const struct list_head *h) { return h->next == h; }
#define list_entry(p, t, m) container_of(p, t, m)
#define list_first_entry(p, t, m) list_entry((p)->next, t, m)
#define list_for_each_entry(pos, head, m) \
	for (pos = list_entry((head)->next, __typeof__(*pos), m); &pos->m != (head); \
	     pos = list_entry(pos->m.next, __typeof__(*pos), m))
#define list_for_each_entry_reverse(pos, head, m) \
	for (pos = list_entry((head)->prev, __typeof__(*pos), m); &pos->m != (head); \
	     pos = list_entry(pos->m.prev, __typeof__(*pos), m))
#define list_for_each_entry_safe(pos, n, head, m) \
	for (pos = list_entry((head)->next, __typeof__(*pos), m), \
	     n = list_entry(pos->m.next, __typeof__(*pos), m); &pos->m != (head); \
	     pos = n, n = list_entry(n->m.next, __typeof__(*n), m))

/* -------------------------------------------------------- bits y cuentas */
static inline int test_bit(int n, const unsigned long *a) { return (a[n / 32] >> (n & 31)) & 1; }
static inline void set_bit(int n, unsigned long *a) { a[n / 32] |= 1UL << (n & 31); }
static inline void clear_bit(int n, unsigned long *a) { a[n / 32] &= ~(1UL << (n & 31)); }
#define __set_bit set_bit
#define __clear_bit clear_bit
static inline int test_and_set_bit(int n, unsigned long *a) { int v = test_bit(n, a); set_bit(n, a); return v; }
static inline int test_and_clear_bit(int n, unsigned long *a) { int v = test_bit(n, a); clear_bit(n, a); return v; }
static inline int ffs(int x) { return __builtin_ffs(x); }
static inline int fls(unsigned int x) { return x ? 32 - __builtin_clz(x) : 0; }
static inline unsigned long __ffs(unsigned long x) { return __builtin_ctzl(x); }
static inline unsigned int hweight32(unsigned int x) { return __builtin_popcount(x); }
#define hweight8(x) hweight32((x) & 0xff)
#define hweight16(x) hweight32((x) & 0xffff)
#define ilog2(x) (fls(x) - 1)
static inline bool is_power_of_2(unsigned long n) { return n && !(n & (n - 1)); }
static inline unsigned long roundup_pow_of_two(unsigned long n) { return n <= 1 ? 1 : 1UL << fls(n - 1); }
static inline int atomic_read(const atomic_t *a) { return a->counter; }
static inline void atomic_set(atomic_t *a, int v) { a->counter = v; }
static inline void atomic_inc(atomic_t *a) { a->counter++; }
static inline void atomic_dec(atomic_t *a) { a->counter--; }
static inline int atomic_dec_and_test(atomic_t *a) { return --a->counter == 0; }
static inline int atomic_inc_return(atomic_t *a) { return ++a->counter; }
static inline int atomic_add_return(int n, atomic_t *a) { return a->counter += n; }
#define ATOMIC_INIT(v) { (v) }
u64 hl_div64(u64 n, u32 d, u32 *resto);
#define do_div(n, base) ({ u32 _r; (n) = hl_div64((n), (base), &_r); _r; })
static inline u64 div_u64(u64 n, u32 d) { return hl_div64(n, d, NULL); }
static inline u64 div64_u64(u64 n, u64 d) { return hl_div64(n, (u32)d, NULL); }
u32 crc32_le(u32 crc, const unsigned char *p, size_t n);
u32 prandom_u32(void);
u32 hl_isqrt(u32 x);
#define int_sqrt(x) hl_isqrt(x)

/* ------------------------------------------- cerrojos (un solo hilo: nada) */
#define spin_lock_init(l) do { } while (0)
#define spin_lock(l) do { } while (0)
#define spin_unlock(l) do { } while (0)
#define spin_lock_bh(l) do { } while (0)
#define spin_unlock_bh(l) do { } while (0)
#define spin_lock_irq(l) do { } while (0)
#define spin_unlock_irq(l) do { } while (0)
#define spin_lock_irqsave(l, f) do { (void)(f); } while (0)
#define spin_unlock_irqrestore(l, f) do { (void)(f); } while (0)
#define DEFINE_SPINLOCK(n) spinlock_t n
#define DEFINE_MUTEX(n) struct mutex n
#define mutex_init(m) do { } while (0)
#define mutex_lock(m) do { } while (0)
#define mutex_unlock(m) do { } while (0)
#define local_irq_save(f) do { (void)(f); } while (0)
#define local_irq_restore(f) do { (void)(f); } while (0)
#define local_bh_disable() do { } while (0)
#define local_bh_enable() do { } while (0)
#define rcu_read_lock() do { } while (0)
#define rcu_read_unlock() do { } while (0)
#define synchronize_irq(i) do { } while (0)
#define in_interrupt() 0
#define in_atomic() 0
#define smp_mb() do { } while (0)
#define ASSERT_RTNL() do { } while (0)
#define lockdep_assert_held(l) do { } while (0)

/* ---------------------------------------------------------------- tiempo */
#define HZ 1000
unsigned long hl_jiffies(void);
#define jiffies hl_jiffies()
void hl_udelay(unsigned long us);
#define udelay(n) hl_udelay(n)
#define mdelay(n) hl_udelay((n) * 1000UL)
#define msleep(n) hl_udelay((n) * 1000UL)
#define usleep_range(a, b) hl_udelay(a)
#define ndelay(n) hl_udelay(1)
#define msecs_to_jiffies(m) ((unsigned long)(m))
#define jiffies_to_msecs(j) ((unsigned int)(j))
#define usecs_to_jiffies(u) ((unsigned long)(((u) + 999) / 1000))
#define time_after(a, b) ((long)((b) - (a)) < 0)
#define time_before(a, b) time_after(b, a)
#define time_after_eq(a, b) ((long)((a) - (b)) >= 0)
#define time_before_eq(a, b) time_after_eq(b, a)
#define NSEC_PER_USEC 1000L
#define USEC_PER_MSEC 1000L
#define NSEC_PER_MSEC 1000000L
static inline ktime_t ktime_get(void) { return (s64)hl_jiffies() * 1000000; }
static inline u64 ktime_get_ns(void) { return (u64)hl_jiffies() * 1000000; }

/* trabajos aplazados y «tasklets»: los corre hl_trabajos() en cada pasada */
struct work_struct;
typedef void (*work_func_t)(struct work_struct *w);
struct work_struct { work_func_t func; struct work_struct *sig; u8 pend; };
struct delayed_work { struct work_struct work; unsigned long cuando; u8 armado; struct delayed_work *sig; };
#define to_delayed_work(w) container_of(w, struct delayed_work, work)
void hl_init_dwork(struct delayed_work *d, work_func_t f);
int hl_queue_dwork(struct delayed_work *d, unsigned long retraso);
int hl_cancel_dwork(struct delayed_work *d);
#define INIT_DELAYED_WORK(d, f) hl_init_dwork(d, f)
#define INIT_WORK(w, f) do { (w)->func = (f); (w)->pend = 0; } while (0)
#define cancel_delayed_work(d) hl_cancel_dwork(d)
#define cancel_delayed_work_sync(d) hl_cancel_dwork(d)
#define schedule_work(w) ((w)->func(w), 1)
#define cancel_work_sync(w) 0
#define flush_workqueue(q) do { } while (0)
struct tasklet_struct { void (*func)(struct tasklet_struct *); u8 pend; u8 puesto; struct tasklet_struct *sig; };
void tasklet_setup(struct tasklet_struct *t, void (*f)(struct tasklet_struct *));
#define from_tasklet(var, t, campo) container_of(t, __typeof__(*var), campo)
void tasklet_schedule(struct tasklet_struct *t);
void tasklet_kill(struct tasklet_struct *t);
struct timer_list { int x; };
#define schedule() do { } while (0)
#define SPINWAIT_DUMMY 0

/* ------------------------------------------------------- entrada y salida */
/* El núcleo corre con segmentos de base «hl_base» (la dirección física de su
 * bloque): una dirección física F se ve en el puntero F - hl_base. */
extern u32 hl_base;
#define hl_virt(fis) ((void *)((u32)(fis) - hl_base))
#define hl_fis(virt) ((u32)(virt) + hl_base)
#ifdef HL_SIMULA_ES
u32 hl_sim_leer(const void *dir, int tam);
void hl_sim_escribir(const void *dir, int tam, u32 v);
#define readb(a) ((u8)hl_sim_leer(a, 1))
#define readw(a) ((u16)hl_sim_leer(a, 2))
#define readl(a) hl_sim_leer(a, 4)
#define writeb(v, a) hl_sim_escribir(a, 1, v)
#define writew(v, a) hl_sim_escribir(a, 2, v)
#define writel(v, a) hl_sim_escribir(a, 4, v)
#else
#define readb(a) (*(volatile u8 *)(a))
#define readw(a) (*(volatile u16 *)(a))
#define readl(a) (*(volatile u32 *)(a))
#define writeb(v, a) (*(volatile u8 *)(a) = (v))
#define writew(v, a) (*(volatile u16 *)(a) = (v))
#define writel(v, a) (*(volatile u32 *)(a) = (v))
#endif
#define ioread8(a) readb(a)
#define ioread16(a) readw(a)
#define ioread32(a) readl(a)
#define iowrite8(v, a) writeb(v, a)
#define iowrite16(v, a) writew(v, a)
#define iowrite32(v, a) writel(v, a)
#define mmiowb() do { } while (0)
#define iounmap(a) do { } while (0)

/* ------------------------------------------------------------ dispositivos */
struct bus_type { const char *name; };
struct device { void *driver_data; void *platform_data; struct device *parent; const char *nombre; void *of_node;
	void (*release)(struct device *); struct bus_type *bus; u64 *dma_mask; u64 coherent_dma_mask; struct device_driver *driver; };
struct device_driver { const char *name; struct bus_type *bus; void *owner; void *pm; };
#define dev_set_name(d, f, a...) 0
static inline void *dev_get_drvdata(const struct device *d) { return d->driver_data; }
static inline void dev_set_drvdata(struct device *d, void *p) { d->driver_data = p; }
#define device_set_wakeup_enable(d, v) do { } while (0)
#define device_initialize(d) do { } while (0)
#define get_device(d) (d)
#define put_device(d) do { } while (0)

struct pci_bus { u8 number; };
struct pci_dev {
	struct device dev;
	struct pci_bus *bus;
	struct pci_bus bus0;
	u8 devfn;
	u16 vendor, device, subsystem_vendor, subsystem_device;
	u8 revision;
	u8 irq;
	u32 bar0;		/* dirección física del BAR 0 */
	u32 bar0_tam;
	u8 pcie_cap;
	void *drvdata;
	struct pci_driver *driver;
};
struct pci_device_id { u32 vendor, device, subvendor, subdevice, class, class_mask; unsigned long driver_data; };
struct pci_driver { const char *name; const struct pci_device_id *id_table;
	int (*probe)(struct pci_dev *, const struct pci_device_id *);
	void (*remove)(struct pci_dev *); void *driver_pm; struct { void *pm; } driver; };
#define PCI_ANY_ID (~0)
#define PCI_DEVICE(v, d) .vendor = (v), .device = (d), .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#define PCI_DEVICE_SUB(v, d, sv, sd) .vendor = (v), .device = (d), .subvendor = (sv), .subdevice = (sd)
#define PCI_VENDOR_ID_BROADCOM 0x14e4
#define PCI_VENDOR_ID_DELL 0x1028
#define PCI_VENDOR_ID_HP 0x103c
#define PCI_VENDOR_ID_APPLE 0x106b
#define PCI_VENDOR_ID_FOXCONN 0x105b
#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02
#define PCI_COMMAND 0x04
#define PCI_COMMAND_MEMORY 0x2
#define PCI_COMMAND_MASTER 0x4
#define PCI_STATUS 0x06
#define PCI_REVISION_ID 0x08
#define PCI_LATENCY_TIMER 0x0d
#define PCI_BASE_ADDRESS_0 0x10
#define PCI_INTERRUPT_LINE 0x3c
#define PCI_INTERRUPT_PIN 0x3d
#define PCI_CAP_ID_EXP 0x10
#define PCI_CAP_ID_PM 0x01
#define PCI_PM_CTRL 4
#define PCI_PM_CTRL_STATE_MASK 0x0003
#define PCI_EXP_LNKCTL 16
#define PCI_EXP_LNKCTL_ASPMC 0x0003
#define PCI_EXP_LNKCTL_CLKREQ_EN 0x0100
#define PCI_EXP_DEVCTL 8
#define PCI_D0 0
#define PCI_D3hot 3
#define PCI_FUNC(devfn) ((devfn) & 7)
#define PCI_SLOT(devfn) (((devfn) >> 3) & 0x1f)
#define PCIBIOS_SUCCESSFUL 0
int pci_read_config_byte(struct pci_dev *d, int w, u8 *v);
int pci_read_config_word(struct pci_dev *d, int w, u16 *v);
int pci_read_config_dword(struct pci_dev *d, int w, u32 *v);
int pci_write_config_byte(struct pci_dev *d, int w, u8 v);
int pci_write_config_word(struct pci_dev *d, int w, u16 v);
int pci_write_config_dword(struct pci_dev *d, int w, u32 v);
int pci_find_capability(struct pci_dev *d, int cap);
int pci_enable_device(struct pci_dev *d);
void pci_set_master(struct pci_dev *d);
void __iomem *pci_iomap(struct pci_dev *d, int bar, unsigned long max);
#define pci_disable_device(d) do { } while (0)
#define pci_request_regions(d, n) 0
#define pci_release_regions(d) do { } while (0)
#define pci_iounmap(d, a) do { } while (0)
#define pci_name(d) "wifi"
#define pci_set_drvdata(d, p) ((d)->drvdata = (p))
#define pci_get_drvdata(d) ((d)->drvdata)
#define pci_is_pcie(d) ((d)->pcie_cap != 0)
#define pci_pcie_cap(d) ((d)->pcie_cap)
#define pci_set_power_state(d, s) 0
#define pci_save_state(d) 0
#define pci_restore_state(d) do { } while (0)
#define pci_enable_wake(d, s, e) 0
#define pci_choose_state(d, s) 0
#define pci_unregister_driver(d) do { } while (0)
#define to_pci_dev(d) container_of(d, struct pci_dev, dev)
#define dev_is_pci(d) 1
#define pci_dev_put(d) do { } while (0)
#define device_property_read_bool(d, n) 0

#define IRQF_SHARED 0x80
#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define IRQ_RETVAL(x) ((x) ? IRQ_HANDLED : IRQ_NONE)
typedef irqreturn_t (*irq_handler_t)(int, void *);
int request_irq(unsigned int irq, irq_handler_t h, unsigned long f, const char *n, void *dev);
void free_irq(unsigned int irq, void *dev);
#define disable_irq(i) do { } while (0)
#define enable_irq(i) do { } while (0)

struct firmware { size_t size; const u8 *data; };
int request_firmware(const struct firmware **fw, const char *nombre, struct device *d);
void release_firmware(const struct firmware *fw);

/* DMA: sin paginación; la dirección para la tarjeta es la física */
#define DMA_TO_DEVICE 1
#define DMA_FROM_DEVICE 2
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL << (n)) - 1))
void *dma_alloc_coherent(struct device *d, size_t n, dma_addr_t *pa, gfp_t g);
void dma_free_coherent(struct device *d, size_t n, void *v, dma_addr_t pa);
static inline dma_addr_t dma_map_single(struct device *d, void *p, size_t n, int dir) { return hl_fis(p); }
#define dma_unmap_single(d, a, n, dir) do { } while (0)
#define dma_mapping_error(d, a) 0
#define dma_set_mask(d, m) 0
#define dma_set_mask_and_coherent(d, m) 0
#define dma_set_coherent_mask(d, m) 0
#define pci_set_dma_mask(d, m) 0
#define pci_set_consistent_dma_mask(d, m) 0

/* módulos y símbolos: nada */
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_LICENSE(x)
#define MODULE_SUPPORTED_DEVICE(x)
#define MODULE_FIRMWARE(x)
#define MODULE_DEVICE_TABLE(a, b)
#define MODULE_ALIAS(x)
#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)
#define module_init(f) int hl_modinit_##f(void) { return f(); }
#define module_exit(f) void hl_modexit_##f(void) { f(); }
#define module_param_named(a...)
#define MODULE_PARM_DESC(a...)
#define fs_initcall(f)
#define subsys_initcall(f)

/* ------------------------------------------------- paquetes (sk_buff) */
struct sk_buff {
	struct sk_buff *next, *prev;
	u8 *head, *data, *tail, *end;
	unsigned int len;
	u32 priority;
	u16 queue_mapping;
	u16 protocol;
	char cb[48] __aligned(8);
	void *dev;
};
struct sk_buff_head { struct sk_buff *next, *prev; u32 qlen; spinlock_t lock; };
struct sk_buff *dev_alloc_skb(unsigned int tam);
#define alloc_skb(n, g) dev_alloc_skb(n)
#define __dev_alloc_skb(n, g) dev_alloc_skb(n)
#define netdev_alloc_skb(d, n) dev_alloc_skb(n)
void kfree_skb(struct sk_buff *s);
#define dev_kfree_skb(s) kfree_skb(s)
#define dev_kfree_skb_any(s) kfree_skb(s)
#define dev_kfree_skb_irq(s) kfree_skb(s)
static inline void *skb_put(struct sk_buff *s, unsigned int n) { u8 *t = s->tail; s->tail += n; s->len += n; return t; }
static inline void *skb_put_zero(struct sk_buff *s, unsigned int n) { void *p = skb_put(s, n); memset(p, 0, n); return p; }
static inline void *skb_put_data(struct sk_buff *s, const void *d, unsigned int n) { void *p = skb_put(s, n); memcpy(p, d, n); return p; }
static inline void *skb_push(struct sk_buff *s, unsigned int n) { s->data -= n; s->len += n; return s->data; }
static inline void *skb_pull(struct sk_buff *s, unsigned int n) { s->data += n; s->len -= n; return s->data; }
static inline void skb_reserve(struct sk_buff *s, int n) { s->data += n; s->tail += n; }
static inline void skb_trim(struct sk_buff *s, unsigned int n) { if (s->len > n) { s->len = n; s->tail = s->data + n; } }
static inline void __skb_trim(struct sk_buff *s, unsigned int n) { s->len = n; s->tail = s->data + n; }
static inline unsigned int skb_headroom(const struct sk_buff *s) { return s->data - s->head; }
static inline int skb_tailroom(const struct sk_buff *s) { return s->end - s->tail; }
static inline bool skb_cloned(const struct sk_buff *s) { return 0; }
static inline bool skb_is_nonlinear(const struct sk_buff *s) { return 0; }
static inline int skb_linearize(struct sk_buff *s) { return 0; }
static inline u16 skb_get_queue_mapping(const struct sk_buff *s) { return s->queue_mapping; }
static inline void skb_set_queue_mapping(struct sk_buff *s, u16 q) { s->queue_mapping = q; }
static inline void skb_orphan(struct sk_buff *s) { }
static inline void skb_queue_head_init(struct sk_buff_head *q) { q->next = q->prev = (struct sk_buff *)q; q->qlen = 0; }
#define __skb_queue_head_init skb_queue_head_init
static inline int skb_queue_empty(const struct sk_buff_head *q) { return q->next == (const struct sk_buff *)q; }
static inline u32 skb_queue_len(const struct sk_buff_head *q) { return q->qlen; }
static inline struct sk_buff *skb_peek(const struct sk_buff_head *q) { return skb_queue_empty(q) ? NULL : q->next; }
static inline struct sk_buff *skb_peek_tail(const struct sk_buff_head *q) { return skb_queue_empty(q) ? NULL : q->prev; }
static inline void __skb_insert(struct sk_buff *n, struct sk_buff *p, struct sk_buff *x, struct sk_buff_head *q)
{ n->next = x; n->prev = p; x->prev = n; p->next = n; q->qlen++; }
static inline void __skb_queue_tail(struct sk_buff_head *q, struct sk_buff *n) { __skb_insert(n, q->prev, (struct sk_buff *)q, q); }
static inline void __skb_queue_head(struct sk_buff_head *q, struct sk_buff *n) { __skb_insert(n, (struct sk_buff *)q, q->next, q); }
static inline void __skb_queue_after(struct sk_buff_head *q, struct sk_buff *p, struct sk_buff *n) { __skb_insert(n, p, p->next, q); }
static inline void __skb_unlink(struct sk_buff *s, struct sk_buff_head *q)
{ q->qlen--; s->next->prev = s->prev; s->prev->next = s->next; s->next = s->prev = NULL; }
static inline struct sk_buff *__skb_dequeue(struct sk_buff_head *q) { struct sk_buff *s = skb_peek(q); if (s) __skb_unlink(s, q); return s; }
static inline struct sk_buff *__skb_dequeue_tail(struct sk_buff_head *q) { struct sk_buff *s = skb_peek_tail(q); if (s) __skb_unlink(s, q); return s; }
#define skb_queue_tail __skb_queue_tail
#define skb_queue_head __skb_queue_head
#define skb_dequeue __skb_dequeue
#define skb_dequeue_tail __skb_dequeue_tail
#define skb_unlink __skb_unlink
static inline bool skb_queue_is_last(const struct sk_buff_head *q, const struct sk_buff *s) { return s->next == (const struct sk_buff *)q; }
static inline bool skb_queue_is_first(const struct sk_buff_head *q, const struct sk_buff *s) { return s->prev == (const struct sk_buff *)q; }
static inline struct sk_buff *skb_queue_next(const struct sk_buff_head *q, const struct sk_buff *s) { return s->next; }
static inline struct sk_buff *skb_queue_prev(const struct sk_buff_head *q, const struct sk_buff *s) { return s->prev; }
#define skb_queue_walk(q, s) for (s = (q)->next; s != (struct sk_buff *)(q); s = s->next)
#define skb_queue_walk_safe(q, s, t) for (s = (q)->next, t = s->next; s != (struct sk_buff *)(q); s = t, t = s->next)
static inline void skb_queue_splice_tail_init(struct sk_buff_head *de, struct sk_buff_head *a)
{ struct sk_buff *s; while ((s = __skb_dequeue(de)) != NULL) __skb_queue_tail(a, s); }
static inline void __skb_queue_purge(struct sk_buff_head *q) { struct sk_buff *s; while ((s = __skb_dequeue(q)) != NULL) kfree_skb(s); }
#define skb_queue_purge __skb_queue_purge

/* Ethernet */
#define ETH_ALEN 6
#define ETH_HLEN 14
#define ETH_P_PAE 0x888E
#define ETH_P_IP 0x0800
#define ETH_P_802_3 0x0001
#define ETH_FCS_LEN 4
#define ETH_DATA_LEN 1500
#define IFNAMSIZ 16
struct ethhdr { u8 h_dest[6]; u8 h_source[6]; __be16 h_proto; } __packed;
static inline bool is_multicast_ether_addr(const u8 *a) { return a[0] & 1; }
static inline bool is_broadcast_ether_addr(const u8 *a) { return (a[0] & a[1] & a[2] & a[3] & a[4] & a[5]) == 0xff; }
static inline bool is_zero_ether_addr(const u8 *a) { return !(a[0] | a[1] | a[2] | a[3] | a[4] | a[5]); }
static inline bool is_valid_ether_addr(const u8 *a) { return !is_multicast_ether_addr(a) && !is_zero_ether_addr(a); }
static inline bool ether_addr_equal(const u8 *a, const u8 *b) { return memcmp(a, b, 6) == 0; }
#define ether_addr_equal_64bits ether_addr_equal
static inline void ether_addr_copy(u8 *d, const u8 *s) { memcpy(d, s, 6); }
static inline void eth_broadcast_addr(u8 *a) { memset(a, 0xff, 6); }
static inline void eth_zero_addr(u8 *a) { memset(a, 0, 6); }
#define eth_random_addr(a) do { (a)[0] = 2; (a)[1] = 0x48; (a)[2] = 0x53; (a)[3] = prandom_u32(); (a)[4] = prandom_u32(); (a)[5] = prandom_u32(); } while (0)
#define random_ether_addr eth_random_addr

/* varios */
#define __must_hold(x)
#define DECLARE_WORK(n, f) struct work_struct n = { .func = (f) }
#define init_waitqueue_head(w) do { } while (0)
#define wake_up(w) do { } while (0)
#define wait_event_timeout(wq, cond, t) ({ unsigned long _f = jiffies + (t); while (!(cond) && time_before(jiffies, _f)) hl_ceder(); (cond) ? 1 : 0; })
void hl_ceder(void);
struct bcma_device_id { u16 manuf; u16 id; u8 rev; u8 class; };
#define BCMA_ANY_MANUF 0xFFFF
#define BCMA_ANY_ID 0xFFFF
#define BCMA_ANY_REV 0xFF
#define BCMA_ANY_CLASS 0xFF
#define BCMA_CORE(_manuf, _id, _rev, _class) { .manuf = _manuf, .id = _id, .rev = _rev, .class = _class, }
struct led_classdev { const char *name; const char *default_trigger; int brightness; void (*brightness_set)(struct led_classdev *, int); };
struct led_trigger { const char *name; };
struct gpio_chip { int base; };
struct gpio_desc;
struct bcm47xx_wdt { u32 (*timer_set)(struct bcm47xx_wdt *, u32); u32 (*timer_set_ms)(struct bcm47xx_wdt *, u32); u32 max_timer_ms; void *driver_data; };
static inline void *bcm47xx_wdt_get_drvdata(struct bcm47xx_wdt *w) { return w->driver_data; }
#define platform_device_register_data(a, b, c, d, e) ((struct platform_device *)0)
#define platform_device_unregister(p) do { } while (0)
#define WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD 0
#define kvmalloc(n, g) hl_malloc(n)
#define no_printk(f, a...) do { } while (0)
#define printk_once(f, a...) hl_log(1, f, ##a)
#define skb_queue_splice_tail(de, a) skb_queue_splice_tail_init(de, a)
void __iomem *ioremap(u32 fis, unsigned long tam);
int pci_register_driver(struct pci_driver *d);
#define SIMPLE_DEV_PM_OPS(a, b, c)
struct uart_port;

#include "hispa_80211.h"

#endif
