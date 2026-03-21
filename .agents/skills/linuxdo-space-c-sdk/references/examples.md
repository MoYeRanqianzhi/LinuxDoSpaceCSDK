# Task Templates

## Add consumer code

- Build one transport layer that reads `/v1/token/email/stream`
- Feed each NDJSON line into `lds_client_ingest_ndjson_line(...)`
- Consume either:
  - `lds_client_listen_next(...)` for the full stream
  - `lds_mailbox_listen_next(...)` for one mailbox

## Add a mailbox binding

```c
lds_mailbox *catch_all = NULL;
lds_client_bind_regex(client, ".*", LDS_SUFFIX_LINUXDO_SPACE, 1, &catch_all);
```

## Inspect a consumed message

```c
lds_message_view view = {0};
lds_message_view_get(message, &view);
printf("%s\n", view.address);
printf("%s\n", view.subject);
lds_message_free(message);
```

