#include "linuxdospace/sdk.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int str_casecmp(const char *a, const char *b) {
  size_t i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    int ca = tolower((unsigned char)a[i]);
    int cb = tolower((unsigned char)b[i]);
    if (ca != cb) {
      return ca - cb;
    }
    i++;
  }
  return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
}

static int str_ncasecmp(const char *a, const char *b, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    int ca = tolower((unsigned char)a[i]);
    int cb = tolower((unsigned char)b[i]);
    if (ca != cb) {
      return ca - cb;
    }
    if (a[i] == '\0' || b[i] == '\0') {
      break;
    }
  }
  return 0;
}

typedef struct lds_message_queue {
  lds_message **items;
  size_t count;
  size_t cap;
} lds_message_queue;

struct lds_message {
  char *address;
  char **recipients;
  size_t recipient_count;
  char *sender;
  char *subject;
  char *message_id;
  char *date;
  char *from_header;
  char *to_header;
  char *cc_header;
  char *reply_to_header;
  char *text;
  char *html;
  char *received_at;
  char *raw;
  uint8_t *raw_bytes;
  size_t raw_bytes_len;
};

struct lds_mailbox {
  struct lds_client *owner;
  lds_mailbox_mode mode;
  char *suffix;
  char *prefix;
  char *pattern;
  int allow_overlap;
  int closed;
  int queue_active;
  lds_message_queue queue;
};

typedef struct lds_binding {
  lds_mailbox *mailbox;
} lds_binding;

struct lds_client {
  char *token;
  char *base_url;
  int closed;
  lds_message_queue all_queue;
  lds_mailbox **mailboxes;
  size_t mailbox_count;
  size_t mailbox_cap;
  lds_binding *bindings;
  size_t binding_count;
  size_t binding_cap;
};

static void message_free_inner(lds_message *msg) {
  if (msg == NULL) {
    return;
  }
  free(msg->address);
  if (msg->recipients != NULL) {
    size_t i;
    for (i = 0; i < msg->recipient_count; i++) {
      free(msg->recipients[i]);
    }
    free(msg->recipients);
  }
  free(msg->sender);
  free(msg->subject);
  free(msg->message_id);
  free(msg->date);
  free(msg->from_header);
  free(msg->to_header);
  free(msg->cc_header);
  free(msg->reply_to_header);
  free(msg->text);
  free(msg->html);
  free(msg->received_at);
  free(msg->raw);
  free(msg->raw_bytes);
  free(msg);
}

static char *dup_str_range(const char *start, const char *end) {
  size_t n = (size_t)(end - start);
  char *out = (char *)malloc(n + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, start, n);
  out[n] = '\0';
  return out;
}

static char *dup_str(const char *s) {
  if (s == NULL) {
    return NULL;
  }
  size_t n = strlen(s);
  char *out = (char *)malloc(n + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, s, n + 1);
  return out;
}

static int queue_push(lds_message_queue *q, lds_message *msg) {
  if (q->count == q->cap) {
    size_t next_cap = q->cap == 0 ? 8 : q->cap * 2;
    lds_message **next = (lds_message **)realloc(q->items, sizeof(lds_message *) * next_cap);
    if (next == NULL) {
      return 0;
    }
    q->items = next;
    q->cap = next_cap;
  }
  q->items[q->count++] = msg;
  return 1;
}

static lds_message *queue_pop(lds_message_queue *q) {
  size_t i;
  lds_message *first;
  if (q->count == 0) {
    return NULL;
  }
  first = q->items[0];
  for (i = 1; i < q->count; i++) {
    q->items[i - 1] = q->items[i];
  }
  q->count--;
  return first;
}

static void queue_clear(lds_message_queue *q) {
  size_t i;
  for (i = 0; i < q->count; i++) {
    message_free_inner(q->items[i]);
  }
  free(q->items);
  q->items = NULL;
  q->count = 0;
  q->cap = 0;
}

