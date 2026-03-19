#include "linuxdospace/sdk.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  lds_client *client = NULL;
  lds_mailbox *alice = NULL;
  lds_message *msg = NULL;
  lds_message_view view;
  const char *mail_line =
      "{\"type\":\"mail\",\"original_envelope_from\":\"bounce@example.com\","
      "\"original_recipients\":[\"alice@linuxdo.space\"],"
      "\"received_at\":\"2026-03-20T10:11:12Z\","
      "\"raw_message_base64\":\"RnJvbTogU2VuZGVyIDxzZW5kZXJAZXhhbXBsZS5jb20+DQpUbzogQWxpY2UgPGFsaWNlQGxpbnV4ZG8uc3BhY2U+DQpTdWJqZWN0OiBUZXN0DQoNCkhlbGxvIGZyb20gQyBTREs=\"}";
  lds_client_config cfg;
  lds_error_code code;

  cfg.token = "lds_pat.demo";
  cfg.base_url = "https://api.linuxdo.space";
  code = lds_client_create(&cfg, &client);
  if (code != LDS_OK) {
    fprintf(stderr, "create failed: %d\n", (int)code);
    return 1;
  }

  code = lds_client_bind_exact(client, "alice", LDS_SUFFIX_LINUXDO_SPACE, 0, &alice);
  if (code != LDS_OK) {
    fprintf(stderr, "bind failed: %d\n", (int)code);
    lds_client_destroy(client);
    return 1;
  }

  /* Queue starts only after first mailbox listen call. */
  (void)lds_mailbox_listen_next(alice, &msg);
  if (msg != NULL) {
    lds_message_free(msg);
    msg = NULL;
  }

  code = lds_client_ingest_ndjson_line(client, mail_line, strlen(mail_line));
  if (code != LDS_OK) {
    fprintf(stderr, "ingest failed: %d\n", (int)code);
    lds_client_destroy(client);
    return 1;
  }

  code = lds_mailbox_listen_next(alice, &msg);
  if (code == LDS_OK && msg != NULL) {
    lds_message_view_get(msg, &view);
    printf("address=%s sender=%s subject=%s\n", view.address, view.sender, view.subject);
    lds_message_free(msg);
  } else {
    fprintf(stderr, "no mailbox message\n");
  }

  lds_client_destroy(client);
  return 0;
}
