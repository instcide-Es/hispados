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
 *     su barra: las pestañas, ← → ↻ ⌂, la barra de direcciones, ☆ (los
 *     marcadores) y ☰ (el menú), con la letra de la oficina (la DejaVu Sans);
 *   - le pasa al Hub el teclado (con los acentos y la ñ), el ratón y la
 *     rueda; Ctrl+L, la barra; Ctrl+T, otra pestaña; Ctrl+W, cerrarla;
 *     Ctrl+Tab, la siguiente; F5 o Ctrl+R, recargar;
 *   - el tamaño de la ventana es el de la página (view_set_viewport).
 * [etapa 10 i] (sección 26) Para el uso de cada día, todo en español:
 *   - los cuadros de las páginas (alert, confirm, prompt, «¿salir de la
 *     página?»), dentro de la ventana, con los de la oficina (ventana.c, ui.c);
 *     elegir un archivo para subirlo (<input type=file>), con el selector de
 *     archivos de la oficina; las listas <select>, con un menú del escritorio;
 *   - las descargas, a la carpeta Descargas (de la casa: /tmp/web/casa), con un
 *     aviso abajo al acabar (y el menú Descargas);
 *   - el menú del botón derecho (abrir el enlace en una pestaña nueva, copiar
 *     el enlace, copiar, cortar, pegar, seleccionar todo, guardar la imagen,
 *     buscar lo elegido, atrás, adelante, recargar, añadir a los marcadores);
 *   - el zoom (Ctrl + y Ctrl -, Ctrl 0, Ctrl y la rueda), buscar en la página
 *     (Ctrl+F: una barra abajo, con «N de M», ▲ ▼; Intro, F3, Esc), los
 *     marcadores (Ctrl+D pone o quita la página; ☆ y Ctrl+B, la lista) y el
 *     historial (Ctrl+H; lo visitado, de más nuevo a más viejo), guardados en
 *     la casa (marcadores.txt, historial.txt);
 *   - la página de inicio propia (⌂, «+» y Ctrl+T): inicio.html, en español,
 *     con el buscador, los accesos, los marcadores y lo último visitado;
 *   - Ctrl+C, Ctrl+X, Ctrl+V, Ctrl+A, Ctrl+Z y Ctrl+Y, como la interfaz de Qt.
 * Uso: NAVEGADOR [URL]   (sin URL, https://hispados.es/)
 * Lo que necesita Ladybird (los recursos, las letras, las autoridades) lo
 * deja MWEB --prepara en /tmp/web, como para el navegador sin ventana. */

#include <AK/Hex.h>
#include <AK/QuickSort.h>
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
#include <LibIPC/File.h>
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
static constexpr int N_BOTONES = 4;                 // ← → ↻ ⌂
static constexpr int ANCHO_PESTAÑA = 210;
static constexpr int ALTO_BUSCA = 36;               // [etapa 10 i] la barra de buscar, abajo
static constexpr int ALTO_AVISO = 28;               // [etapa 10 i] el aviso, abajo
static constexpr int TAM = 13 * 4;                  // 13 puntos (en cuartos, como la oficina)
static constexpr char const* INICIO = "https://hispados.es/";
// [etapa 10 i] la casa (como MWEB: sin usuarios, en /tmp/web) y lo que se guarda en ella
static constexpr char const* CASA = "/tmp/web/casa";
static constexpr char const* DESCARGAS = "/tmp/web/casa/Descargas";
static constexpr char const* INICIO_PROPIO = "file:///tmp/web/casa/inicio.html";
static constexpr char const* BUSCADOR = "https://duckduckgo.com/html/?q=";

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
static constexpr u32 C_ESTRELLA = 0xE8A317;
static constexpr u32 C_AVISO = 0xFFF4D6;
static constexpr u32 C_NO_HAY = 0xF8D7DA;

// [etapa 10 i] las etiquetas de los menús del escritorio (llegan en NV_MENU)
static constexpr u32 ETQ_CONTEXTO = 0x4E01;
static constexpr u32 ETQ_MENÚ = 0x4E02;
static constexpr u32 ETQ_MARCADORES = 0x4E03;
static constexpr u32 ETQ_HISTORIAL = 0x4E04;
static constexpr u32 ETQ_SELECT = 0x4E05;
static constexpr u32 ETQ_DESCARGAS = 0x4E06;
static constexpr size_t MÁX_MENÚ = 32;              // (los de VENTANAS: 32 opciones como mucho)

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
    double zoom { 1.0 };
    Gfx::Cursor cursor { Gfx::StandardCursor::Arrow };
};

// [etapa 10 i] una página guardada (un marcador o una del historial)
struct Sitio {
    ByteString dirección;
    ByteString título;
};

// [etapa 10 i] lo que puede hacer una opción de un menú
enum class Acción {
    Nada,
    AbrirEnlaceNueva, AbrirEnlace, CopiarEnlace, GuardarEnlace,
    AbrirImagenNueva, GuardarImagen, CopiarImagenDir,
    Copiar, Cortar, Pegar, SeleccionarTodo, BuscarElegido,
    Atrás, Adelante, Recargar, Marcador,
    PestañaNueva, Inicio, Marcadores, Historial, Descargas, AbrirDescargas,
    Buscar, Ampliar, Reducir, Normal, CerrarPestaña,
    IrA, BorrarHistorial, OpciónSelect,
};

struct Opción {
    ByteString texto;
    Acción acción { Acción::Nada };
    bool gris { false };
    ByteString dirección;           // (IrA)
    u32 id { 0 };                   // (OpciónSelect)
};

// [etapa 10 i] un cuadro que ha pedido una página (se atienden de uno en uno)
enum class Cuadro { Aviso, Confirma, Pregunta, AntesDeSalir, Archivo };
struct Petición {
    Cuadro clase;
    u64 vista { 0 };
    u64 respuesta { 0 };            // (AntesDeSalir: el answer_id)
    ByteString mensaje;
    ByteString valor;
};

// [etapa 10 i] una descarga
struct Descarga {
    u64 id { 0 };
    ByteString destino;
    WebView::FileDownloader::DownloadStatus estado { WebView::FileDownloader::DownloadStatus::InProgress };
    u64 hecho { 0 };
    Optional<u64> total;
    bool avisada { false };
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
    // [etapa 10 i]
    void pide_cuadro(Petición);
    void menú_contextual(u64 vista, WebView::ContextMenuRequest const&);
    void menú_select(u64 vista, Gfx::IntPoint, Vector<Web::HTML::SelectItem> const&);
    void visitada(URL::URL const&, Optional<String> const& título);
    void título_visitada(URL::URL const&, String const& título);
    void resultado_buscar(u64 vista, size_t actual, Optional<size_t> total);
    void zoom_cambiado(u64 vista);
    void texto_elegido(u64 petición, Optional<ByteString> const&);
    ByteString ruta_de_descarga(ByteString const& archivo);
    void descarga(Hub::DownloadState const&);
    void descarga_acabada(ByteString const& nombre, ByteString const& ruta);
    void avisa(ByteString texto, bool malo = false);

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
    void pinta_abajo(int alto_página);
    void atiende_sucesos();
    void atiende(nv_suceso const&);
    void tecla(nv_suceso const&);
    void tecla_en_barra(nv_suceso const&);
    void tecla_en_busca(nv_suceso const&);
    void ratón(nv_suceso const&);
    void ve_a(String const&);
    void tamaño(int an, int al);
    Web::UIEvents::KeyModifier modificadores(int mods) const;
    // [etapa 10 i]
    void siguiente_cuadro();
    void cuadro_suceso(nv_suceso const&);
    void abre_menú(int x, int y, u32 etiqueta, Vector<Opción>);
    void menú_elegido(nv_suceso const&);
    void haz(Opción const&);
    void menú_principal();
    void menú_marcadores();
    void menú_historial();
    void menú_descargas();
    void abre_busca();
    void cierra_busca();
    void busca(bool otra_vez, bool atrás);
    void marca_o_desmarca();
    bool es_marcador(StringView) const;
    void lee_sitios();
    void guarda_sitios();
    void escribe_inicio();
    URL::URL url_de(char const*);
    void ir(char const*);
    int alto_abajo() const;