static void trim_inplace(char *s) {
  size_t n;
  size_t start = 0;
  size_t end;
  if (s == NULL) {
    return;
  }
  n = strlen(s);
  while (start < n && isspace((unsigned char)s[start])) {
    start++;
  }
  end = n;
  while (end > start && isspace((unsigned char)s[end - 1])) {
    end--;
  }
  if (start > 0) {
    memmove(s, s + start, end - start);
  }
  s[end - start] = '\0';
}

static void lower_inplace(char *s) {
  size_t i;
  if (s == NULL) {
    return;
  }
  for (i = 0; s[i] != '\0'; i++) {
    s[i] = (char)tolower((unsigned char)s[i]);
  }
}

static const char *find_key(const char *json, const char *key) {
  size_t i;
  size_t n = strlen(key);
  for (i = 0; json[i] != '\0'; i++) {
    if (json[i] == '"' && strncmp(json + i + 1, key, n) == 0 && json[i + 1 + n] == '"') {
      return json + i;
    }
  }
  return NULL;
}

static char *extract_json_string(const char *json, const char *key) {
  const char *p = find_key(json, key);
  const char *q;
  const char *vstart;
  const char *vend;
  char *out;
  size_t i, j;
  if (p == NULL) {
    return dup_str("");
  }
  q = strchr(p, ':');
  if (q == NULL) {
    return dup_str("");
  }
  while (*q != '\0' && *q != '"') {
    q++;
  }
  if (*q != '"') {
    return dup_str("");
  }
  vstart = q + 1;
  vend = vstart;
  while (*vend != '\0') {
    if (*vend == '"' && (vend == vstart || *(vend - 1) != '\\')) {
      break;
    }
    vend++;
  }
  out = dup_str_range(vstart, vend);
  if (out == NULL) {
    return NULL;
  }
  /* Minimal unescape for common quote and backslash cases. */
  for (i = 0, j = 0; out[i] != '\0'; i++, j++) {
    if (out[i] == '\\' && (out[i + 1] == '\\' || out[i + 1] == '"')) {
      i++;
    }
    out[j] = out[i];
  }
  out[j] = '\0';
  return out;
}

static char *extract_json_first_array_string(const char *json, const char *key) {
  const char *p = find_key(json, key);
  const char *q;
  if (p == NULL) {
    return dup_str("");
  }
  q = strchr(p, '[');
  if (q == NULL) {
    return dup_str("");
  }
  while (*q != '\0' && *q != '"') {
    if (*q == ']') {
      return dup_str("");
    }
    q++;
  }
  if (*q != '"') {
    return dup_str("");
  }
  {
    const char *vstart = q + 1;
    const char *vend = vstart;
    while (*vend != '\0') {
      if (*vend == '"' && *(vend - 1) != '\\') {
        break;
      }
      vend++;
    }
    return dup_str_range(vstart, vend);
  }
}

static int extract_json_array_strings(const char *json, const char *key, char ***out_values, size_t *out_count) {
  const char *p = find_key(json, key);
  const char *q;
  char **items = NULL;
  size_t count = 0;
  size_t cap = 0;
  if (p == NULL) {
    *out_values = NULL;
    *out_count = 0;
    return 1;
  }
  q = strchr(p, '[');
  if (q == NULL) {
    return 0;
  }
  q++;
  while (*q != '\0') {
    while (*q != '\0' && *q != '"' && *q != ']') {
      q++;
    }
    if (*q == ']') {
      break;
    }
    if (*q != '"') {
      break;
    }
    {
      const char *vstart = q + 1;
      const char *vend = vstart;
      char *item;
      while (*vend != '\0') {
        if (*vend == '"' && *(vend - 1) != '\\') {
          break;
        }
        vend++;
      }
      item = dup_str_range(vstart, vend);
      if (item == NULL) {
        size_t i;
        for (i = 0; i < count; i++) {
          free(items[i]);
        }
        free(items);
        return 0;
      }
      if (count == cap) {
        size_t next_cap = cap == 0 ? 4 : cap * 2;
        char **next_items = (char **)realloc(items, sizeof(char *) * next_cap);
        if (next_items == NULL) {
          size_t i;
          free(item);
          for (i = 0; i < count; i++) {
            free(items[i]);
          }
          free(items);
          return 0;
        }
        items = next_items;
        cap = next_cap;
      }
      trim_inplace(item);
      lower_inplace(item);
      items[count++] = item;
      q = (*vend == '\0') ? vend : vend + 1;
    }
  }
  *out_values = items;
  *out_count = count;
  return 1;
}

