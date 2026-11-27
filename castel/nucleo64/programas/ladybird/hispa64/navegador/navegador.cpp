/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/navegador/navegador.cpp - Hispa-64, etapa 10 h (docs/64/HISPA-64.md,
 * sección 25): NAVEGADOR, Ladybird con ventana en el escritorio de Hispa-64.
 *
 * Ladybird se parte en procesos: el Hub (el navegador: las pestañas, el
 * historial, los ajustes, y quien lanza WebContent, RequestServer,
 * ImageDecoder y el Compositor, que pinta con Skia en el procesador) y la
 * interfaz (en el Ladybird de siempre, la de Qt o la de AppKit). NAVEGADOR es
 * una interfaz propia, pequeña, contra las ventanas de Hispa-64 (VENTANAS:
 * ventana.c, sobre la capa «so» de la oficina): lanza el Hub, le habla por
 * su protocolo (conexion.h), y
 *   - pinta lo que el Compositor deja en los búferes compartidos de cada
 *     página (WebView::PageBackingStores, como RemoteView de Qt) debajo de
 *     su barra: las pestañas, ← → ↻ y la barra de direcciones (con la letra
 *     de la oficina, la DejaVu Sans);
 *   - le pasa al Hub el teclado (con los acentos y la ñ), el ratón y la
 *     rueda; Ctrl+L, la barra; Ctrl+T, otra pestaña; Ctrl+W, cerrarla;
 *     Ctrl+Tab, la siguiente; F5 o Ctrl+R, recargar; Alt+← y Alt+→ (o los
 *     botones), atrás y adelante;
 *   - el tamaño de la ventana es el de la página (view_set_viewport).
 * Uso: NAVEGADOR [URL]   (sin URL, https://hispados.es/)
 * Lo que necesita Ladybird (los recursos, las letras, las autoridades) lo
 * deja MWEB --prepara en /tmp/web, como para el navegador sin ventana. */

#include <AK/Hex.h>
#include <AK/Utf8View.h>
#include <LibCore/Environment.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Notifier.h>
#include <LibCore/Process.h>
#include <LibCore/System.h>
#include <LibCore/Timer.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/SystemTheme.h>
#include <LibIPC/Encoder.h>
#include <LibIPC/Transport.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWebCommon/UIEvents/KeyCode.h>
#include <LibWebView/PageBackingStores.h>
#include <LibWebView/URL.h>

#include "conexion.h"
#include "ventana.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace Navegador {

// las medidas de la barra (en puntos de la pantalla)
static constexpr int ALTO_PESTAÑAS = 30;
static constexpr int ALTO_HERRAMIENTAS = 38;
static constexpr int ALTO_BARRA = ALTO_PESTAÑAS + ALTO_HERRAMIENTAS;
static constexpr int ANCHO_BOTÓN = 34;
static constexpr int ANCHO_PESTAÑA = 210;
static constexpr int TAM = 13 * 4;                  // 13 puntos (en cuartos, como la oficina)
static constexpr char const* INICIO = "https://hispados.es/";

// los colores (como los de VENTANAS: lo activo se distingue por la luz)
static constexpr u32 C_FONDO = 0xDDE1E6;
static constexpr u32 C_PESTAÑA = 0xF6F7F9;
static constexpr u32 C_PESTAÑA_OTRA = 0xC9CED5;
static constexpr u32 C_TEXTO = 0x1D2228;
static constexpr u32 C_GRIS = 0x8A9099;
static constexpr u32 C_CAMPO = 0xFFFFFF;
static constexpr u32 C_BORDE = 0x9AA2AC;
static constexpr u32 C_FOCO = 0x2E6FD8;
static constexpr u32 C_SELECCIÓN = 0xBCD3F7;

struct Pestaña {
    AK_ALLOC_WITH_KMALLOC;

    u64 vista { 0 };
    WebView::PageBackingStores búferes;
    u64 página_búferes { 0 };
    String título;
    String dirección;
    bool cargando { false };
    bool atrás { false };
    bool adelante { false };
    bool roto { false };
    Gfx::Cursor cursor { Gfx::StandardCursor::Arrow };
};

class Interfaz {
public:
    ErrorOr<int> empieza(Main::Arguments);

    // lo que cuenta el Hub (Conexion)
    Pestaña* pestaña(u64 vista);
    void ha_cambiado_la_barra() { m_barra_sucia = true; pide_pintar(); }
    void ha_cambiado_la_página(u64 vista);
    void cierra_vista(u64 vista);
    void activa_vista(u64 vista);
    void adopta_vista(u64 vista, bool activar);
    void abre_pestaña(URL::URL const&, bool activar);
    void suelta_búfer(u64 página, i32 id);
    void título_de(u64 vista, String);
    void enlace(Optional<URL::URL>);
    void sale(int código);
    Conexion& hub() { return *m_hub; }
    int ventana() const { return m_v; }

private:
    ErrorOr<void> prepara_recursos();
    ErrorOr<void> lanza_hub(Vector<ByteString> const& urls);
    void prepara_vista(Pestaña&, bool activa);
    Pestaña* activa() { return m_pestañas.is_empty() ? nullptr : m_pestañas[m_activa].ptr(); }
    void cambia_a(size_t);
    void cierra_pestaña(size_t);
    void pide_pintar();
    void pinta();
    void pinta_barra();
    void pinta_página();
    void atiende_sucesos();
    void atiende(nv_suceso const&);
    void tecla(nv_suceso const&);
    void tecla_en_barra(nv_suceso const&);
    void ratón(nv_suceso const&);
    void ve_a(String const&);
    void tamaño(int an, int al);
    Web::UIEvents::KeyModifier modificadores(int mods) const;

    RefPtr<Conexion> m_hub;
    Optional<Core::Process> m_proceso_hub;
    int m_v { 0 };
    int m_an { 0 };
    int m_al { 0 };
    Vector<u32> m_barra;                    // los puntos de la barra (m_an x ALTO_BARRA)
    Vector<u32> m_blanco;                   // lo que no cubre la página
    Vector<NonnullOwnPtr<Pestaña>> m_pestañas;
    size_t m_activa { 0 };
    bool m_barra_sucia { true };
    bool m_página_sucia { true };
    bool m_pintar_pedido { false };
    Core::AnonymousBuffer m_tema;
    // la barra de direcciones
    bool m_editando { false };
    Vector<u32> m_texto;                    // lo escrito (letras)
    size_t m_cursor { 0 };
    bool m_todo_marcado { false };
    // el ratón
    int m_botones { 0 };
    bool m_dentro_de_la_página { false };
    Optional<String> m_enlace;
    // los sucesos de la ventana: otro hilo espera y avisa por un tubo
    int m_tubo[2] { -1, -1 };
    sem_t m_drenado;
    RefPtr<Core::Notifier> m_aviso;
    u64 m_siguiente_entrada { 1 };
};

static Interfaz* s_interfaz;

// ---- la conexión: lo que cuenta el Hub ----

Conexion::Conexion(NonnullOwnPtr<IPC::Transport> transport, Interfaz& interfaz)
    : IPC::ConnectionToServer<HubClientEndpoint, HubServerEndpoint>(*this, move(transport))
    , m_interfaz(interfaz)
{
}

void Conexion::die()
{
    // sin el Hub no hay nada que enseñar
    if (m_inicializada)
        m_interfaz.sale(1);
}

void Conexion::view_install_backing_stores(u64 view_id, u64 page_id, Vector<i32> bitmap_ids, Vector<Gfx::SharedImage> backing_stores)
{
    auto* p = m_interfaz.pestaña(view_id);
    if (!p) {
        if (!bitmap_ids.is_empty())
            m_interfaz.suelta_búfer(page_id, bitmap_ids[0]);
        return;
    }
    auto delante = bitmap_ids.is_empty() ? -1 : bitmap_ids.first();
    if (auto r = p->búferes.install(move(bitmap_ids), move(backing_stores)); r.is_error()) {
        warnln("NAVEGADOR: no se pueden poner los búferes de la página {}: {}", page_id, r.error());
        m_interfaz.suelta_búfer(page_id, delante);
        return;
    }
    p->página_búferes = page_id;
}

void Conexion::view_add_backing_stores(u64 view_id, Vector<i32> bitmap_ids, Vector<Gfx::SharedImage> backing_stores)
{
    if (auto* p = m_interfaz.pestaña(view_id)) {
        if (auto r = p->búferes.add(move(bitmap_ids), move(backing_stores)); r.is_error())
            warnln("NAVEGADOR: no se pueden añadir búferes: {}", r.error());
    }
}

void Conexion::view_retire_backing_stores(u64 view_id, Vector<i32> bitmap_ids)
{
    if (auto* p = m_interfaz.pestaña(view_id))
        p->búferes.retire(bitmap_ids);
}

void Conexion::view_present_backing_store(u64 view_id, u64 page_id, i32 bitmap_id, Gfx::IntSize size, double device_pixel_ratio)
{
    auto* p = m_interfaz.pestaña(view_id);
    if (!p) {
        m_interfaz.suelta_búfer(page_id, bitmap_id);
        return;
    }
    // (el que deja de estar delante vuelve al Compositor, que pinta en él otra vez)
    auto r = p->búferes.present(bitmap_id, size, device_pixel_ratio);
    m_interfaz.suelta_búfer(page_id, r.bitmap_id_to_release);
}

void Conexion::view_accept_presented_backing_store(u64 view_id, i32, Gfx::IntRect)
{
    m_interfaz.ha_cambiado_la_página(view_id);
}

void Conexion::view_keep_front_backing_store_as_backup(u64 view_id)
{
    if (auto* p = m_interfaz.pestaña(view_id))
        p->búferes.keep_front_as_backup();
}

void Conexion::view_clear_backup_backing_store(u64 view_id)
{
    if (auto* p = m_interfaz.pestaña(view_id))
        p->búferes.clear_backup();
}

void Conexion::view_clear_backing_stores(u64 view_id)
{
    if (auto* p = m_interfaz.pestaña(view_id))
        p->búferes.clear();
}

void Conexion::view_state_changed(u64 view_id, Hub::ViewState state)
{
    auto* p = m_interfaz.pestaña(view_id);
    if (!p)
        return;
    p->cargando = state.is_loading;
    p->atrás = state.can_navigate_back;
    p->adelante = state.can_navigate_forward;
    if (!state.title.is_empty())
        m_interfaz.título_de(view_id, state.title.to_utf8());
    auto const& u = state.displayed_url.serialize().is_empty() ? state.url : state.displayed_url;
    if (!u.serialize().is_empty())
        p->dirección = u.serialize();
    m_interfaz.ha_cambiado_la_barra();
}

void Conexion::view_title_change(u64 view_id, Utf16String title)
{
    m_interfaz.título_de(view_id, title.to_utf8());
}

void Conexion::view_url_change(u64 view_id, URL::URL url)
{
    if (auto* p = m_interfaz.pestaña(view_id)) {
        p->dirección = url.serialize();
        m_interfaz.ha_cambiado_la_barra();
    }
}

void Conexion::view_loading_state_change(u64 view_id, bool is_loading)
{
    if (auto* p = m_interfaz.pestaña(view_id)) {
        p->cargando = is_loading;
        m_interfaz.ha_cambiado_la_barra();
    }
}

void Conexion::view_load_start(u64 view_id)
{
    if (auto* p = m_interfaz.pestaña(view_id)) {
        p->cargando = true;
        p->roto = false;
        m_interfaz.ha_cambiado_la_barra();
    }
}

void Conexion::view_load_finish(u64 view_id, URL::URL url)
{
    if (auto* p = m_interfaz.pestaña(view_id)) {
        p->cargando = false;
        // (para las pruebas: cuándo ha acabado de cargar cada página)
        outln("NAVEGADOR: cargada {}", url.serialize());
        m_interfaz.ha_cambiado_la_barra();
    }
}

void Conexion::view_cursor_change(u64 view_id, Gfx::Cursor cursor)
{
    auto* p = m_interfaz.pestaña(view_id);
    if (!p)
        return;
    p->cursor = cursor;
    auto texto = cursor.has<Gfx::StandardCursor>() && cursor.get<Gfx::StandardCursor>() == Gfx::StandardCursor::IBeam;
    nv_puntero(m_interfaz.ventana(), texto ? NV_P_TEXTO : NV_P_FLECHA);
}

void Conexion::view_link_hover(u64, URL::URL url)
{
    m_interfaz.enlace(url);
}

void Conexion::view_link_unhover(u64)
{
    m_interfaz.enlace({});
}

void Conexion::view_close(u64 view_id)
{
    m_interfaz.cierra_vista(view_id);
}

void Conexion::view_activate_tab(u64 view_id)
{
    m_interfaz.activa_vista(view_id);
}

void Conexion::view_open_new_view(u64, u64 new_view_id, Hub::ViewState, Web::HTML::ActivateTab activate_tab, Web::HTML::WebViewHints)
{
    m_interfaz.adopta_vista(new_view_id, activate_tab == Web::HTML::ActivateTab::Yes);
}

void Conexion::app_open_view_in_new_tab(u64 view_id, Optional<u64>, WebView::IsPrivate, Web::HTML::ActivateTab activate_tab)
{
    m_interfaz.adopta_vista(view_id, activate_tab == Web::HTML::ActivateTab::Yes);
}

void Conexion::app_open_url_in_new_tab(URL::URL url, Web::HTML::ActivateTab activate_tab, Optional<u64>, WebView::IsPrivate)
{
    m_interfaz.abre_pestaña(url, activate_tab == Web::HTML::ActivateTab::Yes);
}

void Conexion::app_open_urls_in_new_tabs(Vector<URL::URL> urls)
{
    for (auto& u : urls)
        m_interfaz.abre_pestaña(u, true);
}

void Conexion::app_open_new_tab(Vector<URL::URL> urls)
{
    if (urls.is_empty())
        m_interfaz.abre_pestaña(URL::Parser::basic_parse(StringView { INICIO, strlen(INICIO) }).value(), true);
    for (auto& u : urls)
        m_interfaz.abre_pestaña(u, true);
}

void Conexion::view_web_content_crashed(u64 view_id, WebView::ViewImplementation::WebContentCrashReason)
{
    if (auto* p = m_interfaz.pestaña(view_id)) {
        p->roto = true;
        p->cargando = false;
        warnln("NAVEGADOR: la página de la pestaña {} se ha cerrado (WebContent)", view_id);
        m_interfaz.ha_cambiado_la_barra();
    }
}

// (aún sin cuadros de diálogo: se contesta lo prudente, para que la página siga)
void Conexion::view_request_alert(u64 view_id, Utf16String message)
{
    warnln("NAVEGADOR: aviso de la página: {}", message);
    async_view_alert_closed(view_id);
}

void Conexion::view_request_confirm(u64 view_id, Utf16String)
{
    async_view_confirm_closed(view_id, false);
}

void Conexion::view_request_prompt(u64 view_id, Utf16String, Utf16String)
{
    async_view_prompt_closed(view_id, {});
}

void Conexion::view_request_before_unload(u64 view_id, u64 answer_id, String)
{
    async_view_answer(view_id, answer_id, true);
}

void Conexion::view_request_external_url_confirmation(u64 view_id, u64 answer_id, URL::URL, URL::Origin, String)
{
    async_view_answer(view_id, answer_id, false);
}

void Conexion::view_request_permission(u64 view_id, u64 answer_id, WebView::PermissionPromptRequest)
{
    async_view_answer_permission_request(view_id, answer_id, Web::Permissions::PromptOutcome::Dismissed);
}

void Conexion::view_request_select_dropdown(u64 view_id, Gfx::IntPoint, i32, Vector<Web::HTML::SelectItem>)
{
    async_view_select_dropdown_closed(view_id, {});
}

void Conexion::view_request_file_picker(u64 view_id, Web::HTML::FileFilter, Web::HTML::AllowMultipleFiles)
{
    async_view_file_picker_closed(view_id, {});
}

void Conexion::view_request_color_picker(u64 view_id, Color)
{
    async_view_color_picker_update(view_id, {}, Web::HTML::ColorPickerUpdateState::Closed);
}

void Conexion::view_request_date_picker(u64 view_id, Utf16String, Utf16String, Utf16String, Utf16String)
{
    async_view_date_picker_closed(view_id, {});
}

void Conexion::view_close_preflight_finished(u64, u64, bool)
{
}

void Conexion::app_set_clipboard_text(String text, WebView::Application::ClipboardType)
{
    nv_al_portapapeles(text.bytes_as_string_view().characters_without_null_termination(), text.bytes().size());
}

void Conexion::app_read_clipboard_text(u64 request_id, WebView::Application::ClipboardType)
{
    u32 n = 0;
    char* t = nv_del_portapapeles(&n);
    auto texto = t ? Utf16String::from_utf8_without_validation(StringView { t, n }) : Utf16String {};
    free(t);
    async_answer_clipboard_text(request_id, move(texto));
}

void Conexion::app_read_clipboard_item(u64 request_id)
{
    async_answer_clipboard_item(request_id, {});
}

void Conexion::app_ask_user_for_download_path(u64 request_id, ByteString file)
{
    // las descargas, a /tmp/web/descargas
    (void)Core::System::mkdir("/tmp/web/descargas"sv, 0755);
    async_answer_download_path(request_id, ByteString::formatted("/tmp/web/descargas/{}", file));
}

void Conexion::app_display_error_dialog(String message)
{
    warnln("NAVEGADOR: {}", message);
}

void Conexion::app_quit_for_signal()
{
    m_interfaz.sale(0);
}

// ---- la interfaz ----

Pestaña* Interfaz::pestaña(u64 vista)
{
    for (auto& p : m_pestañas) {
        if (p->vista == vista)
            return p.ptr();
    }
    return nullptr;
}

void Interfaz::suelta_búfer(u64 página, i32 id)
{
    if (id >= 0 && m_hub)
        m_hub->async_release_backing_store(página, id);
}

void Interfaz::ha_cambiado_la_página(u64 vista)
{
    if (auto* p = activa(); p && p->vista == vista) {
        m_página_sucia = true;
        pide_pintar();
    }
}

void Interfaz::título_de(u64 vista, String t)
{
    if (auto* p = pestaña(vista)) {
        // (para las pruebas: el título nuevo, por la salida)
        if (p->título != t)
            outln("NAVEGADOR: título «{}»", t);
        p->título = move(t);
        ha_cambiado_la_barra();
    }
}

void Interfaz::enlace(Optional<URL::URL> u)
{
    m_enlace = u.has_value() ? Optional<String>(u->serialize()) : Optional<String> {};
    m_página_sucia = true;
    pide_pintar();
}

void Interfaz::sale(int código)
{
    if (m_v)
        nv_cierra(m_v);
    m_v = 0;
    Core::EventLoop::current().quit(código);
}

ErrorOr<void> Interfaz::prepara_recursos()
{
    // lo de Ladybird (los recursos, las letras, las autoridades), en /tmp/web,
    // como lo deja MWEB (si no está ya, de otro NAVEGADOR o de MWEB)
    struct stat st;
    if (stat("/tmp/web/share/Lagom/themes/Default.ini", &st) != 0) {
        char const* av[] = { "/bin/MWEB", "--prepara", nullptr };
        pid_t pid;
        if (int e = posix_spawn(&pid, av[0], nullptr, nullptr, const_cast<char* const*>(av), environ); e != 0)
            return Error::from_errno(e);
        int s = 0;
        waitpid(pid, &s, 0);
        if (!WIFEXITED(s) || WEXITSTATUS(s) != 0)
            return Error::from_string_literal("MWEB --prepara no ha podido dejar los recursos en /tmp/web");
    }
    // (como MWEB: sin usuarios, la casa en /tmp/web)
    (void)mkdir("/tmp/web/casa", 0755);
    (void)mkdir("/tmp/web/run", 0700);
    setenv("XDG_DATA_DIRS", "/share", 1);
    setenv("HOME", "/tmp/web/casa", 1);
    setenv("XDG_CONFIG_HOME", "/tmp/web/casa/.config", 1);
    setenv("XDG_DATA_HOME", "/tmp/web/casa/.local/share", 1);
    setenv("XDG_CACHE_HOME", "/tmp/web/casa/.cache", 1);
    setenv("XDG_RUNTIME_DIR", "/tmp/web/run", 1);
    m_tema = TRY(Gfx::load_system_theme("/tmp/web/share/Lagom/themes/Default.ini"sv));
    return {};
}

ErrorOr<void> Interfaz::lanza_hub(Vector<ByteString> const& urls)
{
    // lo que la interfaz puede hacer (Hub::UIInfo), en hexadecimal, como la de Qt
    Hub::UIInfo info {};
    info.ui_font_family = "DejaVu Sans"_string;
    info.system_font_family = "DejaVu Sans"_string;
    info.interface_color_scheme = Web::CSS::PreferredColorScheme::Light;
    info.scrollbars_always_shown = true;
    info.reports_scroll_momentum = false;
    info.preferred_languages = { "es-ES"_string, "es"_string };
    IPC::MessageBuffer b;
    IPC::Encoder codificador { b };
    TRY(codificador.encode(info));

    Vector<ByteString> args;
    // (sin la caja de arena de Linux: seccomp y Landlock, que Hispa-64 no tiene)
    args.append("--disable-sandbox");
    args.append("--certificate=/tmp/web/share/autoridades.pem");
    args.append(ByteString::formatted("--ui-info={}", encode_hex(b.data().span())));
    args.append(ByteString::formatted("--ui-pid={}", getpid()));
    for (auto& u : urls)
        args.append(u);

    int fds[2] {};
    TRY(Core::System::socketpair(AF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0, fds));
    Core::ProcessSpawnOptions op {
        .name = "Hub"sv,
        .executable = "/bin/Hub",
        .arguments = args,
    };
    op.file_actions.append(Core::FileAction::DupFd { .write_fd = fds[1], .fd = fds[1] });
    TRY(Core::Environment::set("SOCKET_TAKEOVER"sv, ByteString::formatted("ipc:{}", fds[1]), Core::Environment::Overwrite::Yes));
    m_proceso_hub = TRY(Core::Process::spawn(op));
    TRY(Core::Environment::unset("SOCKET_TAKEOVER"sv));
    TRY(Core::System::close(fds[1]));
    m_hub = adopt_ref(*new Conexion(TRY(IPC::Transport::adopt_fd(fds[0])), *this));
    return {};
}

void Interfaz::prepara_vista(Pestaña& p, bool es_activa)
{
    auto& h = *m_hub;
    h.async_view_set_presents_from_cpu_memory(p.vista, true);
    h.async_view_set_viewport(p.vista, { m_an, max(1, m_al - ALTO_BARRA) }, 1.0);
    h.async_view_set_current_tab_of_window(p.vista, es_activa);
    h.async_view_set_system_visibility_state(p.vista, es_activa ? Web::HTML::VisibilityState::Visible : Web::HTML::VisibilityState::Hidden);
    h.async_view_set_has_system_focus(p.vista, es_activa && !m_editando);
}

void Interfaz::adopta_vista(u64 vista, bool activar)
{
    if (pestaña(vista))
        return;
    auto p = make<Pestaña>();
    p->vista = vista;
    p->título = "Pestaña nueva"_string;
    auto* q = p.ptr();
    m_pestañas.append(move(p));
    prepara_vista(*q, false);
    if (activar)
        cambia_a(m_pestañas.size() - 1);
    ha_cambiado_la_barra();
}

void Interfaz::abre_pestaña(URL::URL const& url, bool activar)
{
    auto r = m_hub->create_view({
        .is_private = WebView::IsPrivate::No,
        .device_pixel_ratio = 1.0,
        .maximum_frames_per_second = 60.0,
        .display_id = {},
        .screen_shows_high_dynamic_range = false,
        .system_theme = m_tema,
        .screen_rects = {},
        .owner_view_id = {},
        .websites_prefer_dark = false,
        .is_tab = true,
    });
    auto p = make<Pestaña>();
    p->vista = r.view_id();
    p->título = "Cargando…"_string;
    p->dirección = url.serialize();
    auto* q = p.ptr();
    m_pestañas.append(move(p));
    prepara_vista(*q, false);
    m_hub->async_view_load(q->vista, url, Web::Bindings::NavigationHistoryBehavior::Auto);
    if (activar || m_pestañas.size() == 1)
        cambia_a(m_pestañas.size() - 1);
    ha_cambiado_la_barra();
}

void Interfaz::cambia_a(size_t i)
{
    if (i >= m_pestañas.size())
        return;
    if (auto* a = activa(); a && m_activa != i) {
        m_hub->async_view_set_current_tab_of_window(a->vista, false);
        m_hub->async_view_set_system_visibility_state(a->vista, Web::HTML::VisibilityState::Hidden);
        m_hub->async_view_set_has_system_focus(a->vista, false);
    }
    m_activa = i;
    auto& p = *m_pestañas[i];
    m_hub->async_active_view_changed(p.vista);
    prepara_vista(p, true);
    m_editando = false;
    m_página_sucia = true;
    ha_cambiado_la_barra();
}

void Interfaz::cierra_pestaña(size_t i)
{
    if (i >= m_pestañas.size())
        return;
    auto vista = m_pestañas[i]->vista;
    m_pestañas.remove(i);
    m_hub->async_destroy_view(vista);
    if (m_pestañas.is_empty()) {
        sale(0);
        return;
    }
    if (m_activa >= m_pestañas.size())
        m_activa = m_pestañas.size() - 1;
    else if (m_activa > i)
        m_activa--;
    // (la que queda activa, encendida: la cerrada ya no está)
    auto a = m_activa;
    m_activa = a + 1 < m_pestañas.size() ? a + 1 : (a > 0 ? a - 1 : a);
    if (m_activa == a)
        prepara_vista(*m_pestañas[a], true);
    cambia_a(a);
}

void Interfaz::cierra_vista(u64 vista)
{
    for (size_t i = 0; i < m_pestañas.size(); i++) {
        if (m_pestañas[i]->vista == vista) {
            cierra_pestaña(i);
            return;
        }
    }
}

void Interfaz::activa_vista(u64 vista)
{
    for (size_t i = 0; i < m_pestañas.size(); i++) {
        if (m_pestañas[i]->vista == vista)
            cambia_a(i);
    }
}

void Interfaz::pide_pintar()
{
    if (m_pintar_pedido)
        return;
    m_pintar_pedido = true;
    Core::deferred_invoke([this] {
        m_pintar_pedido = false;
        pinta();
    });
}

void Interfaz::pinta()
{
    if (!m_v)
        return;
    if (m_barra_sucia)
        pinta_barra();
    if (m_página_sucia)
        pinta_página();
    m_barra_sucia = false;
    m_página_sucia = false;
}

// un rectángulo en la barra
static void rect(Vector<u32>& px, int an, int x, int y, int w, int h, u32 c)
{
    for (int f = max(0, y); f < min(ALTO_BARRA, y + h); f++) {
        for (int k = max(0, x); k < min(an, x + w); k++)
            px[f * an + k] = c;
    }
}

// ↻ (la DejaVu recortada de la oficina no la tiene): un anillo de radio 7
// abierto arriba a la derecha y la punta de la flecha, con suavizado
static void icono_recarga(Vector<u32>& px, int an, int cx, int cy, u32 c)
{
    auto mezcla = [&](int x, int y, double a) {
        if (x < 0 || x >= an || y < 0 || y >= ALTO_BARRA || a <= 0)
            return;
        a = min(a, 1.0);
        auto& d = px[y * an + x];
        u32 r = 0;
        for (int k = 0; k < 24; k += 8) {
            double o = (d >> k) & 0xFF, t = (c >> k) & 0xFF;
            r |= (u32)(o + (t - o) * a + 0.5) << k;
        }
        d = r;
    };
    for (int y = -10; y <= 10; y++) {
        for (int x = -10; x <= 10; x++) {
            double dist = __builtin_sqrt((double)(x * x + y * y));
            double ang = __builtin_atan2((double)-y, (double)x) * 180 / 3.14159265;
            if (ang > 15 && ang < 70)
                continue;
            double a = 1.0 - __builtin_fabs(dist - 6.5) / 1.2;
            mezcla(cx + x, cy + y, a);
        }
    }
    // la punta: un triángulo en el final del anillo (arriba a la derecha)
    for (int y = 0; y < 7; y++) {
        for (int x = 0; x <= y; x++)
            mezcla(cx + 3 + x, cy - 9 + y, 1.0);
    }
}

static ByteString recorta(StringView t, int ancho)
{
    // el texto, que quepa en «ancho» (con «…» si no)
    if (nv_ancho(ByteString(t).characters(), 0, TAM) <= ancho)
        return t;
    Utf8View u(t);
    Vector<u32> letras;
    for (auto c : u)
        letras.append(c);
    while (!letras.is_empty()) {
        letras.take_last();
        StringBuilder sb;
        for (auto c : letras)
            sb.append_code_point(c);
        sb.append("…"sv);
        auto s = sb.to_byte_string();
        if (nv_ancho(s.characters(), 0, TAM) <= ancho)
            return s;
    }
    return "…";
}

void Interfaz::pinta_barra()
{
    auto an = m_an;
    if (an <= 0)
        return;
    auto& px = m_barra;
    rect(px, an, 0, 0, an, ALTO_BARRA, C_FONDO);
    // las pestañas
    int x = 6;
    int ancho_p = m_pestañas.is_empty() ? ANCHO_PESTAÑA : min(ANCHO_PESTAÑA, (an - 50) / (int)m_pestañas.size());
    for (size_t i = 0; i < m_pestañas.size(); i++) {
        auto& p = *m_pestañas[i];
        auto es = i == m_activa;
        rect(px, an, x, 5, ancho_p - 4, ALTO_PESTAÑAS - 5, es ? C_PESTAÑA : C_PESTAÑA_OTRA);
        auto t = p.título.is_empty() ? p.dirección : p.título;
        auto texto = ByteString::formatted("{}{}", p.cargando ? "◌ "sv : ""sv, p.roto ? "(cerrada) "sv : ""sv);
        texto = ByteString::formatted("{}{}", texto, recorta(t.bytes_as_string_view(), ancho_p - 52));
        nv_texto(px.data(), an, ALTO_BARRA, an, x + 8, 23, texto.characters(), es ? C_TEXTO : 0x3A4048, es, TAM);
        nv_texto(px.data(), an, ALTO_BARRA, an, x + ancho_p - 22, 23, "×", C_GRIS, 0, TAM + 8);
        x += ancho_p;
    }
    nv_texto(px.data(), an, ALTO_BARRA, an, x + 8, 24, "+", C_TEXTO, 1, TAM + 12);
    // las herramientas: ← → ↻ y la dirección
    auto* p = activa();
    int y0 = ALTO_PESTAÑAS;
    rect(px, an, 0, y0, an, ALTO_HERRAMIENTAS, C_PESTAÑA);
    rect(px, an, 0, ALTO_BARRA - 1, an, 1, C_BORDE);
    char const* botones[] = { "←", "→", "✕" };
    bool vivos[] = { p && p->atrás, p && p->adelante, p != nullptr };
    for (int b = 0; b < 3; b++) {
        if (b == 2 && !(p && p->cargando))
            icono_recarga(px, an, 8 + b * ANCHO_BOTÓN + 14, y0 + 19, vivos[b] ? C_TEXTO : C_GRIS);
        else
            nv_texto(px.data(), an, ALTO_BARRA, an, 8 + b * ANCHO_BOTÓN + 6, y0 + 26, botones[b], vivos[b] ? C_TEXTO : C_GRIS, 1, TAM + 16);
    }
    int cx = 8 + 3 * ANCHO_BOTÓN + 8, cw = an - cx - 10;
    rect(px, an, cx, y0 + 5, cw, ALTO_HERRAMIENTAS - 10, m_editando ? C_FOCO : C_BORDE);
    rect(px, an, cx + 1, y0 + 6, cw - 2, ALTO_HERRAMIENTAS - 12, C_CAMPO);
    if (m_editando) {
        StringBuilder sb;
        for (size_t i = 0; i < m_cursor; i++)
            sb.append_code_point(m_texto[i]);
        auto antes = sb.to_byte_string();
        for (size_t i = m_cursor; i < m_texto.size(); i++)
            sb.append_code_point(m_texto[i]);
        auto todo = sb.to_byte_string();
        if (m_todo_marcado)
            rect(px, an, cx + 8, y0 + 9, min(cw - 16, nv_ancho(todo.characters(), 0, TAM)), ALTO_HERRAMIENTAS - 18, C_SELECCIÓN);
        nv_texto(px.data(), an, ALTO_BARRA, an, cx + 8, y0 + 24, todo.characters(), C_TEXTO, 0, TAM);
        int k = cx + 8 + nv_ancho(antes.characters(), 0, TAM);
        rect(px, an, k, y0 + 9, 1, ALTO_HERRAMIENTAS - 18, C_TEXTO);
    } else if (p) {
        nv_texto(px.data(), an, ALTO_BARRA, an, cx + 8, y0 + 24, recorta(p->dirección.bytes_as_string_view(), cw - 16).characters(), C_TEXTO, 0, TAM);
    }
    nv_mapa(m_v, 0, 0, an, ALTO_BARRA, px.data(), an);
    // el título de la ventana
    if (p) {
        auto t = ByteString::formatted("{} — Navegador", p->título.is_empty() ? p->dirección : p->título);
        nv_titulo(m_v, t.characters());
    }
}

void Interfaz::pinta_página()
{
    int alto = m_al - ALTO_BARRA;
    if (alto <= 0 || m_an <= 0)
        return;
    auto* p = activa();
    int hecho_an = 0, hecho_al = 0;
    if (p) {
        if (auto f = p->búferes.displayed_frame(); f.has_value() && f->shared_image_buffer) {
            if (auto bm = f->shared_image_buffer->bitmap_if_present()) {
                hecho_an = min(bm->width(), m_an);
                hecho_al = min(bm->height(), alto);
                auto paso = (int)(bm->pitch() / 4);
                if (bm->format() == Gfx::BitmapFormat::BGRA8888 || bm->format() == Gfx::BitmapFormat::BGRx8888) {
                    nv_mapa(m_v, 0, ALTO_BARRA, hecho_an, hecho_al, reinterpret_cast<u32 const*>(bm->scanline(0)), paso);
                } else {
                    // (RGBA: a 0x00RRGGBB, fila a fila)
                    Vector<u32> fila;
                    fila.resize(hecho_an);
                    for (int y = 0; y < hecho_al; y++) {
                        auto const* o = reinterpret_cast<u32 const*>(bm->scanline(y));
                        for (int x = 0; x < hecho_an; x++) {
                            auto c = o[x];
                            fila[x] = (c & 0xFF) << 16 | (c & 0xFF00) | (c >> 16 & 0xFF);
                        }
                        nv_mapa(m_v, 0, ALTO_BARRA + y, hecho_an, 1, fila.data(), hecho_an);
                    }
                }
            }
        }
    }
    // lo que no cubre la página (al crecer la ventana, hasta el siguiente cuadro), en blanco
    if (m_blanco.size() < (size_t)m_an)
        m_blanco.resize(m_an);
    for (auto& c : m_blanco)
        c = 0xFFFFFF;
    if (hecho_an < m_an)
        nv_mapa(m_v, hecho_an, ALTO_BARRA, m_an - hecho_an, alto, m_blanco.data(), 0);
    if (hecho_al < alto)
        nv_mapa(m_v, 0, ALTO_BARRA + hecho_al, hecho_an, alto - hecho_al, m_blanco.data(), 0);
    // el enlace bajo el ratón, abajo a la izquierda
    if (m_enlace.has_value()) {
        auto t = recorta(m_enlace->bytes_as_string_view(), m_an / 2);
        int w = nv_ancho(t.characters(), 0, TAM) + 12, h = 20;
        Vector<u32> caja;
        caja.resize(w * h);
        for (auto& c : caja)
            c = 0xF2F3F5;
        for (int k = 0; k < w; k++)
            caja[k] = C_BORDE;
        nv_texto(caja.data(), w, h, w, 6, 15, t.characters(), C_TEXTO, 0, TAM);
        nv_mapa(m_v, 0, m_al - h, min(w, m_an), h, caja.data(), w);
    }
}

void Interfaz::tamaño(int an, int al)
{
    if (an <= 0 || al <= 0)
        return;
    m_an = an;
    m_al = al;
    m_barra.resize(an * ALTO_BARRA);
    for (auto& p : m_pestañas)
        m_hub->async_view_set_viewport(p->vista, { an, max(1, al - ALTO_BARRA) }, 1.0);
    m_barra_sucia = true;
    m_página_sucia = true;
    pide_pintar();
}

Web::UIEvents::KeyModifier Interfaz::modificadores(int mods) const
{
    unsigned m = Web::UIEvents::Mod_None;
    if (mods & NV_CON_MAYUS)
        m |= Web::UIEvents::Mod_Shift;
    if ((mods & NV_CON_CONTROL) || nv_control())
        m |= Web::UIEvents::Mod_Ctrl;
    return static_cast<Web::UIEvents::KeyModifier>(m);
}

void Interfaz::ve_a(String const& texto)
{
    auto* p = activa();
    if (!p)
        return;
    auto t = texto.bytes_as_string_view().trim_whitespace();
    if (t.is_empty())
        return;
    Optional<URL::URL> u = WebView::sanitize_url(t);
    if (!u.has_value() && !t.contains(' ') && t.contains('.'))
        u = URL::Parser::basic_parse(ByteString::formatted("https://{}", t));
    if (!u.has_value()) {
        // (sin buscador: lo que no es una dirección, a DuckDuckGo)
        StringBuilder sb;
        sb.append("https://duckduckgo.com/html/?q="sv);
        sb.append(URL::percent_encode(t, URL::PercentEncodeSet::Query, URL::SpaceAsPlus::Yes));
        u = URL::Parser::basic_parse(sb.string_view());
    }
    if (!u.has_value())
        return;
    p->dirección = u->serialize();
    m_hub->async_view_load(p->vista, *u, Web::Bindings::NavigationHistoryBehavior::Auto);
    m_editando = false;
    m_hub->async_view_set_has_system_focus(p->vista, true);
    ha_cambiado_la_barra();
}

static Web::UIEvents::KeyCode tecla_de(u32 t)
{
    using namespace Web::UIEvents;
    switch (t) {
    case NV_T_ARRIBA: return KeyCode::Key_Up;
    case NV_T_ABAJO: return KeyCode::Key_Down;
    case NV_T_IZQUIERDA: return KeyCode::Key_Left;
    case NV_T_DERECHA: return KeyCode::Key_Right;
    case NV_T_INICIO: return KeyCode::Key_Home;
    case NV_T_FIN: return KeyCode::Key_End;
    case NV_T_RE_PAG: return KeyCode::Key_PageUp;
    case NV_T_AV_PAG: return KeyCode::Key_PageDown;
    case NV_T_SUPR: return KeyCode::Key_Delete;
    case NV_T_INSERTAR: return KeyCode::Key_Insert;
    case NV_T_MAYUS_TAB: return KeyCode::Key_Tab;
    case 8: return KeyCode::Key_Backspace;
    case 9: return KeyCode::Key_Tab;
    case 10: case 13: return KeyCode::Key_Return;
    case 27: return KeyCode::Key_Escape;
    default: break;
    }
    if (t >= NV_T_F1 && t <= NV_T_F12)
        return static_cast<KeyCode>(to_underlying(KeyCode::Key_F1) + (t - NV_T_F1));
    if (t < NV_T_BASE)
        return code_point_to_key_code(t);
    return KeyCode::Key_Invalid;
}

void Interfaz::tecla_en_barra(nv_suceso const& s)
{
    auto t = s.tecla;
    if (t == 10 || t == 13) {
        StringBuilder sb;
        for (auto c : m_texto)
            sb.append_code_point(c);
        ve_a(MUST(sb.to_string()));
        return;
    }
    if (t == 27) {
        m_editando = false;
        if (auto* p = activa())
            m_hub->async_view_set_has_system_focus(p->vista, true);
        ha_cambiado_la_barra();
        return;
    }
    if (m_todo_marcado && (t == 8 || t == NV_T_SUPR || (t >= 32 && t < NV_T_BASE && !(s.mods & NV_CON_CONTROL)))) {
        m_texto.clear();
        m_cursor = 0;
        m_todo_marcado = false;
        if (t == 8 || t == NV_T_SUPR) {
            ha_cambiado_la_barra();
            return;
        }
    }
    m_todo_marcado = false;
    if (t == 8) {
        if (m_cursor > 0)
            m_texto.remove(--m_cursor);
    } else if (t == NV_T_SUPR) {
        if (m_cursor < m_texto.size())
            m_texto.remove(m_cursor);
    } else if (t == NV_T_IZQUIERDA) {
        if (m_cursor > 0)
            m_cursor--;
    } else if (t == NV_T_DERECHA) {
        if (m_cursor < m_texto.size())
            m_cursor++;
    } else if (t == NV_T_INICIO) {
        m_cursor = 0;
    } else if (t == NV_T_FIN) {
        m_cursor = m_texto.size();
    } else if (t == 1 && (s.mods & NV_CON_CONTROL)) {
        m_todo_marcado = true;          // Ctrl+A
    } else if (t == 22 && (s.mods & NV_CON_CONTROL)) {
        u32 n = 0;                      // Ctrl+V
        if (char* d = nv_del_portapapeles(&n)) {
            for (auto c : Utf8View(StringView { d, n })) {
                if (c >= 32)
                    m_texto.insert(m_cursor++, c);
            }
            free(d);
        }
    } else if (t >= 32 && t < NV_T_BASE && !(s.mods & NV_CON_CONTROL)) {
        m_texto.insert(m_cursor++, t);
    }
    ha_cambiado_la_barra();
}

void Interfaz::tecla(nv_suceso const& s)
{
    auto t = s.tecla;
    auto ctrl = (s.mods & NV_CON_CONTROL) != 0;
    auto* p = activa();
    // lo del navegador
    if (ctrl && (t == 12 || t == 'l')) {           // Ctrl+L: la barra de direcciones
        m_editando = true;
        m_texto.clear();
        if (p) {
            for (auto c : Utf8View(p->dirección.bytes_as_string_view()))
                m_texto.append(c);
            m_hub->async_view_set_has_system_focus(p->vista, false);
        }
        m_cursor = m_texto.size();
        m_todo_marcado = true;
        ha_cambiado_la_barra();
        return;
    }
    if (ctrl && (t == 20 || t == 't')) {           // Ctrl+T: una pestaña en blanco, con la barra para escribir
        abre_pestaña(URL::about_blank(), true);
        m_editando = true;
        m_texto.clear();
        m_cursor = 0;
        m_todo_marcado = false;
        ha_cambiado_la_barra();
        return;
    }
    if (ctrl && (t == 23 || t == 'w')) {           // Ctrl+W
        cierra_pestaña(m_activa);
        return;
    }
    if (t == NV_T_CTRL_TAB) {
        if (!m_pestañas.is_empty())
            cambia_a((m_activa + 1) % m_pestañas.size());
        return;
    }
    if (p && (t == NV_T_F5 || (ctrl && (t == 18 || t == 'r')))) {
        m_hub->async_view_reload(p->vista, WebView::BypassCache::No);
        return;
    }
    if (m_editando) {
        tecla_en_barra(s);
        return;
    }
    if (!p)
        return;
    // a la página: KeyDown y KeyUp (las teclas de Hispa-64 llegan al pulsarlas)
    u32 letra = t;
    Web::UIEvents::KeyCode código;
    auto mods = modificadores(s.mods);
    if (ctrl && t >= 1 && t <= 26) {
        letra = 'a' + t - 1;
        código = Web::UIEvents::code_point_to_key_code(letra);
    } else {
        código = tecla_de(t);
    }
    if (t == NV_T_MAYUS_TAB)
        mods = static_cast<Web::UIEvents::KeyModifier>(mods | Web::UIEvents::Mod_Shift);
    if (código == Web::UIEvents::KeyCode::Key_Invalid && (t >= NV_T_BASE || t < 32))
        return;
    u32 cp = 0;
    bool inserta = false;
    if (t == 10 || t == 13) {
        cp = '\n';
        inserta = true;
    } else if (t == 9) {
        cp = '\t';
    } else if (t == 8 || t == 27 || t >= NV_T_BASE) {
        cp = 0;
    } else {
        cp = letra;
        inserta = !ctrl && letra >= 32;
    }
    for (auto tipo : { Web::KeyEvent::Type::KeyDown, Web::KeyEvent::Type::KeyUp }) {
        Web::KeyEvent e {};
        e.type = tipo;
        e.key = código;
        e.modifiers = mods;
        e.code_point = cp;
        e.should_insert_text = tipo == Web::KeyEvent::Type::KeyDown && inserta;
        e.id = m_siguiente_entrada;
        m_hub->async_view_enqueue_key_event(p->vista, m_siguiente_entrada++, e.clone_without_browser_data());
    }
}

static Web::UIEvents::MouseButton botones_de(int b)
{
    unsigned r = Web::UIEvents::MouseButton::None;
    if (b & NV_BOTON_IZQ)
        r |= Web::UIEvents::MouseButton::Primary;
    if (b & NV_BOTON_DER)
        r |= Web::UIEvents::MouseButton::Secondary;
    if (b & NV_BOTON_CEN)
        r |= Web::UIEvents::MouseButton::Middle;
    return static_cast<Web::UIEvents::MouseButton>(r);
}

void Interfaz::ratón(nv_suceso const& s)
{
    auto* p = activa();
    auto antes = m_botones;
    if (s.clase == NV_BAJA || s.clase == NV_SUBE || s.clase == NV_MUEVE)
        m_botones = s.botones;
    // en la barra
    if (s.y < ALTO_BARRA && s.clase != NV_RUEDA) {
        if (m_dentro_de_la_página && p) {
            Web::MouseEvent e {};
            e.type = Web::MouseEvent::Type::MouseLeave;
            m_hub->async_view_enqueue_mouse_event(p->vista, m_siguiente_entrada++, e.clone_without_browser_data());
            m_dentro_de_la_página = false;
            nv_puntero(m_v, NV_P_FLECHA);
        }
        if (s.clase != NV_BAJA || !(s.botones & NV_BOTON_IZQ & ~antes))
            return;
        if (s.y < ALTO_PESTAÑAS) {
            int ancho_p = m_pestañas.is_empty() ? ANCHO_PESTAÑA : min(ANCHO_PESTAÑA, (m_an - 50) / (int)m_pestañas.size());
            int i = (s.x - 6) / ancho_p;
            if (s.x >= 6 && i >= 0 && (size_t)i < m_pestañas.size()) {
                if (s.x >= 6 + i * ancho_p + ancho_p - 26)
                    cierra_pestaña(i);
                else
                    cambia_a(i);
            } else if (s.x >= 6 + (int)m_pestañas.size() * ancho_p && s.x < 6 + (int)m_pestañas.size() * ancho_p + 30) {
                abre_pestaña(URL::Parser::basic_parse(StringView { INICIO, strlen(INICIO) }).value(), true);
            }
            return;
        }
        int b = (s.x - 8) / ANCHO_BOTÓN;
        if (s.x >= 8 && b >= 0 && b < 3) {
            if (!p)
                return;
            if (b == 0 && p->atrás)
                m_hub->async_view_traverse_the_history_by_delta(p->vista, -1);
            else if (b == 1 && p->adelante)
                m_hub->async_view_traverse_the_history_by_delta(p->vista, 1);
            else if (b == 2 && p->cargando)
                m_hub->async_view_stop_loading(p->vista);
            else if (b == 2)
                m_hub->async_view_reload(p->vista, WebView::BypassCache::No);
            return;
        }
        if (s.x >= 8 + 3 * ANCHO_BOTÓN) {
            nv_suceso l {};
            l.clase = NV_TECLA;
            l.tecla = 12;
            l.mods = NV_CON_CONTROL;
            tecla(l);
        }
        return;
    }
    if (!p)
        return;
    if (m_editando && s.clase == NV_BAJA) {
        m_editando = false;
        m_hub->async_view_set_has_system_focus(p->vista, true);
        ha_cambiado_la_barra();
    }
    m_dentro_de_la_página = true;
    Web::MouseEvent e {};
    e.position = { s.x, s.y - ALTO_BARRA };
    e.screen_position = { s.x, s.y };
    e.buttons = botones_de(m_botones);
    e.modifiers = modificadores(s.mods);
    if (s.clase == NV_RUEDA) {
        e.type = Web::MouseEvent::Type::MouseWheel;
        // (un paso de la rueda: tres líneas de 40 puntos, como Ladybird)
        double d = -s.pasos * 120.0;
        if (s.mods & NV_CON_MAYUS) {
            e.wheel_delta_x = d;
            e.wheel_tick_120ths_x = -s.pasos * 120.0;
        } else {
            e.wheel_delta_y = d;
            e.wheel_tick_120ths_y = -s.pasos * 120.0;
        }
        e.wheel_delta_precision = Web::WheelDeltaPrecision::Discrete;
    } else if (s.clase == NV_MUEVE) {
        e.type = Web::MouseEvent::Type::MouseMove;
    } else {
        e.type = s.clase == NV_BAJA ? Web::MouseEvent::Type::MouseDown : Web::MouseEvent::Type::MouseUp;
        auto cambio = s.clase == NV_BAJA ? (s.botones & ~antes) : (antes & ~s.botones);
        if (!cambio)
            cambio = NV_BOTON_IZQ;
        e.button = botones_de(cambio & -cambio);
        e.click_count = max(1, s.veces);
    }
    m_hub->async_view_enqueue_mouse_event(p->vista, m_siguiente_entrada++, e.clone_without_browser_data());
}

void Interfaz::atiende(nv_suceso const& s)
{
    switch (s.clase) {
    case NV_TECLA:
        tecla(s);
        break;
    case NV_BAJA:
    case NV_SUBE:
    case NV_MUEVE:
    case NV_RUEDA:
        ratón(s);
        break;
    case NV_TAMANO:
        tamaño(s.x, s.y);
        break;
    case NV_FOCO:
        if (auto* p = activa())
            m_hub->async_view_set_has_system_focus(p->vista, s.foco && !m_editando);
        break;
    case NV_CERRAR:
    case NV_SALE:
        sale(0);
        break;
    default:
        break;
    }
}

void Interfaz::atiende_sucesos()
{
    char b[64];
    while (read(m_tubo[0], b, sizeof b) == (ssize_t)sizeof b)
        ;
    nv_suceso s;
    while (m_v && nv_suceso_de(m_v, &s))
        atiende(s);
    sem_post(&m_drenado);
}

// (el hilo que espera los sucesos de la ventana: escribe en el tubo y espera
// a que el hilo principal los haya atendido todos)
static int g_tubo_escribir = -1;
static sem_t* g_drenado = nullptr;

static void* espera_sucesos(void* a)
{
    auto& i = *static_cast<Interfaz*>(a);
    for (;;) {
        int v = i.ventana();
        if (!v)
            return nullptr;
        if (nv_espera(v, 500)) {
            char c = 1;
            if (write(g_tubo_escribir, &c, 1) < 0 && errno != EAGAIN)
                return nullptr;
            sem_wait(g_drenado);
        }
    }
}

ErrorOr<int> Interfaz::empieza(Main::Arguments argumentos)
{
    s_interfaz = this;
    setvbuf(stdout, nullptr, _IOLBF, 0);
    outln("NAVEGADOR: Ladybird con ventana (la interfaz de Hispa-64 sobre el Hub de Ladybird)");
    if (!nv_hay_escritorio()) {
        warnln("NAVEGADOR: no está el escritorio (VENTANAS): sin ventana, MWEB");
        return 1;
    }
    Core::EventLoop bucle;
    TRY(prepara_recursos());
    Vector<ByteString> urls;
    for (size_t i = 1; i < argumentos.strings.size(); i++)
        urls.append(argumentos.strings[i]);
    TRY(lanza_hub({}));
    auto inicio = m_hub->try_initialize();
    if (inicio.is_error()) {
        warnln("NAVEGADOR: el Hub no contesta");
        return 1;
    }
    m_hub->inicializada();
    nv_letras();
    int an = 0, al = 0;
    nv_pantalla(&an, &al);
    m_v = nv_abre("Navegador", an * 9 / 10, al * 9 / 10 - 30);
    if (!m_v) {
        warnln("NAVEGADOR: no se puede abrir la ventana");
        return 1;
    }
    nv_maximiza(m_v);
    tamaño(an * 9 / 10, al * 9 / 10 - 30);
    // los sucesos de la ventana: otro hilo los espera y avisa por el tubo
    if (::pipe2(m_tubo, O_CLOEXEC | O_NONBLOCK) < 0)
        return Error::from_errno(errno);
    g_tubo_escribir = m_tubo[1];
    sem_init(&m_drenado, 0, 0);
    g_drenado = &m_drenado;
    m_aviso = Core::Notifier::construct(m_tubo[0], Core::Notifier::Type::Read);
    m_aviso->on_activation = [this] { atiende_sucesos(); };
    pthread_t hilo;
    pthread_create(&hilo, nullptr, espera_sucesos, this);
    pthread_detach(hilo);
    m_hub->async_ui_did_start();
    auto estado = inicio.release_value();
    // (lo que abre el Hub al empezar; sin «about:welcome», la bienvenida de
    // Ladybird en inglés: en Hispa-64 el perfil está en /tmp/web, y saldría
    // cada vez)
    if (urls.is_empty()) {
        for (auto& u : estado.urls) {
            if (u.serialize() != "about:welcome"sv)
                urls.append(u.serialize().to_byte_string());
        }
    }
    if (urls.is_empty())
        urls.append(INICIO);
    for (auto& u : urls) {
        auto url = WebView::sanitize_url(u);
        if (url.has_value())
            abre_pestaña(*url, false);
    }
    if (m_pestañas.is_empty())
        abre_pestaña(URL::Parser::basic_parse(StringView { INICIO, strlen(INICIO) }).value(), true);
    cambia_a(0);
    auto r = bucle.exec();
    if (m_proceso_hub.has_value())
        kill(m_proceso_hub->pid(), SIGTERM);
    return r;
}

}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    signal(SIGPIPE, SIG_IGN);
    static Navegador::Interfaz interfaz;
    return interfaz.empieza(arguments);
}
