/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/red/ifconfig.c - Hispa-64, etapa 9: IFCONFIG (vale
 * también como un «ip» sencillo): lo que se sabe de la red.
 *   IFCONFIG            todo
 *   IFCONFIG NOMBRE     solo esa interfaz (lo, eth0...)
 * Cada interfaz (las de SIOCGIFCONF y las de /proc/net/dev), con los ioctl
 * SIOCGIFFLAGS, SIOCGIFINDEX, SIOCGIFHWADDR, SIOCGIFMTU, SIOCGIFADDR y
 * SIOCGIFNETMASK sobre un socket AF_INET/SOCK_DGRAM: si está arriba
 * (IFF_UP) y con enlace (IFF_RUNNING), su MAC, MTU, IP y máscara; los
 * paquetes y octetos, de /proc/net/dev; la puerta de enlace, de
 * /proc/net/route; los DNS, de /etc/resolv.conf. Si falta alguno de esos
 * archivos, lo dice y sigue. Acaba con «IFCONFIG: N interfaces». */
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include "red.h"

#define MAX_IF 64

static char nombres[MAX_IF][IFNAMSIZ];
static int n_if;

static void añade(const char *n)
{
	int i;
	for (i = 0; i < n_if; i++)
		if (!strcmp(nombres[i], n))
			return;
	if (n_if < MAX_IF) {
		snprintf(nombres[n_if], IFNAMSIZ, "%s", n);
		n_if++;
	}
}

/* /proc/net/dev: dos líneas de cabecera y luego «nombre: rx_octetos rx_paquetes
 * rx_errores rx_tirados ... (8 de recibir) tx_octetos tx_paquetes ...» */
static int estadística(const char *nombre, unsigned long long v[16])
{
	FILE *f = fopen("/proc/net/dev", "r");
	char l[512];
	int bien = 0;
	if (!f)
		return 0;
	while (fgets(l, sizeof l, f)) {
		char *dp = strchr(l, ':'), *p = l;
		if (!dp)
			continue;
		*dp = 0;
		while (*p == ' ')
			p++;
		if (nombre) {
			if (strcmp(p, nombre))
				continue;
			bien = sscanf(dp + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			              &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7],
			              &v[8], &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15]) >= 11;
			break;
		}
		añade(p);
		bien++;
	}
	fclose(f);
	return bien;
}

static const char *ipv4(struct sockaddr *s)
{
	static char b[4][INET_ADDRSTRLEN];
	static int k;
	char *p = b[k++ & 3];
	inet_ntop(AF_INET, &((struct sockaddr_in *)s)->sin_addr, p, INET_ADDRSTRLEN);
	return p;
}

static int bits(uint32_t m)
{
	int n = 0;
	m = ntohl(m);
	while (m & 0x80000000u) {
		n++;
		m <<= 1;
	}
	return n;
}

static void interfaz(int fd, const char *nombre)
{
	struct ifreq r;
	int fl = 0, tiene_fl;
	unsigned long long v[16];

	memset(&r, 0, sizeof r);
	snprintf(r.ifr_name, IFNAMSIZ, "%s", nombre);
	tiene_fl = ioctl(fd, SIOCGIFFLAGS, &r) == 0;
	if (tiene_fl)
		fl = r.ifr_flags;
	printf("%s: ", nombre);
	if (!tiene_fl)
		printf("(SIOCGIFFLAGS: %s)", strerror(errno));
	else
		printf("%s, %s%s%s", (fl & IFF_UP) ? "arriba" : "abajo",
		       (fl & IFF_RUNNING) ? "con enlace" : "sin enlace",
		       (fl & IFF_LOOPBACK) ? ", de bucle" : "",
		       (fl & IFF_BROADCAST) ? ", difusión" : "");
	if (ioctl(fd, SIOCGIFINDEX, &r) == 0)
		printf("; índice %d", r.ifr_ifindex);
	printf("\n ");
	if (ioctl(fd, SIOCGIFHWADDR, &r) == 0) {
		unsigned char *m = (unsigned char *)r.ifr_hwaddr.sa_data;
		if (r.ifr_hwaddr.sa_family == ARPHRD_ETHER)
			printf(" MAC %02x:%02x:%02x:%02x:%02x:%02x;", m[0], m[1], m[2], m[3], m[4], m[5]);
		else if (r.ifr_hwaddr.sa_family == ARPHRD_LOOPBACK)
			printf(" sin MAC (bucle);");
		else
			printf(" hardware de tipo %d;", r.ifr_hwaddr.sa_family);
	}
	if (ioctl(fd, SIOCGIFMTU, &r) == 0)
		printf(" MTU %d;", r.ifr_mtu);
	if (ioctl(fd, SIOCGIFADDR, &r) == 0) {
		printf(" IP %s", ipv4(&r.ifr_addr));
		if (ioctl(fd, SIOCGIFNETMASK, &r) == 0)
			printf(", máscara %s (/%d)", ipv4(&r.ifr_netmask),
			       bits(((struct sockaddr_in *)&r.ifr_netmask)->sin_addr.s_addr));
	} else
		printf(" sin IP");
	printf("\n");
	if (estadística(nombre, v))
		printf("  recibidos: %llu paquetes (%llu octetos, %llu errores); enviados: %llu paquetes (%llu octetos, %llu errores)\n",
		       v[1], v[0], v[2], v[9], v[8], v[10]);
}

