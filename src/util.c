#include "util.h"

void copyStr(char *dst, const char *src, uint8_t maxLength) {
  if (!maxLength) return;
  
  while (--maxLength) {
    *dst = *src;
    if (!*src) return;
    dst++;
    src++;
  }
  
  *dst = 0;
}

void userSendData(uint8_t data) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, 0, data);
  app_message_outbox_send();
}
