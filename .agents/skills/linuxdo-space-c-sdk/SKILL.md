---
name: linuxdo-space-c-sdk
description: Use when writing or fixing C code that consumes or maintains the LinuxDoSpace C SDK under sdk/c. Use for source integration, ndjson ingestion, full-stream consumption, mailbox bindings, allow_overlap matching, lifecycle/close semantics, release artifact guidance, and local validation.
---

# LinuxDoSpace C SDK

Read [references/consumer.md](references/consumer.md) first for normal SDK usage.
Read [references/api.md](references/api.md) for exact public C API names.
Read [references/examples.md](references/examples.md) for task-shaped snippets.
Read [references/development.md](references/development.md) only when editing `sdk/c` itself.

## Workflow

1. Treat this SDK as a transport-agnostic core, not a full built-in HTTPS client.
2. The SDK root relative to this `SKILL.md` is `../../../`.
3. Preserve these invariants:
   - one `lds_client` models one logical stream state
   - `lds_client_ingest_ndjson_line(...)` is the upstream ingestion entrypoint
   - `lds_client_listen_next(...)` is the full-stream consumer entrypoint
   - `lds_mailbox_listen_next(...)` is the mailbox consumer entrypoint
   - `LDS_SUFFIX_LINUXDO_SPACE` is semantic and resolves after `ready.owner_username`
   - exact and regex bindings share one ordered chain per suffix
   - `allow_overlap=0` stops at first match; non-zero continues
   - mailbox queues activate only after first `lds_mailbox_listen_next(...)`
   - callers must free consumed messages with `lds_message_free(...)`
   - `lds_client_close(...)` is logical close; `lds_client_destroy(...)` frees memory
4. If behavior changes, update `../../../README.md`, headers, implementation, and example/test workflow expectations together.
5. Validate with the commands in `references/development.md`.

## Do Not Regress

- Do not add hidden pre-listen mailbox buffering.
- Do not document PCRE/ECMAScript regex support; the matcher is intentionally lightweight.
- Do not imply `lds_mailbox_close(...)` frees mailbox memory independently of client destruction.
- Do not describe this SDK as a public package-manager install; current release output is source artifacts.
