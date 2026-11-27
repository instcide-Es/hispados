/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/sindbus.c - Hispa-64, etapa 10 g (docs/64/HISPA-64.md, sección 24):
 * las funciones de hispa64/incluir/dbus/dbus.h. En Hispa-64 no hay D-Bus:
 * no se conecta (nulo y el error «org.freedesktop.DBus.Error.NotSupported»),
 * y lo demás no hace nada (no se llama sin conexión).
 */
#include <dbus/dbus.h>
#include <stddef.h>

static const char sin_bus[] = "org.freedesktop.DBus.Error.NotSupported";
static const char sin_bus_texto[] = "Hispa-64 no tiene D-Bus";

void dbus_error_init(DBusError *e) { if (e) { e->name = NULL; e->message = NULL; e->dummy = 0; e->padding1 = NULL; } }
void dbus_error_free(DBusError *e) { dbus_error_init(e); }
dbus_bool_t dbus_error_is_set(DBusError const *e) { return e && e->name; }
DBusConnection *dbus_bus_get_private(DBusBusType t, DBusError *e) { (void)t; if (e) { e->name = sin_bus; e->message = sin_bus_texto; } return NULL; }
void dbus_bus_add_match(DBusConnection *c, char const *r, DBusError *e) { (void)c; (void)r; if (e) { e->name = sin_bus; e->message = sin_bus_texto; } }
void dbus_connection_set_exit_on_disconnect(DBusConnection *c, dbus_bool_t v) { (void)c; (void)v; }
dbus_bool_t dbus_connection_set_watch_functions(DBusConnection *c, DBusAddWatchFunction a, DBusRemoveWatchFunction r, DBusWatchToggledFunction t, void *d, DBusFreeFunction f) { (void)c; (void)a; (void)r; (void)t; (void)d; (void)f; return 0; }
dbus_bool_t dbus_connection_set_timeout_functions(DBusConnection *c, DBusAddTimeoutFunction a, DBusRemoveTimeoutFunction r, DBusTimeoutToggledFunction t, void *d, DBusFreeFunction f) { (void)c; (void)a; (void)r; (void)t; (void)d; (void)f; return 0; }
void dbus_connection_set_dispatch_status_function(DBusConnection *c, DBusDispatchStatusFunction s, void *d, DBusFreeFunction f) { (void)c; (void)s; (void)d; (void)f; }
DBusDispatchStatus dbus_connection_dispatch(DBusConnection *c) { (void)c; return DBUS_DISPATCH_COMPLETE; }
dbus_bool_t dbus_connection_add_filter(DBusConnection *c, DBusHandleMessageFunction h, void *d, DBusFreeFunction f) { (void)c; (void)h; (void)d; (void)f; return 0; }
dbus_bool_t dbus_connection_send(DBusConnection *c, DBusMessage *m, dbus_uint32_t *s) { (void)c; (void)m; (void)s; return 0; }
dbus_bool_t dbus_connection_send_with_reply(DBusConnection *c, DBusMessage *m, DBusPendingCall **p, int t) { (void)c; (void)m; (void)t; if (p) *p = NULL; return 0; }
void dbus_connection_close(DBusConnection *c) { (void)c; }
void dbus_connection_unref(DBusConnection *c) { (void)c; }
dbus_bool_t dbus_watch_get_enabled(DBusWatch *w) { (void)w; return 0; }
int dbus_watch_get_unix_fd(DBusWatch *w) { (void)w; return -1; }
unsigned int dbus_watch_get_flags(DBusWatch *w) { (void)w; return 0; }
dbus_bool_t dbus_watch_handle(DBusWatch *w, unsigned int f) { (void)w; (void)f; return 0; }
int dbus_timeout_get_interval(DBusTimeout *t) { (void)t; return 0; }
dbus_bool_t dbus_timeout_get_enabled(DBusTimeout *t) { (void)t; return 0; }
dbus_bool_t dbus_timeout_handle(DBusTimeout *t) { (void)t; return 0; }
DBusMessage *dbus_message_new_method_call(char const *a, char const *b, char const *c, char const *d) { (void)a; (void)b; (void)c; (void)d; return NULL; }
void dbus_message_unref(DBusMessage *m) { (void)m; }
int dbus_message_get_type(DBusMessage *m) { (void)m; return DBUS_MESSAGE_TYPE_INVALID; }
dbus_bool_t dbus_message_get_args(DBusMessage *m, DBusError *e, int t, ...) { (void)m; (void)t; if (e) { e->name = sin_bus; e->message = sin_bus_texto; } return 0; }
dbus_bool_t dbus_message_append_args(DBusMessage *m, int t, ...) { (void)m; (void)t; return 0; }
dbus_bool_t dbus_message_is_signal(DBusMessage *m, char const *i, char const *s) { (void)m; (void)i; (void)s; return 0; }
char const *dbus_message_get_sender(DBusMessage *m) { (void)m; return NULL; }
char const *dbus_message_get_error_name(DBusMessage *m) { (void)m; return NULL; }
void dbus_message_set_no_reply(DBusMessage *m, dbus_bool_t v) { (void)m; (void)v; }
void dbus_message_iter_init_append(DBusMessage *m, DBusMessageIter *i) { (void)m; (void)i; }
dbus_bool_t dbus_message_iter_init(DBusMessage *m, DBusMessageIter *i) { (void)m; (void)i; return 0; }
dbus_bool_t dbus_message_iter_append_basic(DBusMessageIter *i, int t, void const *v) { (void)i; (void)t; (void)v; return 0; }
dbus_bool_t dbus_message_iter_append_fixed_array(DBusMessageIter *i, int t, void const *v, int n) { (void)i; (void)t; (void)v; (void)n; return 0; }
dbus_bool_t dbus_message_iter_open_container(DBusMessageIter *i, int t, char const *s, DBusMessageIter *s2) { (void)i; (void)t; (void)s; (void)s2; return 0; }
dbus_bool_t dbus_message_iter_close_container(DBusMessageIter *i, DBusMessageIter *s) { (void)i; (void)s; return 0; }
int dbus_message_iter_get_arg_type(DBusMessageIter *i) { (void)i; return DBUS_TYPE_INVALID; }
void dbus_message_iter_recurse(DBusMessageIter *i, DBusMessageIter *s) { (void)i; (void)s; }
dbus_bool_t dbus_message_iter_next(DBusMessageIter *i) { (void)i; return 0; }
void dbus_message_iter_get_basic(DBusMessageIter *i, void *v) { (void)i; (void)v; }
void dbus_pending_call_unref(DBusPendingCall *p) { (void)p; }
DBusMessage *dbus_pending_call_steal_reply(DBusPendingCall *p) { (void)p; return NULL; }
dbus_bool_t dbus_pending_call_set_notify(DBusPendingCall *p, DBusPendingCallNotifyFunction f, void *d, DBusFreeFunction l) { (void)p; (void)f; (void)d; (void)l; return 0; }
