# API Reference

## Paths

- SDK root: `../../../`
- Public header: `../../../include/linuxdospace/sdk.h`
- Implementation: `../../../src/sdk.c`
- Consumer README: `../../../README.md`
- Build definition: `../../../CMakeLists.txt`

## Public surface

- Types: `lds_client`, `lds_mailbox`, `lds_message`, `lds_message_view`, `lds_client_config`
- Constants: `LDS_SUFFIX_LINUXDO_SPACE`
- Client lifecycle:
  - `lds_client_create(...)`
  - `lds_client_close(...)`
  - `lds_client_destroy(...)`
- Stream / routing:
  - `lds_client_ingest_ndjson_line(...)`
  - `lds_client_listen_next(...)`
  - `lds_client_route(...)`
- Mailbox binding:
  - `lds_client_bind_exact(...)`
  - `lds_client_bind_regex(...)`
  - `lds_mailbox_listen_next(...)`
  - `lds_mailbox_close(...)`
- Message access:
  - `lds_message_view_get(...)`
  - `lds_message_free(...)`

## Semantics

- Full-stream queue and mailbox queues are separate.
- Mailbox bindings register immediately, but mailbox queues do not backfill.
- Exact and regex bindings share one ordered chain per suffix.
- `allow_overlap=0` stops at the first match.
- Regex support is lightweight, not full PCRE.
- `lds_client_close(...)` does not free the client allocation.