static int b64_val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  if (c == '=') return -2;
  return -1;
}

static int base64_decode(const char *in, uint8_t **out_bytes, size_t *out_len) {
  size_t n = strlen(in);
  size_t i = 0;
  size_t o = 0;
  uint8_t *buf = (uint8_t *)malloc((n / 4 + 1) * 3);
  if (buf == NULL) {
    return 0;
  }
  while (i < n) {
    int a, b, c, d;
    while (i < n && isspace((unsigned char)in[i])) i++;
    if (i >= n) break;
    if (i + 3 >= n) {
      free(buf);
      return 0;
    }
    a = b64_val(in[i++]);
    b = b64_val(in[i++]);
    c = b64_val(in[i++]);
    d = b64_val(in[i++]);
    if (a < 0 || b < 0 || c == -1 || d == -1) {
      free(buf);
      return 0;
    }
    buf[o++] = (uint8_t)((a << 2) | (b >> 4));
    if (c != -2) {
      buf[o++] = (uint8_t)(((b & 15) << 4) | (c >> 2));
    }
    if (d != -2 && c != -2) {
      buf[o++] = (uint8_t)(((c & 3) << 6) | d);
    }
  }
  *out_bytes = buf;
  *out_len = o;
  return 1;
}

static char *extract_header_value(const char *raw, const char *key) {
  size_t key_len = strlen(key);
  const char *line = raw;
  while (*line != '\0') {
    const char *line_end = strstr(line, "\n");
    if (line_end == NULL) {
      line_end = line + strlen(line);
    }
    if (line_end == line || (line_end == line + 1 && line[0] == '\r')) {
      break;
    }
    if (str_ncasecmp(line, key, key_len) == 0 && line[key_len] == ':') {
      const char *v = line + key_len + 1;
      while (v < line_end && isspace((unsigned char)*v)) {
        v++;
      }
      {
        char *out = dup_str_range(v, line_end);
        if (out == NULL) {
          return NULL;
        }
        trim_inplace(out);
        return out;
      }
    }
    line = (*line_end == '\0') ? line_end : line_end + 1;
  }
  return dup_str("");
}

static char *extract_body(const char *raw) {
  const char *sep = strstr(raw, "\r\n\r\n");
  if (sep != NULL) {
    return dup_str(sep + 4);
  }
  sep = strstr(raw, "\n\n");
  if (sep != NULL) {
    return dup_str(sep + 2);
  }
  return dup_str("");
}

static lds_message *clone_message(const lds_message *src) {
  lds_message *msg = (lds_message *)calloc(1, sizeof(lds_message));
  size_t i;
  if (msg == NULL) {
    return NULL;
  }
  msg->address = dup_str(src->address);
  if (src->recipient_count > 0) {
    msg->recipients = (char **)calloc(src->recipient_count, sizeof(char *));
    if (msg->recipients == NULL) {
      message_free_inner(msg);
      return NULL;
    }
    msg->recipient_count = src->recipient_count;
    for (i = 0; i < src->recipient_count; i++) {
      msg->recipients[i] = dup_str(src->recipients[i]);
      if (msg->recipients[i] == NULL) {
        message_free_inner(msg);
        return NULL;
      }
    }
  }
  msg->sender = dup_str(src->sender);
  msg->subject = dup_str(src->subject);
  msg->message_id = dup_str(src->message_id);
  msg->date = dup_str(src->date);
  msg->from_header = dup_str(src->from_header);
  msg->to_header = dup_str(src->to_header);
  msg->cc_header = dup_str(src->cc_header);
  msg->reply_to_header = dup_str(src->reply_to_header);
  msg->text = dup_str(src->text);
  msg->html = dup_str(src->html);
  msg->received_at = dup_str(src->received_at);
  msg->raw = dup_str(src->raw);
  msg->raw_bytes = (uint8_t *)malloc(src->raw_bytes_len);
  msg->raw_bytes_len = src->raw_bytes_len;
  if (msg->raw_bytes != NULL && src->raw_bytes_len > 0) {
    memcpy(msg->raw_bytes, src->raw_bytes, src->raw_bytes_len);
  }
  if (msg->address == NULL || msg->sender == NULL || msg->subject == NULL ||
      msg->message_id == NULL || msg->date == NULL || msg->from_header == NULL ||
      msg->to_header == NULL || msg->cc_header == NULL || msg->reply_to_header == NULL ||
      msg->text == NULL || msg->html == NULL || msg->received_at == NULL ||
      msg->raw == NULL || (src->raw_bytes_len > 0 && msg->raw_bytes == NULL)) {
    message_free_inner(msg);
    return NULL;
  }
  return msg;
}

