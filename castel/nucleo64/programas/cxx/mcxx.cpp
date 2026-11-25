/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/cxx/mcxx.cpp - Hispa-64, etapa 10 (docs/64/HISPA-64.md,
 * sección 16): MCXX, la prueba de C++ (C++17, la libstdc++ con musl, cxx.sh)
 * como las de musl: una línea por grupo y, al final, «MCXX: todo bien (N
 * pruebas)» y código 0 (o «MAL: ...» y código 1). Lo que pide un navegador:
 * los constructores de lo global y thread_local (con su destructor), las
 * excepciones (entre funciones, otra vez lanzadas, exception_ptr, las del
 * sistema: bad_alloc, out_of_range), RTTI y dynamic_cast, las plantillas y
 * las lambdas con std::function, los contenedores (vector, map,
 * unordered_map, set, deque, list), los algoritmos, string y string_view,
 * los flujos (ostringstream, istringstream, fstream de /SALUDO.TXT), los
 * números (to_string, stod, iostream con coma flotante), regex, los hilos
 * (std::thread a cientos, mutex, condition_variable con wait_for, atomic,
 * future y async), chrono y shared_ptr, weak_ptr y unique_ptr. */
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <deque>
#include <list>
#include <algorithm>
#include <numeric>
#include <functional>
#include <memory>
#include <stdexcept>
#include <exception>
#include <typeinfo>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <chrono>
#include <regex>
#include <optional>
#include <variant>
#include <cstdio>
#include <cstring>
#include <cmath>

static int bien_, mal_;
#define MIRA(c, d) do { if (c) bien_++; else { mal_++; std::printf("  MAL: %s (línea %d)\n", (d), __LINE__); std::fflush(stdout); } } while (0)

/* lo global, con su constructor (antes de main) */
struct Global {
	int v;
	Global() : v(1234) {}
};
static Global global_;

/* thread_local con destructor: cuenta cuántos se han destruido */
static std::atomic<int> destruidos{0};
struct PorHilo {
	int n = 7;
	~PorHilo() { destruidos++; }
};
static thread_local PorHilo por_hilo;

struct Figura {
	virtual ~Figura() {}
	virtual double área() const = 0;
};
struct Cuadrado : Figura {
	double l;
	explicit Cuadrado(double x) : l(x) {}
	double área() const override { return l * l; }
};
struct Círculo : Figura {
	double r;
	explicit Círculo(double x) : r(x) {}
	double área() const override { return 3.0 * r * r; }
};

static void lanza(int k)
{
	if (k == 0) throw std::runtime_error("de lanza(0)");
	if (k == 1) throw 42;
	if (k == 2) throw Cuadrado(2);
	lanza(k - 3);
}

template <typename T> static T suma(const std::vector<T> &v) { return std::accumulate(v.begin(), v.end(), T()); }