    RefPtr<Conexion> m_hub;
    Optional<Core::Process> m_proceso_hub;
    int m_v { 0 };
    int m_an { 0 };
    int m_al { 0 };
    Vector<u32> m_barra;                    // los puntos de la barra (m_an x ALTO_BARRA)
    Vector<u32> m_blanco;                   // lo que no cubre la página
    Vector<u32> m_capa;                     // [etapa 10 i] la página atenuada, con el cuadro encima
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
    // [etapa 10 i] los cuadros, los menús, buscar, el aviso de abajo
    Vector<Petición> m_cuadros;             // el primero, el que está abierto (si hay)
    bool m_cuadro_abierto { false };
    ByteString m_carpeta_subir { "/" };
    Vector<Opción> m_menú;
    u32 m_menú_etiqueta { 0 };
    u64 m_menú_vista { 0 };
    bool m_buscando { false };
    bool m_busca_foco { false };
    Vector<u32> m_busca;
    size_t m_busca_cursor { 0 };
    ByteString m_busca_dice;
    bool m_busca_no_hay { false };
    Optional<ByteString> m_aviso_texto;
    bool m_aviso_malo { false };
    RefPtr<Core::Timer> m_aviso_reloj;
    Vector<Sitio> m_marcadores;
    Vector<Sitio> m_historial;              // el más nuevo, el primero
    Vector<Descarga> m_descargas;
    u64 m_siguiente_petición { 1 };
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
    p->zoom = state.zoom_level;
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
        m_interfaz.abre_pestaña(URL::Parser::basic_parse(StringView { INICIO_PROPIO, strlen(INICIO_PROPIO) }).value(), true);
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

// [etapa 10 i] los cuadros de las páginas, de verdad (ventana.c: los de la oficina)
void Conexion::view_request_alert(u64 view_id, Utf16String message)
{
    m_interfaz.pide_cuadro({ Cuadro::Aviso, view_id, 0, message.to_utf8().to_byte_string(), {} });
}

void Conexion::view_request_confirm(u64 view_id, Utf16String message)
{
    m_interfaz.pide_cuadro({ Cuadro::Confirma, view_id, 0, message.to_utf8().to_byte_string(), {} });
}

void Conexion::view_request_prompt(u64 view_id, Utf16String message, Utf16String default_value)
{
    m_interfaz.pide_cuadro({ Cuadro::Pregunta, view_id, 0, message.to_utf8().to_byte_string(), default_value.to_utf8().to_byte_string() });
}

void Conexion::view_request_before_unload(u64 view_id, u64 answer_id, String)
{
    m_interfaz.pide_cuadro({ Cuadro::AntesDeSalir, view_id, answer_id, {}, {} });
}

void Conexion::view_request_external_url_confirmation(u64 view_id, u64 answer_id, URL::URL, URL::Origin, String)
{
    async_view_answer(view_id, answer_id, false);
}

void Conexion::view_request_permission(u64 view_id, u64 answer_id, WebView::PermissionPromptRequest)
{
    async_view_answer_permission_request(view_id, answer_id, Web::Permissions::PromptOutcome::Dismissed);
}

void Conexion::view_request_select_dropdown(u64 view_id, Gfx::IntPoint content_position, i32, Vector<Web::HTML::SelectItem> items)
{
    m_interfaz.menú_select(view_id, content_position, items);
}

void Conexion::view_request_file_picker(u64 view_id, Web::HTML::FileFilter, Web::HTML::AllowMultipleFiles)
{
    m_interfaz.pide_cuadro({ Cuadro::Archivo, view_id, 0, {}, {} });
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
    // [etapa 10 i] las descargas, a la carpeta Descargas de la casa
    async_answer_download_path(request_id, m_interfaz.ruta_de_descarga(file));
}

void Conexion::app_display_download_confirmation_dialog(String download_name, ByteString path)
{
    m_interfaz.descarga_acabada(download_name.to_byte_string(), path);
}

void Conexion::download_added(Hub::DownloadState download)
{
    m_interfaz.descarga(download);
}

void Conexion::download_updated(Hub::DownloadState download)
{
    m_interfaz.descarga(download);
}

void Conexion::app_display_error_dialog(String message)
{
    warnln("NAVEGADOR: {}", message);
    m_interfaz.avisa(message.to_byte_string(), true);
}

void Conexion::app_quit_for_signal()
{
    m_interfaz.sale(0);
}

// [etapa 10 i] el menú del botón derecho, el texto elegido, el historial, buscar y el zoom
void Conexion::view_request_context_menu(u64 view_id, WebView::ContextMenuRequest request)
{
    m_interfaz.menú_contextual(view_id, request);
}

void Conexion::view_selected_text(u64, u64 request_id, Optional<ByteString> text)
{
    m_interfaz.texto_elegido(request_id, text);
}

void Conexion::view_visited_page(u64, URL::URL url, Optional<String> title, WebView::HistoryVisitTransition)
{
    m_interfaz.visitada(url, title);
}

void Conexion::view_visited_page_title_change(u64, URL::URL url, String title)
{
    m_interfaz.título_visitada(url, title);
}

void Conexion::view_find_in_page(u64 view_id, size_t current_match_index, Optional<size_t> total_match_count)
{
    m_interfaz.resultado_buscar(view_id, current_match_index, total_match_count);
}

void Conexion::view_update_zoom(u64 view_id)
{
    m_interfaz.zoom_cambiado(view_id);
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
    (void)mkdir(CASA, 0755);
    (void)mkdir(DESCARGAS, 0755);
    (void)mkdir("/tmp/web/run", 0700);
    setenv("XDG_DATA_DIRS", "/share", 1);
    setenv("HOME", CASA, 1);
    setenv("XDG_CONFIG_HOME", "/tmp/web/casa/.config", 1);
    setenv("XDG_DATA_HOME", "/tmp/web/casa/.local/share", 1);
    setenv("XDG_CACHE_HOME", "/tmp/web/casa/.cache", 1);
    setenv("XDG_RUNTIME_DIR", "/tmp/web/run", 1);
    setenv("XDG_DOWNLOAD_DIR", DESCARGAS, 1);
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
    h.async_view_set_has_system_focus(p.vista, es_activa && !m_editando && !m_busca_foco);
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
        if (m_buscando)
            m_hub->async_view_find_in_page_end(a->vista);
    }
    m_activa = i;
    auto& p = *m_pestañas[i];
    m_hub->async_active_view_changed(p.vista);
    m_editando = false;
    m_buscando = false;
    m_busca_foco = false;
    prepara_vista(p, true);
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

// un rectángulo en unos puntos de an x al
static void rect(Vector<u32>& px, int an, int x, int y, int w, int h, u32 c, int al = ALTO_BARRA)
{
    for (int f = max(0, y); f < min(al, y + h); f++) {
        for (int k = max(0, x); k < min(an, x + w); k++)
            px[f * an + k] = c;
    }
}

// un punto mezclado con lo que hay (a: cuánto del color, de 0 a 1)
static void mezcla(Vector<u32>& px, int an, int al, int x, int y, double a, u32 c)
{
    if (x < 0 || x >= an || y < 0 || y >= al || a <= 0)
        return;
    a = min(a, 1.0);
    auto& d = px[y * an + x];
    u32 r = 0;
    for (int k = 0; k < 24; k += 8) {
        double o = (d >> k) & 0xFF, t = (c >> k) & 0xFF;
        r |= (u32)(o + (t - o) * a + 0.5) << k;
    }
    d = r;
}

// ↻ (la DejaVu recortada de la oficina no la tiene): un anillo de radio 7
// abierto arriba a la derecha y la punta de la flecha, con suavizado
static void icono_recarga(Vector<u32>& px, int an, int cx, int cy, u32 c)
{
    for (int y = -10; y <= 10; y++) {
        for (int x = -10; x <= 10; x++) {
            double dist = __builtin_sqrt((double)(x * x + y * y));
            double ang = __builtin_atan2((double)-y, (double)x) * 180 / 3.14159265;
            if (ang > 15 && ang < 70)
                continue;
            double a = 1.0 - __builtin_fabs(dist - 6.5) / 1.2;
            mezcla(px, an, ALTO_BARRA, cx + x, cy + y, a, c);
        }
    }
    // la punta: un triángulo en el final del anillo (arriba a la derecha)
    for (int y = 0; y < 7; y++) {
        for (int x = 0; x <= y; x++)
            mezcla(px, an, ALTO_BARRA, cx + 3 + x, cy - 9 + y, 1.0, c);
    }
}

// [etapa 10 i] ¿está (x, y) dentro del polígono? (par-impar)
static bool dentro(double const* pts, int n, double x, double y)
{
    bool d = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = pts[2 * i], yi = pts[2 * i + 1], xj = pts[2 * j], yj = pts[2 * j + 1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
            d = !d;
    }
    return d;
}

// [etapa 10 i] un polígono relleno, suavizado (4 x 4 muestras por punto); con
// «hueco», menos lo de dentro de ese otro (para dibujar solo el borde)
static void polígono(Vector<u32>& px, int an, int al, double const* pts, int n, u32 c, double const* hueco = nullptr)
{
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (int i = 0; i < n; i++) {
        x0 = min(x0, pts[2 * i]);
        x1 = max(x1, pts[2 * i]);
        y0 = min(y0, pts[2 * i + 1]);
        y1 = max(y1, pts[2 * i + 1]);
    }
    for (int y = (int)y0 - 1; y <= (int)y1 + 1; y++) {
        for (int x = (int)x0 - 1; x <= (int)x1 + 1; x++) {
            int k = 0;
            for (int sy = 0; sy < 4; sy++) {
                for (int sx = 0; sx < 4; sx++) {
                    double mx = x + (sx + 0.5) / 4, my = y + (sy + 0.5) / 4;
                    if (dentro(pts, n, mx, my) && !(hueco && dentro(hueco, n, mx, my)))
                        k++;
                }
            }
            mezcla(px, an, al, x, y, k / 16.0, c);
        }
    }
}

// [etapa 10 i] ☆ y ★ (tampoco los tiene la letra recortada)
static void icono_estrella(Vector<u32>& px, int an, int cx, int cy, bool llena, u32 c)
{
    double fuera[20], hueco[20];
    for (int i = 0; i < 10; i++) {
        double r = (i % 2) ? 4.2 : 9.5, a = -3.14159265 / 2 + i * 3.14159265 / 5;
        fuera[2 * i] = cx + r * __builtin_cos(a);
        fuera[2 * i + 1] = cy + 1 + r * __builtin_sin(a);
        double r2 = (i % 2) ? 2.6 : 6.6;
        hueco[2 * i] = cx + r2 * __builtin_cos(a);
        hueco[2 * i + 1] = cy + 1.4 + r2 * __builtin_sin(a);
    }
    polígono(px, an, ALTO_BARRA, fuera, 10, c, llena ? nullptr : hueco);
}

// [etapa 10 i] ⌂ (la casa: la página de inicio)
static void icono_casa(Vector<u32>& px, int an, int cx, int cy, u32 c)
{
    double tejado[] = { cx - 9.0, cy - 0.5, (double)cx, cy - 9.0, cx + 9.0, cy - 0.5, cx + 6.5, cy - 0.5, (double)cx, cy - 6.5, cx - 6.5, cy - 0.5 };
    polígono(px, an, ALTO_BARRA, tejado, 6, c);
    double cuerpo[] = { cx - 6.0, cy - 1.0, cx + 6.0, cy - 1.0, cx + 6.0, cy + 8.0, cx - 6.0, cy + 8.0 };
    double puerta[] = { cx - 4.3, cy + 0.6, cx + 4.3, cy + 0.6, cx + 4.3, cy + 6.4, cx - 4.3, cy + 6.4 };
    polígono(px, an, ALTO_BARRA, cuerpo, 4, c, puerta);
    double puerta2[] = { cx - 1.6, cy + 2.0, cx + 1.6, cy + 2.0, cx + 1.6, cy + 8.0, cx - 1.6, cy + 8.0 };
    polígono(px, an, ALTO_BARRA, puerta2, 4, c);
}

// [etapa 10 i] ☰ (el menú)
static void icono_menú(Vector<u32>& px, int an, int cx, int cy, u32 c)
{
    for (int k = -1; k <= 1; k++)
        rect(px, an, cx - 8, cy + k * 6 - 1, 16, 2, c);
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

// [etapa 10 i] el texto, de N letras como mucho (para los menús)
static ByteString corto(StringView t, size_t n)
{
    Utf8View u(t);
    if (u.length() <= n)
        return t;
    StringBuilder sb;
    size_t k = 0;
    for (auto c : u) {
        if (k++ >= n - 1)
            break;
        sb.append_code_point(c);
    }
    sb.append("…"sv);
    return sb.to_byte_string();
}

// (las x de lo de la barra de herramientas)
static int x_dirección() { return 8 + N_BOTONES * ANCHO_BOTÓN + 8; }
static int x_estrella(int an) { return an - 10 - 2 * ANCHO_BOTÓN; }
static int x_menú(int an) { return an - 10 - ANCHO_BOTÓN; }

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
    // las herramientas: ← → ↻ ⌂, la dirección, ☆ y ☰
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
    icono_casa(px, an, 8 + 3 * ANCHO_BOTÓN + 14, y0 + 19, C_TEXTO);
    int cx = x_dirección(), cw = x_estrella(an) - 6 - cx;
    rect(px, an, cx, y0 + 5, cw, ALTO_HERRAMIENTAS - 10, m_editando ? C_FOCO : C_BORDE);
    rect(px, an, cx + 1, y0 + 6, cw - 2, ALTO_HERRAMIENTAS - 12, C_CAMPO);
    // (el zoom, si no es el 100 %, a la derecha de la dirección)
    int hueco_zoom = 0;
    if (p && !m_editando && (p->zoom < 0.995 || p->zoom > 1.005)) {
        auto z = ByteString::formatted("{} %", (int)(p->zoom * 100 + 0.5));
        hueco_zoom = nv_ancho(z.characters(), 0, TAM) + 16;
        nv_texto(px.data(), an, ALTO_BARRA, an, cx + cw - hueco_zoom + 6, y0 + 24, z.characters(), C_GRIS, 0, TAM);
    }
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
        if (todo.is_empty())
            nv_texto(px.data(), an, ALTO_BARRA, an, cx + 8, y0 + 24, "Escribe una dirección o lo que quieras buscar", C_GRIS, 0, TAM);
        else
            nv_texto(px.data(), an, ALTO_BARRA, an, cx + 8, y0 + 24, todo.characters(), C_TEXTO, 0, TAM);
        int k = cx + 8 + nv_ancho(antes.characters(), 0, TAM);
        rect(px, an, k, y0 + 9, 1, ALTO_HERRAMIENTAS - 18, C_TEXTO);
    } else if (p) {
        // (la página de inicio propia no enseña su ruta: se escribe encima)
        if (p->dirección.bytes_as_string_view() == StringView { INICIO_PROPIO, strlen(INICIO_PROPIO) })
            nv_texto(px.data(), an, ALTO_BARRA, an, cx + 8, y0 + 24, "Escribe una dirección o lo que quieras buscar", C_GRIS, 0, TAM);
        else
            nv_texto(px.data(), an, ALTO_BARRA, an, cx + 8, y0 + 24, recorta(p->dirección.bytes_as_string_view(), cw - 16 - hueco_zoom).characters(), C_TEXTO, 0, TAM);
    }
    icono_estrella(px, an, x_estrella(an) + ANCHO_BOTÓN / 2, y0 + 18, p && es_marcador(p->dirección.bytes_as_string_view()), p && es_marcador(p->dirección.bytes_as_string_view()) ? C_ESTRELLA : C_TEXTO);
    icono_menú(px, an, x_menú(an) + ANCHO_BOTÓN / 2, y0 + 19, C_TEXTO);
    nv_mapa(m_v, 0, 0, an, ALTO_BARRA, px.data(), an);
    // el título de la ventana
    if (p) {
        auto t = ByteString::formatted("{} — Navegador", p->título.is_empty() ? p->dirección : p->título);
        nv_titulo(m_v, t.characters());
    }
}

