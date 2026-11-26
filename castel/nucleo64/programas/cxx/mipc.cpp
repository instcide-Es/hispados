// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
// nucleo64/programas/cxx/mipc.cpp - Hispa-64, etapa 10 d (docs/64/HISPA-64.md,
// sección 21): MIPC, dos procesos que se hablan como los de Ladybird (el
// navegador y su WebContent):
//   - el primero hace un socketpair de flujo y arranca al segundo con
//     posix_spawn (el mismo programa, «--hijo»), con su extremo en el
//     descriptor 3 (posix_spawn_file_actions_adddup2) y SOCKET_TAKEOVER en el
//     entorno, como LibCore;
//   - los mensajes, como los de LibIPC: el largo (u32), el número del
//     mensaje (u32) y lo suyo, con los descriptores por SCM_RIGHTS; cada lado
//     manda desde un hilo propio (una cola con su mutex y su condición, como
//     IPC::TransportSocket) y lee en un bucle de sucesos con poll (el socket y
//     un eventfd para despertarlo, como Core::EventLoop);
//   - dos mapas de bits de 256 x 256 (Core::AnonymousBuffer: memfd_create,
//     ftruncate y mmap MAP_SHARED) que se pasan una vez por SCM_RIGHTS; el
//     segundo pinta en ellos 60 cuadros, uno en cada uno por turno, y dice
//     su suma; el primero la mira en su memoria (la misma);
//   - SO_PEERCRED (el pid del otro) y el final: «adiós», y el segundo acaba
//     con 0.
// Al final, «MIPC: todo bien (N pruebas)» y código 0. Corre igual en Linux.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <deque>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <optional>
#include <span>
#include <atomic>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/eventfd.h>

extern char **environ;

static int bien, mal;
static void mira(bool c, const char *d, int línea)
{
	if (c) bien++;
	else { mal++; std::printf("  MAL: %s (línea %d)\n", d, línea); std::fflush(stdout); }
}
#define MIRA(c, d) mira((c), (d), __LINE__)