/*
 * Small regex matcher used by the C SDK to avoid external regex dependencies.
 * Supported features:
 * - literal characters
 * - '.' any character
 * - '*' zero or more for the previous token
 * - '^' and '$' anchors
 * - escaped '\d' (digit), '\w' (word), and escaped literals
 */
static int pattern_char_matches(char pattern_ch, int escaped, char input_ch) {
  if (!escaped && pattern_ch == '.') {
    return input_ch != '\0';
  }
  if (escaped && pattern_ch == 'd') {
    return input_ch != '\0' && isdigit((unsigned char)input_ch);
  }
  if (escaped && pattern_ch == 'w') {
    return input_ch != '\0' && (isalnum((unsigned char)input_ch) || input_ch == '_');
  }
  return input_ch != '\0' && pattern_ch == input_ch;
}

static int regex_match_here(const char *p, const char *s);

static int regex_match_star(char token, int escaped, const char *p, const char *s) {
  const char *cur = s;
  if (regex_match_here(p, cur)) {
    return 1;
  }
  while (*cur != '\0' && pattern_char_matches(token, escaped, *cur)) {
    cur++;
    if (regex_match_here(p, cur)) {
      return 1;
    }
  }
  return 0;
}

static int regex_match_here(const char *p, const char *s) {
  int escaped;
  char token;
  if (*p == '\0') {
    return *s == '\0';
  }
  if (*p == '$' && *(p + 1) == '\0') {
    return *s == '\0';
  }
  escaped = (*p == '\\' && *(p + 1) != '\0') ? 1 : 0;
  token = escaped ? *(p + 1) : *p;
  if ((escaped && *(p + 2) == '*') || (!escaped && *(p + 1) == '*')) {
    const char *next = escaped ? p + 3 : p + 2;
    return regex_match_star(token, escaped, next, s);
  }
  if (pattern_char_matches(token, escaped, *s)) {
    return regex_match_here(escaped ? p + 2 : p + 1, s + 1);
  }
  return 0;
}

static int regex_fullmatch_simple(const char *pattern, const char *text) {
  if (pattern == NULL || text == NULL) {
    return 0;
  }
  if (pattern[0] == '^') {
    return regex_match_here(pattern + 1, text);
  }
  return regex_match_here(pattern, text);
}

static int mailbox_matches(const lds_mailbox *mailbox, const char *local_part, const char *suffix) {
  if (mailbox->closed) {
    return 0;
  }
  if (str_casecmp(mailbox->suffix, suffix) != 0) {
    return 0;
  }
  if (mailbox->mode == LDS_MAILBOX_MODE_EXACT) {
    return str_casecmp(mailbox->prefix, local_part) == 0;
  }
  return regex_fullmatch_simple(mailbox->pattern, local_part);
}

static void split_address(const char *address, char **local_part, char **suffix) {
  const char *at = strchr(address, '@');
  if (at == NULL || at == address || *(at + 1) == '\0') {
    *local_part = dup_str("");
    *suffix = dup_str("");
    return;
  }
  *local_part = dup_str_range(address, at);
  *suffix = dup_str(at + 1);
  if (*local_part != NULL) {
    lower_inplace(*local_part);
  }
  if (*suffix != NULL) {
    lower_inplace(*suffix);
  }
}