int Interfaz::alto_abajo() const
{
    return (m_buscando ? ALTO_BUSCA : 0) + (m_aviso_texto.has_value() ? ALTO_AVISO : 0);
}

void Interfaz::pinta_página()
{
    int alto = m_al - ALTO_BARRA;
    if (alto <= 0 || m_an <= 0)
        return;
    auto* p = activa();
    int hecho_an = 0, hecho_al = 0;
    Gfx::Bitmap const* bm = nullptr;
    if (p) {
        if (auto f = p->búferes.displayed_frame(); f.has_value() && f->shared_image_buffer)
            bm = f->shared_image_buffer->bitmap_if_present();
    }
    // [etapa 10 i] con un cuadro abierto: la página atenuada y el cuadro encima
    if (m_cuadro_abierto) {
        m_capa.resize(m_an * alto);
        for (int y = 0; y < alto; y++) {
            auto* d = m_capa.data() + y * m_an;
            int k = 0;
            if (bm && y < bm->height()) {
                auto const* o = reinterpret_cast<u32 const*>(bm->scanline(y));
                bool rgba = !(bm->format() == Gfx::BitmapFormat::BGRA8888 || bm->format() == Gfx::BitmapFormat::BGRx8888);
                for (; k < min(bm->width(), m_an); k++) {
                    auto c = o[k];
                    if (rgba)
                        c = (c & 0xFF) << 16 | (c & 0xFF00) | (c >> 16 & 0xFF);
                    d[k] = c;
                }
            }
            for (; k < m_an; k++)
                d[k] = 0xFFFFFF;
            for (k = 0; k < m_an; k++) {
                auto c = d[k];
                u32 r = 0;
                for (int s = 0; s < 24; s += 8)
                    r |= (((c >> s) & 0xFF) * 55 / 100 + 28) << s;
                d[k] = r;
            }
        }
        nv_cuadro_pinta(m_capa.data(), m_an, alto);
        nv_mapa(m_v, 0, ALTO_BARRA, m_an, alto, m_capa.data(), m_an);
        return;
    }
    if (bm) {
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
    // lo que no cubre la página (al crecer la ventana, hasta el siguiente cuadro), en blanco
    if (m_blanco.size() < (size_t)m_an)
        m_blanco.resize(m_an);
    for (auto& c : m_blanco)
        c = 0xFFFFFF;
    if (hecho_an < m_an)
        nv_mapa(m_v, hecho_an, ALTO_BARRA, m_an - hecho_an, alto, m_blanco.data(), 0);
    if (hecho_al < alto)
        nv_mapa(m_v, 0, ALTO_BARRA + hecho_al, hecho_an, alto - hecho_al, m_blanco.data(), 0);
    pinta_abajo(alto);
}

// [etapa 10 i] lo de abajo, encima de la página: el enlace bajo el ratón, el
// aviso y la barra de buscar
void Interfaz::pinta_abajo(int alto)
{
    int y_abajo = m_al;
    if (m_buscando) {
        int h = ALTO_BUSCA, w = m_an;
        Vector<u32> b;
        b.resize(w * h);
        rect(b, w, 0, 0, w, h, C_PESTAÑA, h);
        rect(b, w, 0, 0, w, 1, C_BORDE, h);
        nv_texto(b.data(), w, h, w, 12, 23, "Buscar en la página:", C_TEXTO, 0, TAM);
        int fx = 12 + nv_ancho("Buscar en la página:", 0, TAM) + 10, fw = 320;
        rect(b, w, fx, 5, fw, h - 10, m_busca_foco ? C_FOCO : C_BORDE, h);
        rect(b, w, fx + 1, 6, fw - 2, h - 12, m_busca_no_hay ? C_NO_HAY : C_CAMPO, h);
        StringBuilder sb;
        for (size_t i = 0; i < m_busca_cursor; i++)
            sb.append_code_point(m_busca[i]);
        auto antes = sb.to_byte_string();
        for (size_t i = m_busca_cursor; i < m_busca.size(); i++)
            sb.append_code_point(m_busca[i]);
        auto todo = sb.to_byte_string();
        nv_texto(b.data(), w, h, w, fx + 8, 23, recorta(todo, fw - 16).characters(), C_TEXTO, 0, TAM);
        if (m_busca_foco)
            rect(b, w, fx + 8 + nv_ancho(antes.characters(), 0, TAM), 9, 1, h - 18, C_TEXTO, h);
        int bx = fx + fw + 10;
        nv_texto(b.data(), w, h, w, bx + 8, 24, "▲", C_TEXTO, 0, TAM);
        nv_texto(b.data(), w, h, w, bx + ANCHO_BOTÓN + 8, 24, "▼", C_TEXTO, 0, TAM);
        nv_texto(b.data(), w, h, w, bx + 2 * ANCHO_BOTÓN + 12, 23, m_busca_dice.characters(), m_busca_no_hay ? 0xA0262E : C_GRIS, 0, TAM);
        nv_texto(b.data(), w, h, w, w - 30, 24, "✕", C_TEXTO, 1, TAM + 8);
        y_abajo -= h;
        nv_mapa(m_v, 0, y_abajo, w, h, b.data(), w);
    }
    if (m_aviso_texto.has_value()) {
        int h = ALTO_AVISO, w = m_an;
        Vector<u32> b;
        b.resize(w * h);
        rect(b, w, 0, 0, w, h, m_aviso_malo ? C_NO_HAY : C_AVISO, h);
        rect(b, w, 0, 0, w, 1, C_BORDE, h);
        nv_texto(b.data(), w, h, w, 12, 19, recorta(*m_aviso_texto, w - 60).characters(), C_TEXTO, 0, TAM);
        nv_texto(b.data(), w, h, w, w - 28, 20, "✕", C_GRIS, 1, TAM + 4);
        y_abajo -= h;
        nv_mapa(m_v, 0, y_abajo, w, h, b.data(), w);
    }
    (void)alto;
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
        nv_mapa(m_v, 0, y_abajo - h, min(w, m_an), h, caja.data(), w);
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

URL::URL Interfaz::url_de(char const* t)
{
    return URL::Parser::basic_parse(StringView { t, strlen(t) }).value();
}

void Interfaz::ir(char const* t)
{
    if (auto* p = activa()) {
        p->dirección = MUST(String::from_utf8(StringView { t, strlen(t) }));
        m_hub->async_view_load(p->vista, url_de(t), Web::Bindings::NavigationHistoryBehavior::Auto);
        m_editando = false;
        m_hub->async_view_set_has_system_focus(p->vista, true);
        ha_cambiado_la_barra();
    }
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
        sb.append(StringView { BUSCADOR, strlen(BUSCADOR) });
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

// [etapa 10 i] lo escrito en un campo de una línea (la barra de direcciones y la de buscar)
static bool edita(Vector<u32>& texto, size_t& cursor, bool& todo_marcado, nv_suceso const& s)
{
    auto t = s.tecla;
    if (todo_marcado && (t == 8 || t == NV_T_SUPR || (t >= 32 && t < NV_T_BASE && !(s.mods & NV_CON_CONTROL)))) {
        texto.clear();
        cursor = 0;
        todo_marcado = false;
        if (t == 8 || t == NV_T_SUPR)
            return true;
    }
    todo_marcado = false;
    if (t == 8) {
        if (cursor > 0)
            texto.remove(--cursor);
    } else if (t == NV_T_SUPR) {
        if (cursor < texto.size())
            texto.remove(cursor);
    } else if (t == NV_T_IZQUIERDA) {
        if (cursor > 0)
            cursor--;
    } else if (t == NV_T_DERECHA) {
        if (cursor < texto.size())
            cursor++;
    } else if (t == NV_T_INICIO) {
        cursor = 0;
    } else if (t == NV_T_FIN) {
        cursor = texto.size();
    } else if (t == 1 && (s.mods & NV_CON_CONTROL)) {
        todo_marcado = true;          // Ctrl+A
    } else if (t == 22 && (s.mods & NV_CON_CONTROL)) {
        u32 n = 0;                      // Ctrl+V
        if (char* d = nv_del_portapapeles(&n)) {
            for (auto c : Utf8View(StringView { d, n })) {
                if (c >= 32)
                    texto.insert(cursor++, c);
            }
            free(d);
        }
    } else if (t == 3 && (s.mods & NV_CON_CONTROL)) {
        StringBuilder sb;               // Ctrl+C: todo lo escrito
        for (auto c : texto)
            sb.append_code_point(c);
        auto b = sb.to_byte_string();
        nv_al_portapapeles(b.characters(), b.length());
    } else if (t >= 32 && t < NV_T_BASE && !(s.mods & NV_CON_CONTROL)) {
        texto.insert(cursor++, t);
    } else {
        return false;
    }
    return true;
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
    edita(m_texto, m_cursor, m_todo_marcado, s);
    ha_cambiado_la_barra();
}

// ---- [etapa 10 i] buscar en la página ----

void Interfaz::abre_busca()
{
    auto* p = activa();
    if (!p)
        return;
    m_buscando = true;
    m_busca_foco = true;
    m_busca_cursor = m_busca.size();
    m_editando = false;
    m_hub->async_view_set_has_system_focus(p->vista, false);
    m_página_sucia = true;
    ha_cambiado_la_barra();
    outln("NAVEGADOR: buscar en la página");
}

void Interfaz::cierra_busca()
{
    if (auto* p = activa()) {
        m_hub->async_view_find_in_page_end(p->vista);
        m_hub->async_view_set_has_system_focus(p->vista, true);
    }
    m_buscando = false;
    m_busca_foco = false;
    m_busca_dice = {};
    m_busca_no_hay = false;
    m_página_sucia = true;
    pide_pintar();
}

void Interfaz::busca(bool otra_vez, bool atrás)
{
    auto* p = activa();
    if (!p)
        return;
    StringBuilder sb;
    for (auto c : m_busca)
        sb.append_code_point(c);
    auto q = sb.to_byte_string();
    if (q.is_empty()) {
        m_hub->async_view_find_in_page_end(p->vista);
        m_busca_dice = {};
        m_busca_no_hay = false;
        m_página_sucia = true;
        pide_pintar();
        return;
    }
    if (otra_vez && !atrás)
        m_hub->async_view_find_in_page_next_match(p->vista);
    else if (otra_vez)
        m_hub->async_view_find_in_page_previous_match(p->vista);
    else
        m_hub->async_view_find_in_page(p->vista, Utf16String::from_utf8_without_validation(q), CaseSensitivity::CaseInsensitive, true);
}

void Interfaz::resultado_buscar(u64 vista, size_t actual, Optional<size_t> total)
{
    auto* p = activa();
    if (!p || p->vista != vista || !m_buscando)
        return;
    if (total.has_value() && *total == 0) {
        m_busca_dice = "No está";
        m_busca_no_hay = true;
    } else if (total.has_value()) {
        m_busca_dice = ByteString::formatted("{} de {}", actual + 1, *total);
        m_busca_no_hay = false;
    } else {
        m_busca_dice = {};
        m_busca_no_hay = false;
    }
    // (para las pruebas)
    outln("NAVEGADOR: buscar: {}", m_busca_dice.is_empty() ? "-"sv : m_busca_dice.view());
    m_página_sucia = true;
    pide_pintar();
}

void Interfaz::tecla_en_busca(nv_suceso const& s)
{
    auto t = s.tecla;
    if (t == 27) {
        cierra_busca();
        return;
    }
    if (t == 10 || t == 13) {
        busca(true, (s.mods & NV_CON_MAYUS) != 0);
        return;
    }
    bool marcado = false;
    auto antes = m_busca.size();
    Vector<u32> viejo = m_busca;
    if (edita(m_busca, m_busca_cursor, marcado, s)) {
        if (m_busca != viejo || m_busca.size() != antes)
            busca(false, false);
        m_página_sucia = true;
        pide_pintar();
    }
}

// ---- [etapa 10 i] el zoom, el texto elegido, el aviso ----

void Interfaz::zoom_cambiado(u64 vista)
{
    auto* p = pestaña(vista);
    if (!p)
        return;
    p->zoom = m_hub->view_description(vista).zoom_level;
    outln("NAVEGADOR: zoom {} %", (int)(p->zoom * 100 + 0.5));
    ha_cambiado_la_barra();
}

void Interfaz::texto_elegido(u64, Optional<ByteString> const& t)
{
    if (!t.has_value() || t->is_empty())
        return;
    nv_al_portapapeles(t->characters(), t->length());
    outln("NAVEGADOR: copiado «{}»", corto(*t, 60));
}

void Interfaz::avisa(ByteString texto, bool malo)
{
    outln("NAVEGADOR: aviso «{}»", texto);
    m_aviso_texto = move(texto);
    m_aviso_malo = malo;
    if (!m_aviso_reloj) {
        m_aviso_reloj = Core::Timer::create_single_shot(AK::Duration::from_milliseconds(8000), [this] {
            m_aviso_texto = {};
            m_página_sucia = true;
            pide_pintar();
        });
    }
    m_aviso_reloj->restart();
    m_página_sucia = true;
    pide_pintar();
}

// ---- [etapa 10 i] las descargas ----

ByteString Interfaz::ruta_de_descarga(ByteString const& archivo)
{
    (void)mkdir(DESCARGAS, 0755);
    // (el nombre, sin carpetas; si ya está, «nombre (2).ext»...)
    auto nombre = archivo;
    if (auto k = nombre.find_last('/'); k.has_value())
        nombre = nombre.substring(*k + 1);
    if (nombre.is_empty())
        nombre = "descarga";
    auto ruta = ByteString::formatted("{}/{}", DESCARGAS, nombre);
    struct stat st;
    for (int n = 2; stat(ruta.characters(), &st) == 0 && n < 1000; n++) {
        auto punto = nombre.find_last('.');
        if (punto.has_value() && *punto > 0)
            ruta = ByteString::formatted("{}/{} ({}){}", DESCARGAS, nombre.substring(0, *punto), n, nombre.substring(*punto));
        else
            ruta = ByteString::formatted("{}/{} ({})", DESCARGAS, nombre, n);
    }
    outln("NAVEGADOR: descarga a {}", ruta);
    return ruta;
}

static ByteString tamaño_de(u64 n)
{
    if (n < 1024)
        return ByteString::formatted("{} octetos", n);
    if (n < 1024 * 1024)
        return ByteString::formatted("{} KB", (n + 512) / 1024);
    return ByteString::formatted("{},{} MB", n / (1024 * 1024), (n % (1024 * 1024)) * 10 / (1024 * 1024));
}

void Interfaz::descarga(Hub::DownloadState const& d)
{
    Descarga* e = nullptr;
    for (auto& x : m_descargas) {
        if (x.id == d.id)
            e = &x;
    }
    if (!e) {
        m_descargas.append({});
        e = &m_descargas.last();
        e->id = d.id;
    }
    e->destino = d.destination;
    e->estado = d.status;
    e->hecho = d.downloaded_size;
    e->total = d.total_size;
    auto nombre = d.destination;
    if (auto k = nombre.find_last('/'); k.has_value())
        nombre = nombre.substring(*k + 1);
    using St = WebView::FileDownloader::DownloadStatus;
    if (!e->avisada && d.status == St::Completed) {
        e->avisada = true;
        outln("NAVEGADOR: descargado {} ({} octetos)", d.destination, d.downloaded_size);
        avisa(ByteString::formatted("Descargado «{}» ({}), en la carpeta Descargas ({})", nombre, tamaño_de(d.downloaded_size), DESCARGAS));
    } else if (!e->avisada && (d.status == St::Failed || d.status == St::Canceled)) {
        e->avisada = true;
        outln("NAVEGADOR: descarga fallida {}", d.destination);
        avisa(ByteString::formatted("No se ha podido descargar «{}»{}{}", nombre, d.error.has_value() ? ": "sv : ""sv,
                  d.error.has_value() ? d.error->to_byte_string() : ByteString {}),
            true);
    }
}

void Interfaz::descarga_acabada(ByteString const& nombre, ByteString const& ruta)
{
    // (el Hub lo dice para las capturas; las descargas avisan en descarga())
    for (auto& x : m_descargas) {
        if (x.destino == ruta && x.avisada)
            return;
    }
    avisa(ByteString::formatted("Guardado «{}» en {}", nombre, ruta));
}

// ---- [etapa 10 i] los marcadores, el historial y la página de inicio ----

static ByteString sin_tabs(StringView t)
{
    StringBuilder sb;
    for (auto c : t)
        sb.append(c == '\t' || c == '\n' || c == '\r' ? ' ' : c);
    return sb.to_byte_string();
}

static Vector<Sitio> lee_lista(char const* ruta)
{
    Vector<Sitio> r;
    FILE* f = fopen(ruta, "r");
    if (!f)
        return r;
    char línea[4096];
    while (fgets(línea, sizeof línea, f)) {
        auto l = StringView { línea, strlen(línea) }.trim("\r\n"sv);
        auto tab = l.find('\t');
        if (l.is_empty())
            continue;
        if (tab.has_value())
            r.append({ l.substring_view(0, *tab), l.substring_view(*tab + 1) });
        else
            r.append({ l, l });
    }
    fclose(f);
    return r;
}

static void guarda_lista(char const* ruta, Vector<Sitio> const& l)
{
    auto tmp = ByteString::formatted("{}.nuevo", ruta);
    FILE* f = fopen(tmp.characters(), "w");
    if (!f)
        return;
    for (auto& s : l)
        fprintf(f, "%s\t%s\n", sin_tabs(s.dirección).characters(), sin_tabs(s.título).characters());
    fclose(f);
    rename(tmp.characters(), ruta);
}

void Interfaz::lee_sitios()
{
    struct stat st;
    auto rm = ByteString::formatted("{}/marcadores.txt", CASA);
    if (stat(rm.characters(), &st) != 0) {
        // (los de siempre, la primera vez)
        m_marcadores = {
            { "https://hispados.es/", "Hispa-DOS" },
            { "https://github.com/instcide-Es/hispados", "Hispa-DOS en GitHub" },
            { "https://es.wikipedia.org/", "Wikipedia en español" },
            { "https://duckduckgo.com/html/", "DuckDuckGo (buscador)" },
        };
        guarda_lista(rm.characters(), m_marcadores);
    } else {
        m_marcadores = lee_lista(rm.characters());
    }
    m_historial = lee_lista(ByteString::formatted("{}/historial.txt", CASA).characters());
}

void Interfaz::guarda_sitios()
{
    guarda_lista(ByteString::formatted("{}/marcadores.txt", CASA).characters(), m_marcadores);
    guarda_lista(ByteString::formatted("{}/historial.txt", CASA).characters(), m_historial);
    escribe_inicio();
}

bool Interfaz::es_marcador(StringView u) const
{
    for (auto& m : m_marcadores) {
        if (m.dirección == u)
            return true;
    }
    return false;
}

void Interfaz::marca_o_desmarca()
{
    auto* p = activa();
    if (!p)
        return;
    auto u = p->dirección.to_byte_string();
    if (u.view() == StringView { INICIO_PROPIO, strlen(INICIO_PROPIO) } || u.starts_with("about:"sv))
        return;
    for (size_t i = 0; i < m_marcadores.size(); i++) {
        if (m_marcadores[i].dirección == u) {
            m_marcadores.remove(i);
            guarda_sitios();
            avisa(ByteString::formatted("Quitado de los marcadores: «{}»", p->título.is_empty() ? u : p->título.to_byte_string()));
            ha_cambiado_la_barra();
            return;
        }
    }
    m_marcadores.append({ u, p->título.is_empty() ? u : p->título.to_byte_string() });
    guarda_sitios();
    avisa(ByteString::formatted("Añadido a los marcadores: «{}» (la estrella de arriba o Ctrl+B para verlos)", p->título.is_empty() ? u : p->título.to_byte_string()));
    ha_cambiado_la_barra();
}

void Interfaz::visitada(URL::URL const& url, Optional<String> const& título)
{
    auto u = url.serialize().to_byte_string();
    if (u.view() == StringView { INICIO_PROPIO, strlen(INICIO_PROPIO) } || u.starts_with("about:"sv) || u.starts_with("data:"sv))
        return;
    for (size_t i = 0; i < m_historial.size(); i++) {
        if (m_historial[i].dirección == u) {
            m_historial.remove(i);
            break;
        }
    }
    m_historial.prepend({ u, título.has_value() && !título->is_empty() ? título->to_byte_string() : u });
    while (m_historial.size() > 300)
        m_historial.take_last();
    guarda_sitios();
}

void Interfaz::título_visitada(URL::URL const& url, String const& título)
{
    auto u = url.serialize().to_byte_string();
    for (auto& h : m_historial) {
        if (h.dirección == u && !título.is_empty()) {
            h.título = título.to_byte_string();
            guarda_sitios();
            return;
        }
    }
}

static ByteString html(StringView t)
{
    StringBuilder sb;
    for (auto c : t) {
        if (c == '<')
            sb.append("&lt;"sv);
        else if (c == '>')
            sb.append("&gt;"sv);
        else if (c == '&')
            sb.append("&amp;"sv);
        else if (c == '"')
            sb.append("&quot;"sv);
        else
            sb.append(c);
    }
    return sb.to_byte_string();
}

void Interfaz::escribe_inicio()
{
    auto ruta = ByteString::formatted("{}/inicio.html", CASA);
    auto tmp = ByteString::formatted("{}.nuevo", ruta);
    FILE* f = fopen(tmp.characters(), "w");
    if (!f)
        return;
    fputs(R"HTML(<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<title>Inicio</title>
<style>
:root { font-family: "DejaVu Sans", sans-serif; color: #1d2228; background: #f4f6f9; }
body { margin: 0; padding: 48px 24px; }
main { max-width: 760px; margin: 0 auto; }
h1 { font-size: 30px; margin: 0 0 6px; color: #1e4a99; }
p.sub { margin: 0 0 26px; color: #5a6470; }
form { display: flex; gap: 8px; margin-bottom: 30px; }
input[type=search] { flex: 1; font: inherit; font-size: 17px; padding: 10px 14px; border: 2px solid #9aa2ac; border-radius: 8px; background: #fff; }
input[type=search]:focus { border-color: #2e6fd8; outline: none; }
button { font: inherit; font-size: 16px; padding: 0 20px; border: 0; border-radius: 8px; background: #2e6fd8; color: #fff; }
h2 { font-size: 17px; margin: 26px 0 10px; color: #3a4048; }
ul.accesos { display: grid; grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); gap: 10px; list-style: none; padding: 0; margin: 0; }
ul.accesos a { display: block; padding: 12px 14px; background: #fff; border: 1px solid #c9ced5; border-radius: 8px; color: #1d2228; text-decoration: none; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
ul.accesos a:hover { border-color: #2e6fd8; }
ul.accesos small { display: block; color: #8a9099; font-size: 12px; overflow: hidden; text-overflow: ellipsis; }
ol.vistas { padding-left: 22px; margin: 0; }
ol.vistas li { margin: 4px 0; }
ol.vistas a { color: #1e4a99; }
p.ayuda { margin-top: 34px; font-size: 13px; color: #5a6470; line-height: 1.6; }
</style>
</head>
<body>
<main>
<h1>Navegador de Hispa-64</h1>
<p class="sub">Escribe arriba una dirección, o busca aquí lo que quieras.</p>
<form action="https://duckduckgo.com/html/" method="get">
<input type="search" name="q" placeholder="Buscar en internet (DuckDuckGo)" autofocus>
<button type="submit">Buscar</button>
</form>
)HTML",
        f);
    fputs("<h2>Marcadores</h2>\n<ul class=\"accesos\">\n", f);
    for (auto& m : m_marcadores)
        fprintf(f, "<li><a href=\"%s\">%s<small>%s</small></a></li>\n", html(m.dirección).characters(), html(m.título).characters(), html(m.dirección).characters());
    if (m_marcadores.is_empty())
        fputs("<li>Aún no hay ninguno: Ctrl+D (o la estrella de arriba) guarda la página que estés viendo.</li>\n", f);
    fputs("</ul>\n", f);
    if (!m_historial.is_empty()) {
        fputs("<h2>Lo último que has visto</h2>\n<ol class=\"vistas\">\n", f);
        for (size_t i = 0; i < min<size_t>(12, m_historial.size()); i++)
            fprintf(f, "<li><a href=\"%s\">%s</a></li>\n", html(m_historial[i].dirección).characters(), html(corto(m_historial[i].título, 90)).characters());
        fputs("</ol>\n", f);
    }
    fputs(R"HTML(<p class="ayuda">Teclas: Ctrl+T, pestaña nueva; Ctrl+L, la barra de direcciones; Ctrl+F, buscar en la página;
Ctrl+D, guardar la página en los marcadores; Ctrl+B, los marcadores; Ctrl+H, el historial; Ctrl + y Ctrl &minus;, el tamaño
(Ctrl 0, el normal); F5, recargar; Ctrl+W, cerrar la pestaña. Con el botón derecho, el menú de lo que hay debajo.
Lo que se descarga va a la carpeta Descargas.</p>
</main>
</body>
</html>
)HTML",
        f);
    fclose(f);
    rename(tmp.characters(), ruta.characters());
}

// ---- [etapa 10 i] los menús ----

void Interfaz::abre_menú(int x, int y, u32 etiqueta, Vector<Opción> ops)
{
    if (ops.size() > MÁX_MENÚ)
        ops.shrink(MÁX_MENÚ);
    StringBuilder sb;
    u32 grises = 0;
    for (size_t i = 0; i < ops.size(); i++) {
        if (i)
            sb.append('|');
        // («|» separa las opciones)
        for (auto c : ops[i].texto)
            sb.append(c == '|' ? '!' : c);
        if (ops[i].gris || ops[i].acción == Acción::Nada)
            grises |= 1u << i;
    }
    m_menú = move(ops);
    m_menú_etiqueta = etiqueta;
    auto t = sb.to_byte_string();
    nv_menu(m_v, x, y, t.characters(), grises, etiqueta);
}

void Interfaz::menú_elegido(nv_suceso const& s)
{
    if (s.etiqueta != m_menú_etiqueta || s.opcion < 0 || (size_t)s.opcion >= m_menú.size()) {
        if (s.etiqueta == ETQ_SELECT && m_menú_vista)
            m_hub->async_view_select_dropdown_closed(m_menú_vista, {});
        m_menú_etiqueta = 0;
        return;
    }
    auto o = m_menú[s.opcion];
    m_menú_etiqueta = 0;
    outln("NAVEGADOR: menú «{}»", o.texto);
    haz(o);
}

static Optional<URL::URL> url_de_texto(StringView t)
{
    return URL::Parser::basic_parse(t);
}

void Interfaz::haz(Opción const& o)
{
    auto* p = activa();
    switch (o.acción) {
    case Acción::Nada:
        break;
    case Acción::AbrirEnlaceNueva:
    case Acción::AbrirImagenNueva:
        if (auto u = url_de_texto(o.dirección); u.has_value())
            abre_pestaña(*u, true);
        break;
    case Acción::AbrirEnlace:
    case Acción::IrA:
        if (p && !o.dirección.is_empty())
            ir(o.dirección.characters());
        break;
    case Acción::CopiarEnlace:
    case Acción::CopiarImagenDir:
        nv_al_portapapeles(o.dirección.characters(), o.dirección.length());
        avisa(ByteString::formatted("Copiada la dirección: {}", o.dirección));
        break;
    case Acción::GuardarEnlace:
    case Acción::GuardarImagen:
        if (auto u = url_de_texto(o.dirección); u.has_value() && p)
            m_hub->async_view_download_url(m_menú_vista ? m_menú_vista : p->vista, *u, true, {});
        break;
    case Acción::Copiar:
        if (!o.dirección.is_empty()) {
            nv_al_portapapeles(o.dirección.characters(), o.dirección.length());
            outln("NAVEGADOR: copiado «{}»", corto(o.dirección, 60));
        } else if (p) {
            m_hub->async_view_request_selected_text(p->vista, m_siguiente_petición++);
        }
        break;
    case Acción::Cortar:
        if (p)
            m_hub->async_view_cut_selection(p->vista);
        break;
    case Acción::Pegar:
        if (p)
            m_hub->async_view_paste(p->vista, Web::Clipboard::PasteMode::PlainTextOnly);
        break;
    case Acción::SeleccionarTodo:
        if (p)
            m_hub->async_view_select_all(p->vista);
        break;
    case Acción::BuscarElegido: {
        StringBuilder sb;
        sb.append(StringView { BUSCADOR, strlen(BUSCADOR) });
        sb.append(URL::percent_encode(o.dirección, URL::PercentEncodeSet::Query, URL::SpaceAsPlus::Yes));
        if (auto u = url_de_texto(sb.string_view()); u.has_value())
            abre_pestaña(*u, true);
        break;
    }
    case Acción::Atrás:
        if (p && p->atrás)
            m_hub->async_view_traverse_the_history_by_delta(p->vista, -1);
        break;
    case Acción::Adelante:
        if (p && p->adelante)
            m_hub->async_view_traverse_the_history_by_delta(p->vista, 1);
        break;
    case Acción::Recargar:
        if (p)
            m_hub->async_view_reload(p->vista, WebView::BypassCache::No);
        break;
    case Acción::Marcador:
        marca_o_desmarca();
        break;
    case Acción::PestañaNueva: {
        nv_suceso t {};
        t.clase = NV_TECLA;
        t.tecla = 20;
        t.mods = NV_CON_CONTROL;
        tecla(t);
        break;
    }
    case Acción::Inicio:
        ir(INICIO_PROPIO);
        break;
    case Acción::Marcadores:
        menú_marcadores();
        break;
    case Acción::Historial:
        menú_historial();
        break;
    case Acción::Descargas:
        menú_descargas();
        break;
    case Acción::AbrirDescargas:
        abre_pestaña(url_de("file:///tmp/web/casa/Descargas/"), true);
        break;
    case Acción::Buscar:
        abre_busca();
        break;
    case Acción::Ampliar:
        if (p)
            m_hub->async_view_zoom_in(p->vista);
        break;
    case Acción::Reducir:
        if (p)
            m_hub->async_view_zoom_out(p->vista);
        break;
    case Acción::Normal:
        if (p)
            m_hub->async_view_reset_zoom(p->vista);
        break;
    case Acción::CerrarPestaña:
        cierra_pestaña(m_activa);
        break;
    case Acción::BorrarHistorial:
        m_historial.clear();
        guarda_sitios();
        avisa("Historial borrado");
        break;
    case Acción::OpciónSelect:
        if (m_menú_vista)
            m_hub->async_view_select_dropdown_closed(m_menú_vista, o.id);
        break;
    }
}

void Interfaz::menú_contextual(u64 vista, WebView::ContextMenuRequest const& r)
{
    auto* p = pestaña(vista);
    if (!p || m_cuadro_abierto)
        return;
    Vector<Opción> ops;
    m_menú_vista = vista;
    if (r.link_navigation.has_value()) {
        auto u = r.link_navigation->url.serialize().to_byte_string();
        ops.append({ "Abrir el enlace en una pestaña nueva", Acción::AbrirEnlaceNueva, false, u });
        ops.append({ "Abrir el enlace aquí", Acción::AbrirEnlace, false, u });
        ops.append({ "Copiar la dirección del enlace", Acción::CopiarEnlace, false, u });
        ops.append({ "Guardar el enlace (descargarlo)", Acción::GuardarEnlace, false, u });
        ops.append({ "-", Acción::Nada });
    }
    if (r.kind == WebView::ContextMenuKind::Image && r.navigation.has_value()) {
        auto u = r.navigation->url.serialize().to_byte_string();
        ops.append({ "Abrir la imagen en una pestaña nueva", Acción::AbrirImagenNueva, false, u });
        ops.append({ "Guardar la imagen (en Descargas)", Acción::GuardarImagen, false, u });
        ops.append({ "Copiar la dirección de la imagen", Acción::CopiarImagenDir, false, u });
        ops.append({ "-", Acción::Nada });
    }
    bool hay_texto = r.selected_text.has_value() && !r.selected_text->is_empty();
    if (r.is_editable) {
        ops.append({ "Cortar  (Ctrl+X)", Acción::Cortar, !hay_texto });
        ops.append({ "Copiar  (Ctrl+C)", Acción::Copiar, !hay_texto, hay_texto ? r.selected_text->to_byte_string() : ByteString {} });
        ops.append({ "Pegar  (Ctrl+V)", Acción::Pegar });
        ops.append({ "Seleccionar todo  (Ctrl+A)", Acción::SeleccionarTodo });
        ops.append({ "-", Acción::Nada });
    } else if (hay_texto) {
        auto t = r.selected_text->to_byte_string();
        ops.append({ "Copiar  (Ctrl+C)", Acción::Copiar, false, t });
        ops.append({ ByteString::formatted("Buscar «{}» en internet", corto(t, 30)), Acción::BuscarElegido, false, t });
        ops.append({ "-", Acción::Nada });
    }
    ops.append({ "Atrás", Acción::Atrás, !p->atrás });
    ops.append({ "Adelante", Acción::Adelante, !p->adelante });
    ops.append({ "Recargar  (F5)", Acción::Recargar });
    ops.append({ "-", Acción::Nada });
    ops.append({ es_marcador(p->dirección.bytes_as_string_view()) ? "Quitar de los marcadores  (Ctrl+D)" : "Añadir a los marcadores  (Ctrl+D)", Acción::Marcador });
    if (!r.is_editable && !hay_texto)
        ops.append({ "Seleccionar todo  (Ctrl+A)", Acción::SeleccionarTodo });
    abre_menú(r.widget_position.x(), r.widget_position.y() + ALTO_BARRA, ETQ_CONTEXTO, move(ops));
}

void Interfaz::menú_select(u64 vista, Gfx::IntPoint en, Vector<Web::HTML::SelectItem> const& items)
{
    Vector<Opción> ops;
    auto pon = [&](Web::HTML::SelectItemOption const& o, bool dentro) {
        auto etiqueta = o.label.to_utf8();
        auto t = ByteString::formatted("{}{}{}", o.selected ? "✓ "sv : "    "sv, dentro ? "  "sv : ""sv, corto(etiqueta, 60));
        ops.append({ t, Acción::OpciónSelect, o.disabled, {}, o.id });
    };
    for (auto& it : items) {
        it.visit(
            [&](Web::HTML::SelectItemOption const& o) { pon(o, false); },
            [&](Web::HTML::SelectItemOptionGroup const& g) {
                auto etiqueta = g.label.to_utf8();
                ops.append({ corto(etiqueta, 60), Acción::Nada, true });
                for (auto& o : g.items)
                    pon(o, true);
            },
            [&](Web::HTML::SelectItemSeparator const&) { ops.append({ "-", Acción::Nada }); });
    }
    m_menú_vista = vista;
    if (ops.is_empty()) {
        m_hub->async_view_select_dropdown_closed(vista, {});
        return;
    }
    abre_menú(en.x(), en.y() + ALTO_BARRA, ETQ_SELECT, move(ops));
}

void Interfaz::menú_principal()
{
    auto* p = activa();
    Vector<Opción> ops;
    ops.append({ "Pestaña nueva  (Ctrl+T)", Acción::PestañaNueva });
    ops.append({ "Página de inicio", Acción::Inicio });
    ops.append({ "-", Acción::Nada });
    ops.append({ "Marcadores…  (Ctrl+B)", Acción::Marcadores });
    ops.append({ p && es_marcador(p->dirección.bytes_as_string_view()) ? "Quitar de los marcadores  (Ctrl+D)" : "Añadir a los marcadores  (Ctrl+D)", Acción::Marcador, !p });
    ops.append({ "Historial…  (Ctrl+H)", Acción::Historial });
    ops.append({ "Descargas…", Acción::Descargas });
    ops.append({ "-", Acción::Nada });
    ops.append({ "Buscar en la página  (Ctrl+F)", Acción::Buscar, !p });
    ops.append({ "Ampliar  (Ctrl +)", Acción::Ampliar, !p });
    ops.append({ "Reducir  (Ctrl -)", Acción::Reducir, !p });
    ops.append({ "Tamaño normal  (Ctrl 0)", Acción::Normal, !p });
    ops.append({ "-", Acción::Nada });
    ops.append({ "Recargar  (F5)", Acción::Recargar, !p });
    ops.append({ "Cerrar la pestaña  (Ctrl+W)", Acción::CerrarPestaña, !p });
    abre_menú(x_menú(m_an) + ANCHO_BOTÓN - 300, ALTO_BARRA, ETQ_MENÚ, move(ops));
}

void Interfaz::menú_marcadores()
{
    auto* p = activa();
    Vector<Opción> ops;
    bool está = p && es_marcador(p->dirección.bytes_as_string_view());
    ops.append({ está ? "Quitar esta página  (Ctrl+D)" : "Añadir esta página  (Ctrl+D)", Acción::Marcador, !p });
    ops.append({ "-", Acción::Nada });
    for (auto& m : m_marcadores)
        ops.append({ corto(m.título, 60), Acción::IrA, false, m.dirección });
    if (m_marcadores.is_empty())
        ops.append({ "(ningún marcador)", Acción::Nada, true });
    abre_menú(x_estrella(m_an) + ANCHO_BOTÓN - 340, ALTO_BARRA, ETQ_MARCADORES, move(ops));
}

void Interfaz::menú_historial()
{
    Vector<Opción> ops;
    for (size_t i = 0; i < min<size_t>(MÁX_MENÚ - 2, m_historial.size()); i++)
        ops.append({ corto(m_historial[i].título, 60), Acción::IrA, false, m_historial[i].dirección });
    if (m_historial.is_empty())
        ops.append({ "(aún no hay nada en el historial)", Acción::Nada, true });
    ops.append({ "-", Acción::Nada });
    ops.append({ "Borrar el historial", Acción::BorrarHistorial, m_historial.is_empty() });
    abre_menú(x_menú(m_an) + ANCHO_BOTÓN - 340, ALTO_BARRA, ETQ_HISTORIAL, move(ops));
}

void Interfaz::menú_descargas()
{
    Vector<Opción> ops;
    using St = WebView::FileDownloader::DownloadStatus;
    for (auto& d : m_descargas) {
        auto nombre = d.destino;
        if (auto k = nombre.find_last('/'); k.has_value())
            nombre = nombre.substring(*k + 1);
        ByteString estado;
        if (d.estado == St::Completed)
            estado = tamaño_de(d.hecho);
        else if (d.estado == St::Failed)
            estado = "ha fallado";
        else if (d.estado == St::Canceled)
            estado = "cancelada";
        else if (d.total.has_value() && *d.total > 0)
            estado = ByteString::formatted("{} %", d.hecho * 100 / *d.total);
        else
            estado = ByteString::formatted("{}…", tamaño_de(d.hecho));
        ops.append({ ByteString::formatted("{} ({})", corto(nombre, 50), estado), Acción::AbrirDescargas });
    }
    if (m_descargas.is_empty())
        ops.append({ "(nada descargado aún)", Acción::Nada, true });
    ops.append({ "-", Acción::Nada });
    ops.append({ "Abrir la carpeta Descargas", Acción::AbrirDescargas });
    abre_menú(x_menú(m_an) + ANCHO_BOTÓN - 340, ALTO_BARRA, ETQ_DESCARGAS, move(ops));
}

// ---- [etapa 10 i] los cuadros ----

void Interfaz::pide_cuadro(Petición c)
{
    m_cuadros.append(move(c));
    if (!m_cuadro_abierto)
        siguiente_cuadro();
}

void Interfaz::siguiente_cuadro()
{
    if (m_cuadros.is_empty()) {
        m_cuadro_abierto = false;
        return;
    }
    auto& c = m_cuadros.first();
    // (si es de otra pestaña, a esa)
    for (size_t i = 0; i < m_pestañas.size(); i++) {
        if (m_pestañas[i]->vista == c.vista && i != m_activa)
            cambia_a(i);
    }
    auto* p = pestaña(c.vista);
    ByteString sitio = "la página";
    if (p) {
        if (auto u = url_de_texto(p->dirección); u.has_value() && !u->serialized_host().is_empty())
            sitio = u->serialized_host();
    }
    switch (c.clase) {
    case Cuadro::Aviso:
        nv_cuadro_abre(NV_C_AVISO, ByteString::formatted("Mensaje de {}", sitio).characters(), c.mensaje.characters(), nullptr, nullptr);
        break;
    case Cuadro::Confirma:
        nv_cuadro_abre(NV_C_CONFIRMA, ByteString::formatted("Pregunta de {}", sitio).characters(), c.mensaje.characters(), nullptr, nullptr);
        break;
    case Cuadro::Pregunta:
        nv_cuadro_abre(NV_C_PREGUNTA, ByteString::formatted("Pregunta de {}", sitio).characters(), c.mensaje.characters(), c.valor.characters(), nullptr);
        break;
    case Cuadro::AntesDeSalir:
        nv_cuadro_abre(NV_C_SALIR, "¿Salir de la página?", "Esta página pregunta si quieres salir: puede que se pierda lo que hayas escrito en ella.", nullptr, nullptr);
        break;
    case Cuadro::Archivo:
        nv_cuadro_abre(NV_C_ARCHIVO, "Elegir un archivo para subirlo a la página", nullptr, nullptr, m_carpeta_subir.characters());
        break;
    }
    m_cuadro_abierto = true;
    // (para las pruebas)
    outln("NAVEGADOR: cuadro «{}»{}{}", c.clase == Cuadro::Aviso ? "aviso"sv : c.clase == Cuadro::Confirma ? "confirmar"sv : c.clase == Cuadro::Pregunta ? "pregunta"sv : c.clase == Cuadro::AntesDeSalir ? "salir"sv : "archivo"sv,
        c.mensaje.is_empty() ? ""sv : ": "sv, c.mensaje);
    if (p)
        m_hub->async_view_set_has_system_focus(p->vista, false);
    m_página_sucia = true;
    pide_pintar();
}

void Interfaz::cuadro_suceso(nv_suceso const& s)
{
    int alto = m_al - ALTO_BARRA;
    if (s.clase == NV_BAJA || s.clase == NV_SUBE || s.clase == NV_MUEVE || s.clase == NV_RUEDA) {
        if (s.y < ALTO_BARRA)
            return;
    }
    if (nv_cuadro_suceso(m_v, &s, ALTO_BARRA, m_an, alto)) {
        m_página_sucia = true;
        pide_pintar();
    }
    char valor[2048];
    int r = nv_cuadro_cerrado(valor, sizeof valor);
    if (r < 0 || m_cuadros.is_empty())
        return;
    auto c = m_cuadros.take_first();
    outln("NAVEGADOR: cuadro cerrado ({}){}{}", r ? "aceptado"sv : "cancelado"sv, (r && (c.clase == Cuadro::Pregunta || c.clase == Cuadro::Archivo)) ? ": "sv : ""sv,
        (r && (c.clase == Cuadro::Pregunta || c.clase == Cuadro::Archivo)) ? StringView { valor, strlen(valor) } : ""sv);
    switch (c.clase) {
    case Cuadro::Aviso:
        m_hub->async_view_alert_closed(c.vista);
        break;
    case Cuadro::Confirma:
        m_hub->async_view_confirm_closed(c.vista, r == 1);
        break;
    case Cuadro::Pregunta:
        if (r == 1)
            m_hub->async_view_prompt_closed(c.vista, Utf16String::from_utf8_without_validation(StringView { valor, strlen(valor) }));
        else
            m_hub->async_view_prompt_closed(c.vista, {});
        break;
    case Cuadro::AntesDeSalir:
        m_hub->async_view_answer(c.vista, c.respuesta, r == 1);
        break;
    case Cuadro::Archivo: {
        Vector<Web::HTML::SelectedFile> elegidos;
        if (r == 1) {
            ByteString ruta { valor, strlen(valor) };
            if (auto k = ruta.find_last('/'); k.has_value())
                m_carpeta_subir = *k == 0 ? ByteString("/") : ruta.substring(0, *k);
            int fd = open(ruta.characters(), O_RDONLY | O_CLOEXEC);
            if (fd >= 0) {
                auto nombre = ruta;
                if (auto k = nombre.find_last('/'); k.has_value())
                    nombre = nombre.substring(*k + 1);
                elegidos.append(Web::HTML::SelectedFile(Utf16String::from_utf8_without_validation(nombre), IPC::File::adopt_fd(fd)));
            } else {
                avisa(ByteString::formatted("No se puede abrir {}: {}", ruta, strerror(errno)), true);
            }
        }
        m_hub->async_view_file_picker_closed(c.vista, move(elegidos));
        break;
    }
    }
    m_cuadro_abierto = false;
    if (auto* p = activa())
        m_hub->async_view_set_has_system_focus(p->vista, !m_editando && !m_busca_foco);
    m_página_sucia = true;
    pide_pintar();
    if (!m_cuadros.is_empty())
        siguiente_cuadro();
}

// ---- el teclado ----

void Interfaz::tecla(nv_suceso const& s)
{
    auto t = s.tecla;
    auto ctrl = (s.mods & NV_CON_CONTROL) != 0;
    auto* p = activa();
    // [etapa 10 i] con un cuadro abierto, todo para él
    if (m_cuadro_abierto) {
        cuadro_suceso(s);
        return;
    }
    // lo del navegador
    if (ctrl && (t == 12 || t == 'l')) {           // Ctrl+L: la barra de direcciones
        m_editando = true;
        m_busca_foco = false;
        m_texto.clear();
        if (p) {
            if (p->dirección.bytes_as_string_view() != StringView { INICIO_PROPIO, strlen(INICIO_PROPIO) }) {
                for (auto c : Utf8View(p->dirección.bytes_as_string_view()))
                    m_texto.append(c);
            }
            m_hub->async_view_set_has_system_focus(p->vista, false);
        }
        m_cursor = m_texto.size();
        m_todo_marcado = true;
        ha_cambiado_la_barra();
        return;
    }
    if (ctrl && (t == 20 || t == 't')) {           // Ctrl+T: una pestaña con la página de inicio, con la barra para escribir
        abre_pestaña(url_de(INICIO_PROPIO), true);
        m_editando = true;
        m_texto.clear();
        m_cursor = 0;
        m_todo_marcado = false;
        if (auto* q = activa())
            m_hub->async_view_set_has_system_focus(q->vista, false);
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
    // [etapa 10 i] buscar, los marcadores, el historial, el zoom
    if (p && ctrl && (t == 6 || t == 'f')) {       // Ctrl+F
        abre_busca();
        return;
    }
    if (p && (t == NV_T_F3 || (ctrl && (t == 7 || t == 'g')))) {   // F3, Ctrl+G: la siguiente
        if (!m_buscando)
            abre_busca();
        else
            busca(true, (s.mods & NV_CON_MAYUS) != 0);
        return;
    }
    if (ctrl && (t == 4 || t == 'd')) {            // Ctrl+D: a los marcadores (o fuera)
        marca_o_desmarca();
        return;
    }
    if (ctrl && (t == 2 || t == 'b')) {            // Ctrl+B: los marcadores
        menú_marcadores();
        return;
    }
    if (ctrl && (t == 8 || t == 'h')) {            // Ctrl+H: el historial
        menú_historial();
        return;
    }
    if (p && ctrl && (t == '+' || t == '=')) {
        m_hub->async_view_zoom_in(p->vista);
        return;
    }
    if (p && ctrl && t == '-') {
        m_hub->async_view_zoom_out(p->vista);
        return;
    }
    if (p && ctrl && t == '0') {
        m_hub->async_view_reset_zoom(p->vista);
        return;
    }
    if (m_editando) {
        tecla_en_barra(s);
        return;
    }
    if (m_buscando && m_busca_foco) {
        tecla_en_busca(s);
        return;
    }
    if (!p)
        return;
    if (m_buscando && t == 27) {
        cierra_busca();
        return;
    }
    // [etapa 10 i] copiar, cortar, pegar, todo, deshacer y rehacer, como la interfaz de Qt
    if (ctrl && t == 3) {
        m_hub->async_view_request_selected_text(p->vista, m_siguiente_petición++);
        return;
    }
    if (ctrl && t == 24) {
        m_hub->async_view_cut_selection(p->vista);
        return;
    }
    if (ctrl && t == 22) {
        m_hub->async_view_paste(p->vista, Web::Clipboard::PasteMode::PlainTextOnly);
        return;
    }
    if (ctrl && t == 1) {
        m_hub->async_view_select_all(p->vista);
        return;
    }
    if (ctrl && t == 26) {
        m_hub->async_view_undo(p->vista);
        return;
    }
    if (ctrl && t == 25) {
        m_hub->async_view_redo(p->vista);
        return;
    }
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

// ---- el ratón ----

void Interfaz::ratón(nv_suceso const& s)
{
    auto* p = activa();
    auto antes = m_botones;
    if (s.clase == NV_BAJA || s.clase == NV_SUBE || s.clase == NV_MUEVE)
        m_botones = s.botones;
    // [etapa 10 i] con un cuadro abierto, todo para él
    if (m_cuadro_abierto) {
        cuadro_suceso(s);
        return;
    }
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
                nv_suceso t {};
                t.clase = NV_TECLA;
                t.tecla = 20;
                t.mods = NV_CON_CONTROL;
                tecla(t);
            }
            return;
        }
        int b = (s.x - 8) / ANCHO_BOTÓN;
        if (s.x >= 8 && b >= 0 && b < N_BOTONES) {
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
            else if (b == 3)
                ir(INICIO_PROPIO);
            return;
        }
        // [etapa 10 i] ☆ y ☰
        if (s.x >= x_menú(m_an) && s.x < x_menú(m_an) + ANCHO_BOTÓN) {
            menú_principal();
            return;
        }
        if (s.x >= x_estrella(m_an) && s.x < x_estrella(m_an) + ANCHO_BOTÓN) {
            menú_marcadores();
            return;
        }
        if (s.x >= x_dirección() && s.x < x_estrella(m_an)) {
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
    // [etapa 10 i] lo de abajo: la barra de buscar y el aviso
    int y_busca = m_al - (m_buscando ? ALTO_BUSCA : 0);
    int y_aviso = y_busca - (m_aviso_texto.has_value() ? ALTO_AVISO : 0);
    if (s.clase != NV_RUEDA && s.y >= y_aviso && (m_buscando || m_aviso_texto.has_value())) {
        if (s.clase != NV_BAJA || !(s.botones & NV_BOTON_IZQ & ~antes))
            return;
        if (s.y < y_busca) {
            if (s.x >= m_an - 36) {
                m_aviso_texto = {};
                m_página_sucia = true;
                pide_pintar();
            }
            return;
        }
        int fx = 12 + nv_ancho("Buscar en la página:", 0, TAM) + 10, bx = fx + 320 + 10;
        if (s.x >= m_an - 40) {
            cierra_busca();
        } else if (s.x >= bx && s.x < bx + ANCHO_BOTÓN) {
            busca(true, true);
        } else if (s.x >= bx + ANCHO_BOTÓN && s.x < bx + 2 * ANCHO_BOTÓN) {
            busca(true, false);
        } else if (s.x >= fx && s.x < fx + 320) {
            m_busca_foco = true;
            m_editando = false;
            m_hub->async_view_set_has_system_focus(p->vista, false);
            m_página_sucia = true;
            ha_cambiado_la_barra();
        }
        return;
    }
    if ((m_editando || m_busca_foco) && s.clase == NV_BAJA) {
        m_editando = false;
        m_busca_foco = false;
        m_hub->async_view_set_has_system_focus(p->vista, true);
        m_página_sucia = true;
        ha_cambiado_la_barra();
    }
    // [etapa 10 i] Ctrl y la rueda: el zoom
    if (s.clase == NV_RUEDA && ((s.mods & NV_CON_CONTROL) || nv_control())) {
        if (s.pasos > 0)
            m_hub->async_view_zoom_in(p->vista);
        else if (s.pasos < 0)
            m_hub->async_view_zoom_out(p->vista);
        return;
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
            m_hub->async_view_set_has_system_focus(p->vista, s.foco && !m_editando && !m_busca_foco && !m_cuadro_abierto);
        break;
    case NV_MENU:
        // [etapa 10 i] lo elegido en un menú (o en el de un cuadro)
        if (m_cuadro_abierto)
            cuadro_suceso(s);
        else
            menú_elegido(s);
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
    // [etapa 10 i] los marcadores, el historial y la página de inicio
    lee_sitios();
    escribe_inicio();
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
        abre_pestaña(url_de(INICIO), true);
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
