#pragma once
#include <pebble.h>
  
void smsShow(const char* destPhoneNumber);
bool smsHandleDataReceived(DictionaryIterator *received);