static lds_error_code dispatch_message(lds_client *client, const lds_message *base_msg) {
  size_t i;
  size_t recipient_index;
  lds_message *for_all = clone_message(base_msg);
  if (for_all == NULL) {
    return LDS_ERR_NO_MEMORY;
  }
  if (!queue_push(&client->all_queue, for_all)) {
    message_free_inner(for_all);
    return LDS_ERR_NO_MEMORY;
  }

  for (recipient_index = 0; recipient_index < base_msg->recipient_count; recipient_index++) {
    char *local = NULL;
    char *suffix = NULL;
    const char *recipient = base_msg->recipients[recipient_index];
    int seen_recipient = 0;
    for (i = 0; i < recipient_index; i++) {
      if (strcmp(base_msg->recipients[i], recipient) == 0) {
        seen_recipient = 1;
        break;
      }
    }
    if (seen_recipient) {
      continue;
    }
    split_address(recipient, &local, &suffix);
    if (local == NULL || suffix == NULL) {
      free(local);
      free(suffix);
      return LDS_ERR_NO_MEMORY;
    }
    for (i = 0; i < client->binding_count; i++) {
      lds_mailbox *mb = client->bindings[i].mailbox;
      if (!mailbox_matches(mb, local, suffix)) {
        continue;
      }
      if (mb->queue_active && !mb->closed) {
        lds_message *copy = clone_message(base_msg);
        if (copy != NULL) {
          free(copy->address);
          copy->address = dup_str(recipient);
        }
        if (copy == NULL || copy->address == NULL || !queue_push(&mb->queue, copy)) {
          if (copy != NULL) {
            message_free_inner(copy);
          }
          free(local);
          free(suffix);
          return LDS_ERR_NO_MEMORY;
        }
      }
      if (!mb->allow_overlap) {
        break;
      }
    }
    free(local);
    free(suffix);
  }
  return LDS_OK;
}

static lds_error_code parse_mail_event(const char *line, lds_message **out_msg) {
  char *sender = extract_json_string(line, "original_envelope_from");
  char *received_at = extract_json_string(line, "received_at");
  char *b64 = extract_json_string(line, "raw_message_base64");
  char **recipients = NULL;
  size_t recipient_count = 0;
  uint8_t *raw_bytes = NULL;
  size_t raw_bytes_len = 0;
  lds_message *msg;
  char *raw_utf8;
  if (sender == NULL || received_at == NULL || b64 == NULL) {
    free(sender); free(received_at); free(b64);
    return LDS_ERR_NO_MEMORY;
  }
  if (!extract_json_array_strings(line, "original_recipients", &recipients, &recipient_count)) {
    free(sender); free(received_at); free(b64);
    return LDS_ERR_NO_MEMORY;
  }
  if (recipient_count == 0) {
    size_t i;
    free(sender); free(received_at); free(b64);
    for (i = 0; i < recipient_count; i++) {
      free(recipients[i]);
    }
    free(recipients);
    return LDS_ERR_PARSE;
  }
  if (!base64_decode(b64, &raw_bytes, &raw_bytes_len)) {
    size_t i;
    free(sender); free(received_at); free(b64);
    for (i = 0; i < recipient_count; i++) {
      free(recipients[i]);
    }
    free(recipients);
    return LDS_ERR_PARSE;
  }
  raw_utf8 = (char *)malloc(raw_bytes_len + 1);
  if (raw_utf8 == NULL) {
    size_t i;
    free(sender); free(received_at); free(b64); free(raw_bytes);
    for (i = 0; i < recipient_count; i++) {
      free(recipients[i]);
    }
    free(recipients);
    return LDS_ERR_NO_MEMORY;
  }
  memcpy(raw_utf8, raw_bytes, raw_bytes_len);
  raw_utf8[raw_bytes_len] = '\0';

  msg = (lds_message *)calloc(1, sizeof(lds_message));
  if (msg == NULL) {
    size_t i;
    free(sender); free(received_at); free(b64); free(raw_bytes); free(raw_utf8);
    for (i = 0; i < recipient_count; i++) {
      free(recipients[i]);
    }
    free(recipients);
    return LDS_ERR_NO_MEMORY;
  }
  msg->address = dup_str(recipients[0]);
  msg->recipients = recipients;
  msg->recipient_count = recipient_count;
  msg->sender = sender;
  msg->received_at = received_at;
  msg->raw = raw_utf8;
  msg->raw_bytes = raw_bytes;
  msg->raw_bytes_len = raw_bytes_len;
  msg->subject = extract_header_value(raw_utf8, "Subject");
  msg->message_id = extract_header_value(raw_utf8, "Message-ID");
  msg->date = extract_header_value(raw_utf8, "Date");
  msg->from_header = extract_header_value(raw_utf8, "From");
  msg->to_header = extract_header_value(raw_utf8, "To");
  msg->cc_header = extract_header_value(raw_utf8, "Cc");
  msg->reply_to_header = extract_header_value(raw_utf8, "Reply-To");
  msg->text = extract_body(raw_utf8);
  msg->html = dup_str("");
  free(b64);
  if (msg->address == NULL || msg->subject == NULL || msg->message_id == NULL || msg->date == NULL ||
      msg->from_header == NULL || msg->to_header == NULL || msg->cc_header == NULL ||
      msg->reply_to_header == NULL || msg->text == NULL || msg->html == NULL) {
    message_free_inner(msg);
    return LDS_ERR_NO_MEMORY;
  }
  *out_msg = msg;
  return LDS_OK;
}

