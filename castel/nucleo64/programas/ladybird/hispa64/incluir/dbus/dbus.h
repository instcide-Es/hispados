/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/incluir/dbus/dbus.h - Hispa-64, etapa 10 g (docs/64/HISPA-64.md,
 * sección 24): lo de libdbus que nombra NotificationBackendFreedesktop.cpp de
 * LibWebView (los avisos de la web, al escritorio por el bus de sesión). En
 * Hispa-64 no hay D-Bus: las funciones (hispa64/sindbus.c) no conectan
 * (dbus_bus_get_private da nulo y un error), y LibWebView se queda sin avisos
 * del escritorio. Los nombres, los tipos y los valores, los de libdbus 1.x.
 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t dbus_bool_t;
/* (libdbus los da en dbus-macros.h) */
#ifndef TRUE
#    define TRUE 1
#endif
#ifndef FALSE
#    define FALSE 0
#endif
typedef uint32_t dbus_uint32_t;
typedef int32_t dbus_int32_t;
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct DBusPendingCall DBusPendingCall;
typedef struct DBusWatch DBusWatch;
typedef struct DBusTimeout DBusTimeout;
typedef struct DBusMessageIter {
    void* hueco[16];
} DBusMessageIter;
typedef struct DBusError {
    char const* name;
    char const* message;
    unsigned int dummy;
    void* padding1;
} DBusError;
typedef enum { DBUS_BUS_SESSION, DBUS_BUS_SYSTEM, DBUS_BUS_STARTER } DBusBusType;
typedef enum { DBUS_HANDLER_RESULT_HANDLED, DBUS_HANDLER_RESULT_NOT_YET_HANDLED, DBUS_HANDLER_RESULT_NEED_MEMORY } DBusHandlerResult;
typedef enum { DBUS_DISPATCH_DATA_REMAINS, DBUS_DISPATCH_COMPLETE, DBUS_DISPATCH_NEED_MEMORY } DBusDispatchStatus;
typedef enum { DBUS_WATCH_READABLE = 1, DBUS_WATCH_WRITABLE = 2, DBUS_WATCH_ERROR = 4, DBUS_WATCH_HANGUP = 8 } DBusWatchFlags;
#define DBUS_TYPE_INVALID ((int)'\0')
#define DBUS_TYPE_BYTE ((int)'y')
#define DBUS_TYPE_BYTE_AS_STRING "y"
#define DBUS_TYPE_BOOLEAN ((int)'b')
#define DBUS_TYPE_BOOLEAN_AS_STRING "b"
#define DBUS_TYPE_INT32 ((int)'i')
#define DBUS_TYPE_UINT32 ((int)'u')
#define DBUS_TYPE_STRING ((int)'s')
#define DBUS_TYPE_STRING_AS_STRING "s"
#define DBUS_TYPE_ARRAY ((int)'a')
#define DBUS_TYPE_VARIANT ((int)'v')
#define DBUS_TYPE_STRUCT ((int)'r')
#define DBUS_TYPE_DICT_ENTRY ((int)'e')
#define DBUS_MESSAGE_TYPE_INVALID 0
#define DBUS_MESSAGE_TYPE_METHOD_CALL 1
#define DBUS_MESSAGE_TYPE_METHOD_RETURN 2
#define DBUS_MESSAGE_TYPE_ERROR 3
#define DBUS_MESSAGE_TYPE_SIGNAL 4
#define DBUS_INTERFACE_DBUS "org.freedesktop.DBus"
#define DBUS_TIMEOUT_USE_DEFAULT (-1)
typedef dbus_bool_t (*DBusAddWatchFunction)(DBusWatch*, void*);
typedef void (*DBusRemoveWatchFunction)(DBusWatch*, void*);
typedef void (*DBusWatchToggledFunction)(DBusWatch*, void*);
typedef dbus_bool_t (*DBusAddTimeoutFunction)(DBusTimeout*, void*);
typedef void (*DBusRemoveTimeoutFunction)(DBusTimeout*, void*);
typedef void (*DBusTimeoutToggledFunction)(DBusTimeout*, void*);
typedef void (*DBusFreeFunction)(void*);
typedef void (*DBusDispatchStatusFunction)(DBusConnection*, DBusDispatchStatus, void*);
typedef void (*DBusPendingCallNotifyFunction)(DBusPendingCall*, void*);
typedef DBusHandlerResult (*DBusHandleMessageFunction)(DBusConnection*, DBusMessage*, void*);

