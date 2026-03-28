#ifndef LINUXDOSPACE_SDK_H
#define LINUXDOSPACE_SDK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LinuxDoSpace C SDK
 *
 * Design notes:
 * - one client models one token stream
 * - bind metadata is registered immediately
 * - mailbox queue starts only after first mailbox listen call
 * - exact/regex bindings share one ordered chain
 * - allow_overlap=false stops at the first matched binding
 */

typedef struct lds_client lds_client;
typedef struct lds_mailbox lds_mailbox;
typedef struct lds_message lds_message;

typedef enum lds_error_code {
  LDS_OK = 0,
  LDS_ERR_INVALID_ARGUMENT = 1,
  LDS_ERR_NO_MEMORY = 2,
  LDS_ERR_PARSE = 3,
  LDS_ERR_NOT_FOUND = 4,
  LDS_ERR_CLOSED = 5
} lds_error_code;

typedef enum lds_mailbox_mode {
  LDS_MAILBOX_MODE_EXACT = 1,
  LDS_MAILBOX_MODE_PATTERN = 2
} lds_mailbox_mode;

typedef struct lds_client_config {
  const char *token;
  const char *base_url;
} lds_client_config;

typedef struct lds_message_view {
  const char *address;
  const char *sender;
  const char *subject;
  const char *message_id;
  const char *date;
  const char *from_header;
  const char *to_header;
  const char *cc_header;
  const char *reply_to_header;
  const char *text;
  const char *html;
  const char *received_at;
  const char *raw;
  const uint8_t *raw_bytes;
  size_t raw_bytes_len;
} lds_message_view;

/*
 * Convenience first-party namespace suffix constant.
 * This value is semantic rather than literal.
 *
 * Current canonical behavior:
 * - `LDS_SUFFIX_LINUXDO_SPACE` resolves to
 *   `<owner_username>-mail.linuxdo.space`
 * - legacy routed events for `<owner_username>.linuxdo.space` are still
 *   accepted for backward-compatible matching
 * - dynamic semantic variants such as
 *   `<owner_username>-mailfoo.linuxdo.space` are created through the explicit
 *   `*_linuxdo_space(..., "foo", ...)` helper APIs below
 */
#define LDS_SUFFIX_LINUXDO_SPACE "linuxdo.space"

/* Create one client. Token must be non-empty. */
lds_error_code lds_client_create(const lds_client_config *config, lds_client **out_client);

/* Close and release one client plus all owned mailboxes and queued messages. */
void lds_client_destroy(lds_client *client);

/* Close one client but keep pointer ownership with caller. */
void lds_client_close(lds_client *client);

/*
 * Ingest one NDJSON line from /v1/token/email/stream.
 * The function stores owner_username from "ready", ignores "heartbeat", and
 * dispatches "mail". The stored owner_username is also what semantic
 * linuxdo.space bindings use to resolve the canonical `@<owner>-mail.<root>`
 * namespace plus any explicit dynamic `-mail<suffix>` variants.
 */
lds_error_code lds_client_ingest_ndjson_line(lds_client *client, const char *line, size_t line_len);

/* Pop one message from the client full stream queue. Returns LDS_ERR_NOT_FOUND when empty. */
lds_error_code lds_client_listen_next(lds_client *client, lds_message **out_message);

/* Register one exact binding (prefix + suffix). */
lds_error_code lds_client_bind_exact(
    lds_client *client,
    const char *prefix,
    const char *suffix,
    int allow_overlap,
    lds_mailbox **out_mailbox);

/* Register one regex binding (pattern + suffix). */
lds_error_code lds_client_bind_regex(
    lds_client *client,
    const char *pattern,
    const char *suffix,
    int allow_overlap,
    lds_mailbox **out_mailbox);

/*
 * Register one exact binding against the semantic linuxdo.space mail namespace.
 * `mail_suffix_fragment` is optional:
 * - `NULL` or `""` => `<owner_username>-mail.linuxdo.space`
 * - `"foo"` => `<owner_username>-mailfoo.linuxdo.space`
 *
 * The helper keeps backward-compatible matching for legacy
 * `<owner_username>.linuxdo.space` events when the fragment is empty.
 */
lds_error_code lds_client_bind_exact_linuxdo_space(
    lds_client *client,
    const char *prefix,
    const char *mail_suffix_fragment,
    int allow_overlap,
    lds_mailbox **out_mailbox);

/*
 * Register one regex binding against the semantic linuxdo.space mail namespace.
 * `mail_suffix_fragment` follows the same rules as
 * `lds_client_bind_exact_linuxdo_space(...)`.
 */
lds_error_code lds_client_bind_regex_linuxdo_space(
    lds_client *client,
    const char *pattern,
    const char *mail_suffix_fragment,
    int allow_overlap,
    lds_mailbox **out_mailbox);

/* Unregister and close one mailbox. */
void lds_mailbox_close(lds_mailbox *mailbox);

/*
 * Activate mailbox queue and pop one message.
 * First call also starts mailbox queueing from that point onward.
 */
lds_error_code lds_mailbox_listen_next(lds_mailbox *mailbox, lds_message **out_message);

/*
 * Resolve local matches for one concrete address, in order.
 * Returns count copied into out_mailboxes (up to max_count).
 */
size_t lds_client_route(
    lds_client *client,
    const char *address,
    lds_mailbox **out_mailboxes,
    size_t max_count);

/* Extract a read-only message view. */
lds_error_code lds_message_view_get(const lds_message *message, lds_message_view *out_view);

/* Release one message object returned by listen APIs. */
void lds_message_free(lds_message *message);

#ifdef __cplusplus
}
#endif

#endif