static int ensure_binding_cap(lds_client *client) {
  if (client->binding_count == client->binding_cap) {
    size_t next_cap = client->binding_cap == 0 ? 8 : client->binding_cap * 2;
    lds_binding *next = (lds_binding *)realloc(client->bindings, sizeof(lds_binding) * next_cap);
    if (next == NULL) {
      return 0;
    }
    client->bindings = next;
    client->binding_cap = next_cap;
  }
  return 1;
}

static int ensure_mailbox_cap(lds_client *client) {
  if (client->mailbox_count == client->mailbox_cap) {
    size_t next_cap = client->mailbox_cap == 0 ? 8 : client->mailbox_cap * 2;
    lds_mailbox **next = (lds_mailbox **)realloc(client->mailboxes, sizeof(lds_mailbox *) * next_cap);
    if (next == NULL) {
      return 0;
    }
    client->mailboxes = next;
    client->mailbox_cap = next_cap;
  }
  return 1;
}

lds_error_code lds_client_create(const lds_client_config *config, lds_client **out_client) {
  lds_client *client;
  if (config == NULL || out_client == NULL || config->token == NULL || config->token[0] == '\0') {
    return LDS_ERR_INVALID_ARGUMENT;
  }
  client = (lds_client *)calloc(1, sizeof(lds_client));
  if (client == NULL) {
    return LDS_ERR_NO_MEMORY;
  }
  client->token = dup_str(config->token);
  client->base_url = dup_str(config->base_url == NULL ? "https://api.linuxdo.space" : config->base_url);
  if (client->token == NULL || client->base_url == NULL) {
    lds_client_destroy(client);
    return LDS_ERR_NO_MEMORY;
  }
  *out_client = client;
  return LDS_OK;
}

void lds_client_destroy(lds_client *client) {
  size_t i;
  if (client == NULL) {
    return;
  }
  lds_client_close(client);
  for (i = 0; i < client->binding_count; i++) {
    lds_mailbox *mb = client->bindings[i].mailbox;
    if (mb != NULL) {
      mb->closed = 1;
      queue_clear(&mb->queue);
    }
  }
  for (i = 0; i < client->mailbox_count; i++) {
    lds_mailbox *mb = client->mailboxes[i];
    if (mb == NULL) {
      continue;
    }
    free(mb->suffix);
    free(mb->prefix);
    free(mb->pattern);
    free(mb);
  }
  free(client->mailboxes);
  free(client->bindings);
  free(client->token);
  free(client->base_url);
  free(client);
}