DBusConnection* dbus_bus_get_private(DBusBusType, DBusError*);
void dbus_bus_add_match(DBusConnection*, char const*, DBusError*);
void dbus_connection_set_exit_on_disconnect(DBusConnection*, dbus_bool_t);
dbus_bool_t dbus_connection_set_watch_functions(DBusConnection*, DBusAddWatchFunction, DBusRemoveWatchFunction, DBusWatchToggledFunction, void*, DBusFreeFunction);
dbus_bool_t dbus_connection_set_timeout_functions(DBusConnection*, DBusAddTimeoutFunction, DBusRemoveTimeoutFunction, DBusTimeoutToggledFunction, void*, DBusFreeFunction);
void dbus_connection_set_dispatch_status_function(DBusConnection*, DBusDispatchStatusFunction, void*, DBusFreeFunction);
DBusDispatchStatus dbus_connection_dispatch(DBusConnection*);
dbus_bool_t dbus_connection_add_filter(DBusConnection*, DBusHandleMessageFunction, void*, DBusFreeFunction);
dbus_bool_t dbus_connection_send(DBusConnection*, DBusMessage*, dbus_uint32_t*);
dbus_bool_t dbus_connection_send_with_reply(DBusConnection*, DBusMessage*, DBusPendingCall**, int);
void dbus_connection_close(DBusConnection*);
void dbus_connection_unref(DBusConnection*);
dbus_bool_t dbus_watch_get_enabled(DBusWatch*);
int dbus_watch_get_unix_fd(DBusWatch*);
unsigned int dbus_watch_get_flags(DBusWatch*);
dbus_bool_t dbus_watch_handle(DBusWatch*, unsigned int);
int dbus_timeout_get_interval(DBusTimeout*);
dbus_bool_t dbus_timeout_get_enabled(DBusTimeout*);
dbus_bool_t dbus_timeout_handle(DBusTimeout*);
DBusMessage* dbus_message_new_method_call(char const*, char const*, char const*, char const*);
void dbus_message_unref(DBusMessage*);
int dbus_message_get_type(DBusMessage*);
dbus_bool_t dbus_message_get_args(DBusMessage*, DBusError*, int, ...);
dbus_bool_t dbus_message_append_args(DBusMessage*, int, ...);
dbus_bool_t dbus_message_is_signal(DBusMessage*, char const*, char const*);
char const* dbus_message_get_sender(DBusMessage*);
char const* dbus_message_get_error_name(DBusMessage*);
void dbus_message_set_no_reply(DBusMessage*, dbus_bool_t);
void dbus_message_iter_init_append(DBusMessage*, DBusMessageIter*);
dbus_bool_t dbus_message_iter_init(DBusMessage*, DBusMessageIter*);
dbus_bool_t dbus_message_iter_append_basic(DBusMessageIter*, int, void const*);
dbus_bool_t dbus_message_iter_append_fixed_array(DBusMessageIter*, int, void const*, int);
dbus_bool_t dbus_message_iter_open_container(DBusMessageIter*, int, char const*, DBusMessageIter*);
dbus_bool_t dbus_message_iter_close_container(DBusMessageIter*, DBusMessageIter*);
int dbus_message_iter_get_arg_type(DBusMessageIter*);
void dbus_message_iter_recurse(DBusMessageIter*, DBusMessageIter*);
dbus_bool_t dbus_message_iter_next(DBusMessageIter*);
void dbus_message_iter_get_basic(DBusMessageIter*, void*);
void dbus_pending_call_unref(DBusPendingCall*);
DBusMessage* dbus_pending_call_steal_reply(DBusPendingCall*);
dbus_bool_t dbus_pending_call_set_notify(DBusPendingCall*, DBusPendingCallNotifyFunction, void*, DBusFreeFunction);
void dbus_error_init(DBusError*);
void dbus_error_free(DBusError*);
dbus_bool_t dbus_error_is_set(DBusError const*);
#ifdef __cplusplus
}
#endif