// ---- Core::AnonymousBuffer ----
class Búfer {
public:
	static std::optional<Búfer> crea(size_t n)
	{
		int fd = memfd_create("", MFD_CLOEXEC);
		if (fd < 0) return {};
		if (ftruncate(fd, n) < 0) { close(fd); return {}; }
		return Búfer::de(fd, n);
	}
	static std::optional<Búfer> de(int fd, size_t n)
	{
		void *p = mmap(nullptr, n, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (p == MAP_FAILED) { close(fd); return {}; }
		Búfer b;
		b.fd_ = fd; b.p_ = static_cast<uint8_t *>(p); b.n_ = n;
		return b;
	}
	Búfer() = default;
	Búfer(Búfer &&o) noexcept { *this = std::move(o); }
	Búfer &operator=(Búfer &&o) noexcept
	{
		std::swap(fd_, o.fd_); std::swap(p_, o.p_); std::swap(n_, o.n_);
		return *this;
	}
	~Búfer() { if (p_) munmap(p_, n_); if (fd_ >= 0) close(fd_); }
	int fd() const { return fd_; }
	std::span<uint8_t> datos() { return { p_, n_ }; }
private:
	int fd_ = -1;
	uint8_t *p_ = nullptr;
	size_t n_ = 0;
};

// ---- un mensaje (LibIPC): el número, lo suyo y los descriptores ----
struct Mensaje {
	uint32_t id = 0;
	std::vector<uint8_t> datos;
	std::vector<int> fds;
	void pon32(uint32_t v) { uint8_t b[4]; std::memcpy(b, &v, 4); datos.insert(datos.end(), b, b + 4); }
	uint32_t lee32(size_t &i) const { uint32_t v = 0; if (i + 4 <= datos.size()) std::memcpy(&v, &datos[i], 4); i += 4; return v; }
};

enum : uint32_t { PON_MAPAS = 1, PINTA = 2, PINTADO = 3, ADIÓS = 4, LISTO = 5 };

// ---- IPC::TransportSocket: manda desde su hilo; recibe en el bucle ----
class Transporte {
public:
	explicit Transporte(int fd) : fd_(fd), hilo_([this] { manda_cola(); }) {}
	~Transporte()
	{
		{ std::lock_guard<std::mutex> g(m_); fin_ = true; }
		c_.notify_all();
		hilo_.join();
		close(fd_);
	}
	int fd() const { return fd_; }
	void manda(Mensaje m)
	{
		{ std::lock_guard<std::mutex> g(m_); cola_.push_back(std::move(m)); }
		c_.notify_one();
	}
	// lee lo que haya (sin esperar) y saca los mensajes enteros; falso si el otro se ha ido
	bool lee(std::vector<Mensaje> &salen)
	{
		for (;;) {
			uint8_t b[4096];
			char c[CMSG_SPACE(sizeof(int) * 16)];
			iovec v{ b, sizeof b };
			msghdr mh{};
			mh.msg_iov = &v; mh.msg_iovlen = 1; mh.msg_control = c; mh.msg_controllen = sizeof c;
			ssize_t n = recvmsg(fd_, &mh, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
			if (n < 0) return errno == EAGAIN;
			if (n == 0) return false;
			for (cmsghdr *h = CMSG_FIRSTHDR(&mh); h; h = CMSG_NXTHDR(&mh, h))
				if (h->cmsg_level == SOL_SOCKET && h->cmsg_type == SCM_RIGHTS) {
					size_t k = (h->cmsg_len - CMSG_LEN(0)) / sizeof(int);
					for (size_t j = 0; j < k; j++) fds_.push_back(reinterpret_cast<int *>(CMSG_DATA(h))[j]);
				}
			ent_.insert(ent_.end(), b, b + n);
			saca(salen);
		}
	}
private:
	void saca(std::vector<Mensaje> &salen)
	{
		while (ent_.size() >= 9) {
			uint32_t largo, id, nfd;
			std::memcpy(&largo, &ent_[0], 4);
			if (ent_.size() < 4 + (size_t)largo) return;
			std::memcpy(&id, &ent_[4], 4);
			nfd = ent_[8];
			Mensaje m;
			m.id = id;
			m.datos.assign(ent_.begin() + 9, ent_.begin() + 4 + largo);
			for (uint32_t j = 0; j < nfd && !fds_.empty(); j++) { m.fds.push_back(fds_.front()); fds_.pop_front(); }
			ent_.erase(ent_.begin(), ent_.begin() + 4 + largo);
			salen.push_back(std::move(m));
		}
	}
	void manda_cola()
	{
		std::unique_lock<std::mutex> g(m_);
		for (;;) {
			c_.wait(g, [this] { return fin_ || !cola_.empty(); });
			if (cola_.empty()) return;
			Mensaje m = std::move(cola_.front());
			cola_.pop_front();
			g.unlock();
			std::vector<uint8_t> t(9);
			uint32_t largo = 5 + m.datos.size(), nfd = m.fds.size();
			std::memcpy(&t[0], &largo, 4);
			std::memcpy(&t[4], &m.id, 4);
			t[8] = (uint8_t)nfd;
			t.insert(t.end(), m.datos.begin(), m.datos.end());
			size_t hecho = 0;
			bool fds_van = !m.fds.empty();
			while (hecho < t.size()) {
				iovec v{ t.data() + hecho, t.size() - hecho };
				msghdr mh{};
				char c[CMSG_SPACE(sizeof(int) * 16)];
				mh.msg_iov = &v; mh.msg_iovlen = 1;
				if (fds_van) {
					std::memset(c, 0, sizeof c);
					mh.msg_control = c;
					mh.msg_controllen = CMSG_SPACE(sizeof(int) * nfd);
					cmsghdr *h = CMSG_FIRSTHDR(&mh);
					h->cmsg_level = SOL_SOCKET; h->cmsg_type = SCM_RIGHTS;
					h->cmsg_len = CMSG_LEN(sizeof(int) * nfd);
					std::memcpy(CMSG_DATA(h), m.fds.data(), sizeof(int) * nfd);
				}
				ssize_t n = sendmsg(fd_, &mh, MSG_NOSIGNAL);
				if (n < 0) {
					if (errno == EAGAIN) { pollfd p{ fd_, POLLOUT, 0 }; poll(&p, 1, 100); continue; }
					break;
				}
				fds_van = false;
				hecho += n;
			}
			g.lock();
		}
	}
	int fd_;
	std::mutex m_;
	std::condition_variable c_;
	std::deque<Mensaje> cola_;
	bool fin_ = false;
	std::vector<uint8_t> ent_;
	std::deque<int> fds_;
	std::thread hilo_;
};

// ---- Core::EventLoop: poll del socket y de un eventfd para despertarlo ----
class Bucle {
public:
	Bucle() : ev_(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {}
	~Bucle() { close(ev_); }
	void despierta() { uint64_t uno = 1; if (write(ev_, &uno, 8) != 8) {} }
	// hasta que «sigue» diga que no; cada vuelta, lo que llega
	int corre(Transporte &t, const std::function<bool(Mensaje &)> &al_llegar, int plazo_ms)
	{
		int vueltas = 0;
		auto t0 = std::chrono::steady_clock::now();
		for (;;) {
			pollfd p[2] = { { t.fd(), POLLIN, 0 }, { ev_, POLLIN, 0 } };
			int r = poll(p, 2, 200);
			vueltas++;
			if (r < 0 && errno != EINTR) return -1;
			if (p[1].revents & POLLIN) { uint64_t x; if (read(ev_, &x, 8) != 8) {} }
			std::vector<Mensaje> llegan;
			bool vivo = t.lee(llegan);
			for (auto &m : llegan)
				if (!al_llegar(m)) return vueltas;
			if (!vivo) return -2;
			if (std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(plazo_ms)) return -3;
		}
	}
private:
	int ev_;
};

constexpr int AN = 256, AL = 256, CUADROS = 60;

static uint32_t suma(std::span<uint8_t> d)
{
	uint32_t s = 0;
	for (size_t i = 0; i < d.size(); i += 4) { uint32_t v; std::memcpy(&v, &d[i], 4); s = s * 31 + v; }
	return s;
}

// ---- el segundo (WebContent): pinta en los mapas que le dan ----
static int hijo()
{
	const char *t = getenv("SOCKET_TAKEOVER");
	if (!t || std::strcmp(t, "WebContent:3") != 0) return 10;
	int fd = 3;
	if (fcntl(fd, F_GETFD) < 0) return 11;
	ucred cr{};
	socklen_t l = sizeof cr;
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &l) < 0 || cr.pid != getppid()) return 12;
	Transporte tr(fd);
	Bucle bucle;
	std::vector<Búfer> mapas;
	int pintados = 0;
	Mensaje listo; listo.id = LISTO; listo.pon32((uint32_t)getpid());
	tr.manda(std::move(listo));
	int r = bucle.corre(tr, [&](Mensaje &m) {
		size_t i = 0;
		if (m.id == PON_MAPAS) {
			for (int f : m.fds) { auto b = Búfer::de(f, AN * AL * 4); if (b) mapas.push_back(std::move(*b)); }
			return true;
		}
		if (m.id == PINTA) {
			uint32_t cuál = m.lee32(i), cuadro = m.lee32(i);
			if (cuál >= mapas.size()) return false;
			auto d = mapas[cuál].datos();
			for (int y = 0; y < AL; y++)
				for (int x = 0; x < AN; x++) {
					uint32_t v = 0xFF000000u | ((x + cuadro) & 255) << 16 | ((y * 2 + cuadro) & 255) << 8 | ((x ^ y) & 255);
					std::memcpy(&d[(y * AN + x) * 4], &v, 4);
				}
			Mensaje h; h.id = PINTADO; h.pon32(cuál); h.pon32(cuadro); h.pon32(suma(d));
			tr.manda(std::move(h));
			pintados++;
			return true;
		}
		return m.id != ADIÓS;
	}, 60000);
	return r > 0 && pintados == CUADROS && mapas.size() == 2 ? 0 : 20;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IOLBF, 0);
	if (argc > 1 && std::strcmp(argv[1], "--hijo") == 0) return hijo();
	std::printf("MIPC: dos procesos que se hablan como los de Ladybird (C++%ld, %s)\n", (long)(__cplusplus / 100 % 100), __VERSION__);
	int s[2];
	MIRA(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0, "socketpair");
	// el segundo, con su extremo en el 3 (y sin el nuestro: CLOEXEC)
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, s[1], 3);
	std::vector<std::string> ent;
	for (char **e = environ; *e; e++) ent.emplace_back(*e);
	ent.emplace_back("SOCKET_TAKEOVER=WebContent:3");
	std::vector<char *> envp;
	for (auto &e : ent) envp.push_back(e.data());
	envp.push_back(nullptr);
	char yo[256];
	ssize_t ly = readlink("/proc/self/exe", yo, sizeof yo - 1);
	if (ly > 0) yo[ly] = 0; else std::snprintf(yo, sizeof yo, "%s", argv[0]);
	char arg1[] = "--hijo";
	char *args[] = { yo, arg1, nullptr };
	pid_t h = -1;
	MIRA(posix_spawn(&h, yo, &fa, nullptr, args, envp.data()) == 0 && h > 0, "posix_spawn del segundo (adddup2 al 3)");
	posix_spawn_file_actions_destroy(&fa);
	close(s[1]);
	int pintados = 0, bien_suma = 0, listo = 0;
	auto t0 = std::chrono::steady_clock::now();
	{
		Transporte tr(s[0]);
		Bucle bucle;
		std::vector<Búfer> mapas;
		for (int i = 0; i < 2; i++) { auto b = Búfer::crea(AN * AL * 4); if (b) mapas.push_back(std::move(*b)); }
		MIRA(mapas.size() == 2, "dos mapas de bits (memfd_create, ftruncate, mmap MAP_SHARED)");
		Mensaje pm; pm.id = PON_MAPAS;
		for (auto &b : mapas) pm.fds.push_back(b.fd());
		tr.manda(std::move(pm));
		auto pide = [&](uint32_t cuadro) {
			Mensaje p; p.id = PINTA; p.pon32(cuadro % 2); p.pon32(cuadro);
			tr.manda(std::move(p));
		};
		int r = bucle.corre(tr, [&](Mensaje &m) {
			size_t i = 0;
			if (m.id == LISTO) {
				listo = m.lee32(i) == (uint32_t)h;
				pide(0);
				return true;
			}
			if (m.id == PINTADO) {
				uint32_t cuál = m.lee32(i), cuadro = m.lee32(i), su = m.lee32(i);
				if (cuál < 2 && su == suma(mapas[cuál].datos())) bien_suma++;
				pintados++;
				if (cuadro + 1 < CUADROS) { pide(cuadro + 1); return true; }
				Mensaje a; a.id = ADIÓS; tr.manda(std::move(a));
				return false;
			}
			return true;
		}, 60000);
		MIRA(r > 0, "el bucle de sucesos (poll del socket y de un eventfd)");
		ucred cr{};
		socklen_t l = sizeof cr;
		MIRA(getsockopt(s[0], SOL_SOCKET, SO_PEERCRED, &cr, &l) == 0 && cr.pid == getpid(), "SO_PEERCRED de un socketpair: este mismo");
	}
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
	MIRA(listo == 1, "el segundo dice su pid (getpid = el de posix_spawn)");
	MIRA(pintados == CUADROS && bien_suma == CUADROS, "60 cuadros pintados por el segundo, con la misma suma en la memoria del primero");
	int st = 0;
	MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 0, "el segundo acaba con 0 (SOCKET_TAKEOVER, SO_PEERCRED, sus mapas)");
	std::printf("  posix_spawn con el socket en el 3 y SOCKET_TAKEOVER; mensajes de LibIPC con SCM_RIGHTS, desde un hilo que manda y un bucle con poll\n");
	std::printf("  dos mapas de 256 x 256 compartidos (memfd): %d cuadros pintados por el otro proceso, la suma bien, en %lld ms\n", pintados, (long long)ms);
	if (mal) std::printf("MIPC: %d mal de %d\n", mal, bien + mal);
	else std::printf("MIPC: todo bien (%d pruebas)\n", bien);
	return mal ? 1 : 0;
}