void lds_client_close(lds_client *client) {
  size_t i;
  if (client == NULL || client->closed) {
    return;
  }
  client->closed = 1;
  queue_clear(&client->all_queue);
  for (i = 0; i < client->binding_count; i++) {
    if (client->bindings[i].mailbox != NULL) {
      client->bindings[i].mailbox->closed = 1;
      client->bindings[i].mailbox->queue_active = 0;
      client->bindings[i].mailbox->owner = NULL;
      queue_clear(&client->bindings[i].mailbox->queue);
    }
  }
  client->binding_count = 0;
}

lds_error_code lds_client_ingest_ndjson_line(lds_client *client, const char *line, size_t line_len) {
  char *tmp;
  char *etype;
  lds_message *msg = NULL;
  lds_error_code code;
  if (client == NULL || line == NULL) {
    return LDS_ERR_INVALID_ARGUMENT;
  }
  if (client->closed) {
    return LDS_ERR_CLOSED;
  }
  tmp = (char *)malloc(line_len + 1);
  if (tmp == NULL) {
    return LDS_ERR_NO_MEMORY;
  }
  memcpy(tmp, line, line_len);
  tmp[line_len] = '\0';
  etype = extract_json_string(tmp, "type");
  if (etype == NULL) {
    free(tmp);
    return LDS_ERR_NO_MEMORY;
  }
  if (strcmp(etype, "ready") == 0 || strcmp(etype, "heartbeat") == 0) {
    free(etype);
    free(tmp);
    return LDS_OK;
  }
  if (strcmp(etype, "mail") != 0) {
    free(etype);
    free(tmp);
    return LDS_OK;
  }
  free(etype);
  code = parse_mail_event(tmp, &msg);
  free(tmp);
  if (code != LDS_OK) {
    return code;
  }
  code = dispatch_message(client, msg);
  message_free_inner(msg);
  return code;
}

lds_error_code lds_client_listen_next(lds_client *client, lds_message **out_message) {
  lds_message *msg;
  if (client == NULL || out_message == NULL) {
    return LDS_ERR_INVALID_ARGUMENT;
  }
  if (client->closed) {
    return LDS_ERR_CLOSED;
  }
  msg = queue_pop(&client->all_queue);
  if (msg == NULL) {
    return LDS_ERR_NOT_FOUND;
  }
  *out_message = msg;
  return LDS_OK;
}

static lds_error_code bind_common(
    lds_client *client,
    lds_mailbox_mode mode,
    const char *prefix_or_pattern,
    const char *suffix,
    int allow_overlap,
    lds_mailbox **out_mailbox) {
  lds_mailbox *mb;
  if (client == NULL || prefix_or_pattern == NULL || suffix == NULL || out_mailbox == NULL) {
    return LDS_ERR_INVALID_ARGUMENT;
  }
  if (client->closed) {
    return LDS_ERR_CLOSED;
  }
  if (!ensure_binding_cap(client)) {
    return LDS_ERR_NO_MEMORY;
  }
  if (!ensure_mailbox_cap(client)) {
    return LDS_ERR_NO_MEMORY;
  }
  mb = (lds_mailbox *)calloc(1, sizeof(lds_mailbox));
  if (mb == NULL) {
    return LDS_ERR_NO_MEMORY;
  }
  mb->owner = client;
  mb->mode = mode;
  mb->allow_overlap = allow_overlap ? 1 : 0;
  mb->suffix = dup_str(suffix);
  if (mb->suffix != NULL) {
    lower_inplace(mb->suffix);
  }
  if (mode == LDS_MAILBOX_MODE_EXACT) {
    mb->prefix = dup_str(prefix_or_pattern);
    if (mb->prefix != NULL) {
      trim_inplace(mb->prefix);
      lower_inplace(mb->prefix);
    }
  } else {
    mb->pattern = dup_str(prefix_or_pattern);
  }
  if (mb->suffix == NULL || (mode == LDS_MAILBOX_MODE_EXACT && mb->prefix == NULL) ||
      (mode == LDS_MAILBOX_MODE_PATTERN && mb->pattern == NULL)) {
    free(mb->suffix);
    free(mb->prefix);
    free(mb->pattern);
    free(mb);
    return LDS_ERR_NO_MEMORY;
  }
  client->mailboxes[client->mailbox_count++] = mb;
  client->bindings[client->binding_count++].mailbox = mb;
  *out_mailbox = mb;
  return LDS_OK;
}

