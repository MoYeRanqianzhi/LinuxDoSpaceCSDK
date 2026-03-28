# LinuxDoSpace C SDK

`sdk/c` provides a C implementation of the LinuxDoSpace mail-stream client model.

## Scope

This SDK implements the protocol and local dispatch semantics described in:

- `https://github.com/MoYeRanqianzhi/LinuxDoSpace/blob/main/sdk/spec/MAIL_STREAM_PROTOCOL.md`

Current core coverage:

- `Client` lifecycle: create, close, destroy
- `MailMessage` model with stable string/raw-byte fields
- Full stream queue (`lds_client_listen_next`)
- Local mailbox bindings:
  - exact prefix + suffix
  - lightweight pattern + suffix
- Ordered matching chain per suffix
- `allow_overlap` short-circuit behavior

Important:

- `LDS_SUFFIX_LINUXDO_SPACE` is semantic, not literal
- the canonical default binding resolves to
  `<owner_username>-mail.linuxdo.space`
- `lds_client_bind_*_linuxdo_space(..., mail_suffix_fragment, ...)` resolves
  dynamic namespaces like `<owner_username>-mailfoo.linuxdo.space`
- the legacy default alias `<owner_username>.linuxdo.space` still matches the
  default semantic binding automatically
- local route query (`lds_client_route`)
- mailbox close and queue lifecycle

## Important behavior

- Binding metadata is registered immediately when `bind_*` succeeds.
- Mailbox queue is activated on first `lds_mailbox_listen_next` call.
- Messages arriving before mailbox queue activation are not backfilled.
- Exact and regex bindings share one creation-order chain.
- The C SDK pattern matcher is intentionally lightweight, not a full PCRE/ECMAScript regex engine.

## Build

Requirements:

- C11 compiler
- CMake 3.16+

Build steps:

```bash
cmake -S . -B build
cmake --build build
```

## Example

Example source:

- `examples/simple.c`

Run (after build):

```bash
./build/linuxdospace_c_example
```

## Dependencies

- No external HTTP dependency is required for current core behavior.
- The current implementation focuses on stream event ingestion and local routing (`lds_client_ingest_ndjson_line`), which is the core reusable part shared across transports.

## Verification status (this environment)

- Will be compiled locally with `gcc` in this task.
- Runtime network streaming via HTTPS is intentionally transport-agnostic in this iteration.
