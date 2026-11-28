/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/navegador/conexion.h - Hispa-64, etapa 10 h (docs/64/HISPA-64.md,
 * sección 25): la punta de NAVEGADOR (la interfaz) de su conexión con el Hub
 * de Ladybird (el navegador de verdad: las pestañas, el historial, los
 * procesos de cada página), por los mensajes de Services/Hub/HubServer.ipc
 * (lo que la interfaz le pide al Hub) y HubClient.ipc (lo que el Hub le
 * cuenta a la interfaz). Es lo que hace UI/Qt/HubConnection.cpp en el
 * Ladybird de Qt, sin Qt. Aquí, los mensajes que se atienden; los demás, los
 * talones vacíos de talones.py (talones.h). */
#pragma once

#include <Hub/HubClientEndpoint.h>
#include <Hub/HubServerEndpoint.h>
#include <LibIPC/ConnectionToServer.h>

namespace Navegador {

class Interfaz;

class Conexion final
    : public IPC::ConnectionToServer<HubClientEndpoint, HubServerEndpoint>
    , public HubClientEndpoint {
    C_OBJECT_ABSTRACT(Conexion)

public:
    Conexion(NonnullOwnPtr<IPC::Transport>, Interfaz&);
    void inicializada() { m_inicializada = true; }

private:
    Interfaz& m_interfaz;
    bool m_inicializada { false };

    virtual void die() override;

    // lo que se pinta: los búferes de cada página (WebView::PageBackingStores)
    virtual void view_install_backing_stores(u64 view_id, u64 page_id, Vector<i32> bitmap_ids, Vector<Gfx::SharedImage> backing_stores) override;
    virtual void view_add_backing_stores(u64 view_id, Vector<i32> bitmap_ids, Vector<Gfx::SharedImage> backing_stores) override;
    virtual void view_retire_backing_stores(u64 view_id, Vector<i32> bitmap_ids) override;
    virtual void view_present_backing_store(u64 view_id, u64 page_id, i32 bitmap_id, Gfx::IntSize size, double device_pixel_ratio) override;
    virtual void view_accept_presented_backing_store(u64 view_id, i32 bitmap_id, Gfx::IntRect damage_rect) override;
    virtual void view_keep_front_backing_store_as_backup(u64 view_id) override;
    virtual void view_clear_backup_backing_store(u64 view_id) override;
    virtual void view_clear_backing_stores(u64 view_id) override;

    // lo de cada pestaña
    virtual void view_state_changed(u64 view_id, Hub::ViewState state) override;
    virtual void view_title_change(u64 view_id, Utf16String title) override;
    virtual void view_url_change(u64 view_id, URL::URL url) override;
    virtual void view_loading_state_change(u64 view_id, bool is_loading) override;
    virtual void view_load_start(u64 view_id) override;
    virtual void view_load_finish(u64 view_id, URL::URL url) override;
    virtual void view_cursor_change(u64 view_id, Gfx::Cursor cursor) override;
    virtual void view_link_hover(u64 view_id, URL::URL url) override;
    virtual void view_link_unhover(u64 view_id) override;
    virtual void view_close(u64 view_id) override;
    virtual void view_activate_tab(u64 view_id) override;
    virtual void view_open_new_view(u64 view_id, u64 new_view_id, Hub::ViewState new_view_state, Web::HTML::ActivateTab activate_tab, Web::HTML::WebViewHints hints) override;
    virtual void app_open_url_in_new_tab(URL::URL url, Web::HTML::ActivateTab activate_tab, Optional<u64> source_view_id, WebView::IsPrivate is_private) override;
    virtual void app_open_urls_in_new_tabs(Vector<URL::URL> urls) override;
    virtual void app_open_new_tab(Vector<URL::URL> urls) override;
    virtual void app_open_view_in_new_tab(u64 view_id, Optional<u64> source_view_id, WebView::IsPrivate is_private, Web::HTML::ActivateTab activate_tab) override;
    virtual void view_web_content_crashed(u64 view_id, WebView::ViewImplementation::WebContentCrashReason reason) override;

    // lo que el Hub espera que se conteste ([etapa 10 i] con los cuadros de la oficina)
    virtual void view_request_alert(u64 view_id, Utf16String message) override;
    virtual void view_request_confirm(u64 view_id, Utf16String message) override;
    virtual void view_request_prompt(u64 view_id, Utf16String message, Utf16String default_value) override;
    virtual void view_request_before_unload(u64 view_id, u64 answer_id, String source) override;
    virtual void view_request_external_url_confirmation(u64 view_id, u64 answer_id, URL::URL url, URL::Origin initiator_origin, String application_name) override;
    virtual void view_request_permission(u64 view_id, u64 answer_id, WebView::PermissionPromptRequest request) override;
    virtual void view_request_select_dropdown(u64 view_id, Gfx::IntPoint content_position, i32 minimum_width, Vector<Web::HTML::SelectItem> items) override;
    virtual void view_request_file_picker(u64 view_id, Web::HTML::FileFilter accepted_file_types, Web::HTML::AllowMultipleFiles allow_multiple_files) override;
    virtual void view_request_color_picker(u64 view_id, Color current_color) override;
    virtual void view_request_date_picker(u64 view_id, Utf16String input_type, Utf16String value, Utf16String minimum, Utf16String maximum) override;
    virtual void view_close_preflight_finished(u64 view_id, u64 request_id, bool may_close) override;
    virtual void app_set_clipboard_text(String text, WebView::Application::ClipboardType type) override;
    virtual void app_read_clipboard_text(u64 request_id, WebView::Application::ClipboardType type) override;
    virtual void app_read_clipboard_item(u64 request_id) override;
    virtual void app_ask_user_for_download_path(u64 request_id, ByteString file) override;
    virtual void app_display_error_dialog(String message) override;
    // [etapa 10 i] (sección 26) las descargas, el menú del botón derecho, el
    // texto elegido (Ctrl+C), el historial, buscar en la página y el zoom
    virtual void app_display_download_confirmation_dialog(String download_name, ByteString path) override;
    virtual void download_added(Hub::DownloadState download) override;
    virtual void download_updated(Hub::DownloadState download) override;
    virtual void view_request_context_menu(u64 view_id, WebView::ContextMenuRequest request) override;
    virtual void view_selected_text(u64 view_id, u64 request_id, Optional<ByteString> text) override;
    virtual void view_visited_page(u64 view_id, URL::URL url, Optional<String> title, WebView::HistoryVisitTransition transition) override;
    virtual void view_visited_page_title_change(u64 view_id, URL::URL url, String title) override;
    virtual void view_find_in_page(u64 view_id, size_t current_match_index, Optional<size_t> total_match_count) override;
    virtual void view_update_zoom(u64 view_id) override;
    virtual void app_quit_for_signal() override;

public:
#include "talones.h"
};

}