/* /proc/net/route: «Iface Destination Gateway Flags RefCnt Use Metric Mask...»,
 * en hexadecimal y en el orden de la máquina (la de Destination 0 es la puerta) */
static void puertas(void)
{
	FILE *f = fopen("/proc/net/route", "r");
	char l[512], ifn[IFNAMSIZ + 1];
	unsigned dst, gw, fl, mask;
	int n = 0;
	if (!f) {
		printf("puerta de enlace: no se sabe (no hay /proc/net/route)\n");
		return;
	}
	if (!fgets(l, sizeof l, f))
		l[0] = 0;
	while (fgets(l, sizeof l, f)) {
		struct sockaddr_in s;
		int metr;
		if (sscanf(l, "%16s %x %x %x %*d %*d %d %x", ifn, &dst, &gw, &fl, &metr, &mask) < 6)
			continue;
		if (dst != 0 || !(fl & 0x2) || !(fl & 0x1))     /* RTF_UP | RTF_GATEWAY */
			continue;
		memset(&s, 0, sizeof s);
		s.sin_family = AF_INET;
		s.sin_addr.s_addr = gw;
		printf("puerta de enlace: %s (por %s)\n", ipv4((struct sockaddr *)&s), ifn);
		n++;
	}
	fclose(f);
	if (!n)
		printf("puerta de enlace: ninguna\n");
}

static void dns(void)
{
	FILE *f = fopen("/etc/resolv.conf", "r");
	char l[512], d[256];
	int n = 0;
	if (!f) {
		printf("DNS: ninguno (no hay /etc/resolv.conf)\n");
		return;
	}
	while (fgets(l, sizeof l, f))
		if (sscanf(l, " nameserver %255s", d) == 1) {
			printf("%s %s", n ? "," : "DNS:", d);
			n++;
		}
	fclose(f);
	printf(n ? "\n" : "DNS: ninguno (nada en /etc/resolv.conf)\n");
}

int main(int argc, char **argv)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0), i;
	struct ifconf c;
	static struct ifreq lista[MAX_IF];
	unsigned long long v[16];

	if (fd < 0) {
		printf("IFCONFIG: no se abre el socket: %s\n", strerror(errno));
		return 1;
	}
	if (argc > 1) {
		struct ifreq r;
		memset(&r, 0, sizeof r);
		snprintf(r.ifr_name, IFNAMSIZ, "%s", argv[1]);
		if (ioctl(fd, SIOCGIFFLAGS, &r) < 0) {
			printf("IFCONFIG: no hay interfaz «%s»: %s\n", argv[1], strerror(errno));
			return 1;
		}
		interfaz(fd, argv[1]);
		return 0;
	}
	c.ifc_len = sizeof lista;
	c.ifc_req = lista;
	if (ioctl(fd, SIOCGIFCONF, &c) == 0) {
		for (i = 0; i < (int)(c.ifc_len / sizeof(struct ifreq)); i++)
			añade(lista[i].ifr_name);
	} else
		printf("IFCONFIG: SIOCGIFCONF: %s\n", strerror(errno));
	if (!estadística(0, v))
		printf("(no hay /proc/net/dev: sin paquetes ni octetos)\n");
	for (i = 0; i < n_if; i++)
		interfaz(fd, nombres[i]);
	puertas();
	dns();
	printf("IFCONFIG: %d interfaces\n", n_if);
	close(fd);
	return 0;
}
