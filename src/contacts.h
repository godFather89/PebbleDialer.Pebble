#pragma once

#include <pebble.h>

#define MAX_CONTACTS        10
#define CONTACTS_HISTORY    0
#define CONTACTS_LIST       1
  
void contactsShow(uint8_t type);
void contactsRefresh();
bool contactsHandleDataReceived(DictionaryIterator *received);