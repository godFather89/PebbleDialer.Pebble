#include "contacts.h"
#include "sms.h"
#include "util.h"
#define MAX_NAME_LENGTH 20
#define MAX_NUMBER_LENGTH 51
  
typedef struct {
  char Name[MAX_NAME_LENGTH];
  char Number[MAX_NUMBER_LENGTH];
  uint8_t Type;
} __attribute__((__packed__)) Contact;
  
static Contact contacts[2][MAX_CONTACTS];

static bool contactsLoaded[2] = {false, false};
static uint8_t contactsCount[2] = {0,0};
static uint8_t contactsType;

static Window *contactWindow = NULL;
static MenuLayer* contactMenu;
static TextLayer *contactsTextLayer;
static GBitmap *callType[3];

static void contactsMenuCall(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, 0, 2);
  dict_write_cstring(iter, 1, contacts[contactsType][cell_index->row].Number);
  app_message_outbox_send();
  
  window_stack_pop_all(true);
}

static void contactsMenuSms(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if (contactsLoaded[contactsType] && contactsCount[contactsType])
    smsShow(contacts[contactsType][cell_index->row].Number);
}

static void contactsMenuDrawRow(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context) { 
  Contact *contact = &contacts[contactsType][cell_index->row]; 
  menu_cell_basic_draw(ctx, cell_layer, contact->Name, contact->Number,
                       contact->Type < sizeof(callType) ? callType[contact->Type] : NULL);
}

static uint16_t contactsMenuGetCount(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
	return contactsCount[contactsType];
}

static void contactsWindowLoad(Window *window) {
  GRect frame = layer_get_bounds(window_get_root_layer(window));
  contactMenu = menu_layer_create(GRect(0,0,frame.size.w,frame.size.h));
  
  MenuLayerCallbacks cbacks = {
    .get_num_rows = &contactsMenuGetCount,
	  .select_click = &contactsMenuCall,
    .select_long_click = &contactsMenuSms,
	  .draw_row = &contactsMenuDrawRow,
  };
  menu_layer_set_callbacks(contactMenu, NULL, cbacks); 
  menu_layer_set_click_config_onto_window(contactMenu, window);
	layer_add_child(window_get_root_layer(window), menu_layer_get_layer(contactMenu));
  
  contactsTextLayer = text_layer_create(GRect(0, frame.size.h/2 - 30, frame.size.w, 70));
  text_layer_set_font(contactsTextLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(contactsTextLayer, "Loading\nContacts");
  text_layer_set_text_alignment(contactsTextLayer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(contactsTextLayer, GTextOverflowModeWordWrap);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(contactsTextLayer));
  if (contactsLoaded[contactsType]) {
    if (contactsCount[contactsType] == 0)
      text_layer_set_text(contactsTextLayer, "No\nContacts");
    else
      layer_set_hidden(text_layer_get_layer(contactsTextLayer), true);
  }
  
  if (contactsType == CONTACTS_HISTORY) {
    callType[0] = gbitmap_create_with_resource(RESOURCE_ID_CALL_INCOMING);
    callType[1] = gbitmap_create_with_resource(RESOURCE_ID_CALL_MISSED);
    callType[2] = gbitmap_create_with_resource(RESOURCE_ID_CALL_OUTGOING);
  } else {
    callType[0] = NULL;
    callType[1] = NULL;
    callType[2] = NULL;
  }
}

static void contactsWindowUnload(Window *window) {
  if (callType[0]) gbitmap_destroy(callType[0]);
  if (callType[1]) gbitmap_destroy(callType[1]);
  if (callType[2]) gbitmap_destroy(callType[2]);
  menu_layer_destroy(contactMenu);
  text_layer_destroy(contactsTextLayer);
  window_destroy(contactWindow);
  contactWindow = NULL;
}

void contactsShow(uint8_t type) {
  contactWindow = window_create();
  window_set_window_handlers(contactWindow, (WindowHandlers) { .load = contactsWindowLoad, .unload = contactsWindowUnload });
  contactsType = type;
  window_stack_push(contactWindow, true);
}

void contactsRefresh() {
  userSendData(1);
}

bool contactsHandleDataReceived(DictionaryIterator *received) {
  Tuple *type_tuple = dict_find(received, 0);
  
  if (type_tuple == NULL || type_tuple->value->uint8 != 1) 
    return false;
  
  for (uint8_t j = 0; j < 2; j++) {
    uint8_t index = dict_find(received, 1 + j * 10)->value->uint8;

    bool isLast = (index & 0x80) != 0;
    uint8_t type = (index & 0x40) != 0 ? CONTACTS_HISTORY : CONTACTS_LIST;
    bool isEmpty = (index & 0x20) != 0;

    if (!isEmpty) {
      index &= 0x1F;
      if (index < MAX_CONTACTS) {
        Tuple *title = dict_find(received, 2 + j * 10);
        Tuple *subTitle = dict_find(received, 3 + j * 10);
        Tuple *callType = dict_find(received, 4 + j * 10);
        Contact* contact = &contacts[type][index];
        copyStr(contact->Name, title->value->cstring, MAX_NAME_LENGTH);
        copyStr(contact->Number, subTitle->value->cstring, MAX_NUMBER_LENGTH);
        contact->Type = callType ? callType->value->uint8 : 0xFF;
      }
    } 

    if (isLast) {
      contactsCount[type] = isEmpty ? 0 : index + 1;
      contactsLoaded[type] = true;
      if (contactsType == type && contactWindow != NULL && window_is_loaded(contactWindow)) {
        if (isEmpty) {
          text_layer_set_text(contactsTextLayer, "No Contacts");
          layer_set_hidden(text_layer_get_layer(contactsTextLayer), false);  
        } else {
          layer_set_hidden(text_layer_get_layer(contactsTextLayer), true);  
        }
        menu_layer_reload_data(contactMenu);
      }
      break;
    }
  }
  return true;
}
