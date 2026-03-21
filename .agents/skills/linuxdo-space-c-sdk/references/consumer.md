# Consumer Guide

Use this SDK when your transport layer already owns the HTTPS NDJSON stream and
you need a C library for parsing and local mailbox routing.

## Build / integrate

- Preferred local build:

```bash
cmake -S . -B build
cmake --build build
```

- Minimal direct compile path:

```bash
gcc -std=c11 -Iinclude src/sdk.c examples/simple.c -o linuxdospace-c-example
```

Current release assets are source archives, so prefer source integration or
tag-pinned vendoring instead of describing this SDK as a packaged install.

## Core usage

```c
lds_client_config cfg = {0};
cfg.token = "lds_pat...";

lds_client *client = NULL;
lds_mailbox *mailbox = NULL;
lds_message *message = NULL;

lds_client_create(&cfg, &client);
lds_client_bind_exact(client, "alice", LDS_SUFFIX_LINUXDO_SPACE, 0, &mailbox);

lds_client_ingest_ndjson_line(client, line, strlen(line));

if (lds_mailbox_listen_next(mailbox, &message) == LDS_OK) {
    lds_message_view view = {0};
    lds_message_view_get(message, &view);
    puts(view.subject);
    lds_message_free(message);
}

lds_client_destroy(client);
```

## Key semantics

- `lds_client_ingest_ndjson_line(...)` is the only upstream ingestion API.
- `lds_client_listen_next(...)` consumes the full logical stream.
- `lds_mailbox_listen_next(...)` consumes one mailbox queue.
- Mailbox queues activate only on first mailbox listen call.
- `LDS_SUFFIX_LINUXDO_SPACE` is semantic, not literal.
- `lds_client_route(...)` is read-only local matching, not message replay.

