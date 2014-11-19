#pragma once
#include <pebble.h>
  
void smsShow(const char* destPhoneNumber, bool closeAfterSend);
bool smsHandleDataReceived(DictionaryIterator *received);