lds_error_code lds_client_bind_exact(
    lds_client *client,
    const char *prefix,
    const char *suffix,
    int allow_overlap,
    lds_mailbox **out_mailbox) {
  return bind_common(client, LDS_MAILBOX_MODE_EXACT, prefix, suffix, allow_overlap, out_mailbox);
}

lds_error_code lds_client_bind_regex(
    lds_client *client,
    const char *pattern,
    const char *suffix,
    int allow_overlap,
    lds_mailbox **out_mailbox) {
  return bind_common(client, LDS_MAILBOX_MODE_PATTERN, pattern, suffix, allow_overlap, out_mailbox);
}

void lds_mailbox_close(lds_mailbox *mailbox) {
  size_t read_index;
  size_t write_index = 0;
  lds_client *owner;
  if (mailbox == NULL || mailbox->closed) {
    return;
  }
  mailbox->closed = 1;
  queue_clear(&mailbox->queue);
  owner = mailbox->owner;
  if (owner == NULL) {
    return;
  }
  for (read_index = 0; read_index < owner->binding_count; read_index++) {
    if (owner->bindings[read_index].mailbox != mailbox) {
      owner->bindings[write_index++] = owner->bindings[read_index];
    }
  }
  owner->binding_count = write_index;
  mailbox->owner = NULL;
}

lds_error_code lds_mailbox_listen_next(lds_mailbox *mailbox, lds_message **out_message) {
  lds_message *msg;
  if (mailbox == NULL || out_message == NULL) {
    return LDS_ERR_INVALID_ARGUMENT;
  }
  if (mailbox->closed) {
    return LDS_ERR_CLOSED;
  }
  /* First listen call activates the mailbox queue. */
  mailbox->queue_active = 1;
  msg = queue_pop(&mailbox->queue);
  if (msg == NULL) {
    return LDS_ERR_NOT_FOUND;
  }
  *out_message = msg;
  return LDS_OK;
}

size_t lds_client_route(
    lds_client *client,
    const char *address,
    lds_mailbox **out_mailboxes,
    size_t max_count) {
  char *local = NULL;
  char *suffix = NULL;
  size_t i;
  size_t written = 0;
  if (client == NULL || address == NULL || out_mailboxes == NULL || max_count == 0) {
    return 0;
  }
  if (client->closed) {
    return 0;
  }
  split_address(address, &local, &suffix);
  if (local == NULL || suffix == NULL || local[0] == '\0' || suffix[0] == '\0') {
    free(local);
    free(suffix);
    return 0;
  }
  for (i = 0; i < client->binding_count; i++) {
    lds_mailbox *mb = client->bindings[i].mailbox;
    if (!mailbox_matches(mb, local, suffix)) {
      continue;
    }
    if (written < max_count) {
      out_mailboxes[written] = mb;
      written++;
    }
    if (!mb->allow_overlap) {
      break;
    }
  }
  free(local);
  free(suffix);
  return written;
}

lds_error_code lds_message_view_get(const lds_message *message, lds_message_view *out_view) {
  if (message == NULL || out_view == NULL) {
    return LDS_ERR_INVALID_ARGUMENT;
  }
  out_view->address = message->address;
  out_view->sender = message->sender;
  out_view->subject = message->subject;
  out_view->message_id = message->message_id;
  out_view->date = message->date;
  out_view->from_header = message->from_header;
  out_view->to_header = message->to_header;
  out_view->cc_header = message->cc_header;
  out_view->reply_to_header = message->reply_to_header;
  out_view->text = message->text;
  out_view->html = message->html;
  out_view->received_at = message->received_at;
  out_view->raw = message->raw;
  out_view->raw_bytes = message->raw_bytes;
  out_view->raw_bytes_len = message->raw_bytes_len;
  return LDS_OK;
}

void lds_message_free(lds_message *message) {
  message_free_inner(message);
}