int main()
{
	std::printf("MCXX: C++17 con la libstdc++ y musl (cxx.sh)\n");
	std::fflush(stdout);
	/* lo global */
	MIRA(global_.v == 1234, "el constructor de lo global, antes de main");
	/* las excepciones */
	{
		std::string q;
		try { lanza(3); } catch (const std::exception &e) { q = e.what(); }
		MIRA(q == "de lanza(0)", "una excepción entre funciones (runtime_error)");
		int n = 0;
		try { lanza(1); } catch (int x) { n = x; }
		MIRA(n == 42, "lanzar un int");
		double a = 0;
		try { lanza(2); } catch (const Figura &f) { a = f.área(); }
		MIRA(a == 4.0, "lanzar un objeto y atraparlo por su base");
		std::exception_ptr ep;
		try { throw std::logic_error("guardada"); } catch (...) { ep = std::current_exception(); }
		q.clear();
		try { std::rethrow_exception(ep); } catch (const std::logic_error &e) { q = e.what(); }
		MIRA(q == "guardada", "exception_ptr y rethrow_exception");
		bool oor = false;
		try { std::vector<int> v(3); (void)v.at(7); } catch (const std::out_of_range &) { oor = true; }
		MIRA(oor, "vector::at fuera: out_of_range");
		bool ba = false;
		try { char *volatile p = new char[size_t(1) << 50]; p[0] = 1; delete[] p; } catch (const std::bad_alloc &) { ba = true; }
		MIRA(ba, "pedir 1 PB: bad_alloc");
		int n2 = 0;
		try { try { throw 5; } catch (int x) { n2 = x; throw; } } catch (int y) { n2 += y; }
		MIRA(n2 == 10, "throw; (otra vez la misma)");
		std::printf("  las excepciones: entre funciones, por su base, exception_ptr, out_of_range, bad_alloc, throw;\n");
	}
	/* RTTI y lo virtual */
	{
		std::vector<std::unique_ptr<Figura>> fs;
		fs.push_back(std::make_unique<Cuadrado>(3));
		fs.push_back(std::make_unique<Círculo>(1));
		double t = 0;
		for (auto &f : fs) t += f->área();
		MIRA(t == 12.0, "las funciones virtuales");
		MIRA(dynamic_cast<Círculo *>(fs[1].get()) != nullptr && dynamic_cast<Círculo *>(fs[0].get()) == nullptr, "dynamic_cast");
		MIRA(typeid(*fs[0]) == typeid(Cuadrado) && std::strstr(typeid(*fs[1]).name(), "rculo") != nullptr, "typeid y su nombre");
	}
	/* los contenedores y los algoritmos */
	{
		std::vector<int> v(1000);
		std::iota(v.begin(), v.end(), 1);
		MIRA(suma(v) == 500500, "una plantilla (accumulate)");
		std::vector<int> w = v;
		std::reverse(w.begin(), w.end());
		std::sort(w.begin(), w.end());
		MIRA(w == v, "reverse y sort");
		std::map<std::string, int> m;
		std::unordered_map<std::string, int> u;
		for (int i = 0; i < 500; i++) { m["k" + std::to_string(i)] = i; u["k" + std::to_string(i)] = i * 2; }
		MIRA(m.size() == 500 && m["k250"] == 250 && u["k499"] == 998 && m.begin()->first == "k0", "map y unordered_map");
		std::set<int> s(v.begin(), v.end());
		std::deque<int> d(v.begin(), v.begin() + 10);
		std::list<int> l(v.begin(), v.begin() + 5);
		d.push_front(0);
		l.remove(3);
		MIRA(s.count(999) && !s.count(1001) && d.front() == 0 && d.size() == 11 && l.size() == 4, "set, deque y list");
		auto it = std::find_if(v.begin(), v.end(), [](int x) { return x * x > 5000; });
		MIRA(it != v.end() && *it == 71, "find_if con una lambda");
		std::function<int(int)> fib = [&fib](int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); };
		MIRA(fib(20) == 6765, "std::function recursiva");
		std::optional<int> op = 3;
		std::variant<int, std::string> va = std::string("texto");
		MIRA(op.value_or(0) == 3 && std::get<std::string>(va) == "texto", "optional y variant");
		std::printf("  contenedores y algoritmos: vector, map, unordered_map, set, deque, list, sort, find_if, function, optional, variant\n");
	}
	/* los textos y los flujos */
	{
		std::string s = "Hispa-64 habla C++";
		std::string_view sv(s);
		MIRA(sv.substr(9, 5) == "habla" && s.find("C++") == 15, "string y string_view");
		std::ostringstream o;
		o << 3.25 << ' ' << 1e10 << ' ' << std::hex << 255 << ' ' << std::dec << -7;
		MIRA(o.str() == "3.25 1e+10 ff -7", "ostringstream (coma flotante, hex)");
		std::istringstream i("10 20 30 hola");
		int a, b, c;
		std::string pal;
		i >> a >> b >> c >> pal;
		MIRA(a + b + c == 60 && pal == "hola", "istringstream");
		MIRA(std::stod("2.5e3") == 2500.0 && std::to_string(17) == "17" && std::stoi("-42") == -42, "stod, stoi y to_string");
		std::ifstream f("/SALUDO.TXT");
		std::string línea;
		std::getline(f, línea);
		MIRA(f.good() && !línea.empty(), "ifstream de /SALUDO.TXT");
		std::regex r("([a-z]+)@([a-z]+)\\.es");
		std::smatch mr;
		std::string correo = "escribe a carlos@hispados.es hoy";
		MIRA(std::regex_search(correo, mr, r) && mr[2] == "hispados", "regex");
		MIRA(std::fabs(std::sqrt(2.0) - 1.41421356) < 1e-6, "cmath");
		std::printf("  textos y flujos: string_view, ostringstream, istringstream, stod, ifstream («%s»), regex\n", línea.c_str());
	}
	/* los hilos */
	{
		const int N = 200;
		std::atomic<int> cuenta{0};
		std::mutex mu;
		long total = 0;
		std::vector<std::thread> hs;
		for (int i = 0; i < N; i++)
			hs.emplace_back([&, i] {
				cuenta++;
				std::lock_guard<std::mutex> g(mu);
				total += i;
				(void)por_hilo.n;
			});
		for (auto &h : hs) h.join();
		MIRA(cuenta == N && total == long(N) * (N - 1) / 2, "200 hilos a la vez (std::thread, mutex, atomic)");
		MIRA(destruidos == N, "thread_local: su destructor, al acabar cada hilo");
		std::condition_variable cv;
		bool listo = false;
		std::thread t([&] {
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
			std::lock_guard<std::mutex> g(mu);
			listo = true;
			cv.notify_one();
		});
		{
			std::unique_lock<std::mutex> g(mu);
			auto t0 = std::chrono::steady_clock::now();
			bool a_tiempo = cv.wait_for(g, std::chrono::seconds(5), [&] { return listo; });
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
			MIRA(a_tiempo && ms >= 20 && ms < 4000, "condition_variable::wait_for (pthread_cond_clockwait)");
			bool fuera = cv.wait_for(g, std::chrono::milliseconds(50), [] { return false; });
			MIRA(!fuera, "wait_for sin aviso: se acaba el plazo");
		}
		t.join();
		auto fu = std::async(std::launch::async, [] { return 6 * 7; });
		MIRA(fu.get() == 42, "async y future");
		std::promise<std::string> pr;
		auto f2 = pr.get_future();
		std::thread t2([&] { pr.set_value("prometido"); });
		MIRA(f2.get() == "prometido", "promise");
		t2.join();
		std::timed_mutex tm;
		tm.lock();
		bool tomado = tm.try_lock_for(std::chrono::milliseconds(20));
		MIRA(!tomado, "timed_mutex::try_lock_for (pthread_mutex_clocklock): se acaba el plazo");
		tm.unlock();
		std::printf("  hilos: %d std::thread, thread_local (%d destruidos), condition_variable, async, promise, timed_mutex\n", N, destruidos.load());
	}
	/* la memoria */
	{
		auto a = std::make_shared<std::vector<int>>(100000, 3);
		std::weak_ptr<std::vector<int>> w = a;
		auto b = a;
		MIRA(a.use_count() == 2 && w.lock()->size() == 100000, "shared_ptr y weak_ptr");
		a.reset();
		b.reset();
		MIRA(w.expired(), "weak_ptr, caducado al soltar el último");
		std::vector<std::unique_ptr<std::string>> muchos;
		for (int i = 0; i < 20000; i++) muchos.push_back(std::make_unique<std::string>(100, 'x'));
		MIRA(muchos.size() == 20000 && (*muchos[19999])[99] == 'x', "20.000 bloques con new");
	}
	std::cout << "MCXX: " << (mal_ ? std::to_string(mal_) + " mal de " + std::to_string(bien_ + mal_) : "todo bien (" + std::to_string(bien_) + " pruebas)") << std::endl;
	return mal_ ? 1 : 0;
}
