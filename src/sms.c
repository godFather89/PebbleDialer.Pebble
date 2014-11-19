#include "sms.h"
#include "util.h"

#define MAX_SMS_COUNT   10
#define MAX_SMS_LENGTH  25
  
typedef struct {
  char Message[MAX_SMS_LENGTH];
  uint32_t HashCode;
} __attribute__((__packed__)) Sms;
  
static Window *smsWindow = NULL;
static MenuLayer* smsMenu;
static TextLayer *smsTextLayer;
static uint8_t smsCount = 0;
static Sms smsList[MAX_SMS_COUNT];
static char smsPhoneNumber[15];

static void smsMenuClick(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, 0, 0x3E);
  dict_write_cstring(iter, 1, smsPhoneNumber);
  dict_write_uint32(iter, 2, smsList[cell_index->row].HashCode);
  app_message_outbox_send();
  
  window_stack_remove(smsWindow, true);
}

static int16_t smsMenuCellHeight(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  return 25;
}

static void smsMenuDrawRow(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context) { 
  graphics_context_set_text_color(ctx, GColorBlack);

  const char *messageText = smsList[cell_index->row].Message;
  GFont *font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  graphics_draw_text(ctx, messageText, font, GRect(5, -5, 134, 28), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static uint16_t smsMenuGetCount(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
	return smsCount;
}

static void smsWindowLoad(Window *window) {
  GRect frame = layer_get_bounds(window_get_root_layer(window));
  smsMenu = menu_layer_create(GRect(0,0,frame.size.w,frame.size.h));
  
  menu_layer_set_callbacks(smsMenu, NULL, (MenuLayerCallbacks) {
    .get_cell_height = &smsMenuCellHeight,
    .get_num_rows = &smsMenuGetCount,
	  .select_click = &smsMenuClick,
	  .draw_row = &smsMenuDrawRow,
  }); 
  menu_layer_set_click_config_onto_window(smsMenu, window);
	layer_add_child(window_get_root_layer(window), menu_layer_get_layer(smsMenu));
  
  smsTextLayer = text_layer_create(GRect(0, 10, frame.size.w, 70));
  text_layer_set_font(smsTextLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(smsTextLayer, "Loading Messages");
  text_layer_set_text_alignment(smsTextLayer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(smsTextLayer, GTextOverflowModeWordWrap);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(smsTextLayer));
}

static void smsWindowUnload(Window *window) {
  app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
  menu_layer_destroy(smsMenu);
  window_destroy(smsWindow);
  smsWindow = NULL;
}

void smsShow(const char* destPhoneNumber, bool closeAfterSend) {
  smsCount = 0;
  
  app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
  copyStr(smsPhoneNumber, destPhoneNumber, sizeof(smsPhoneNumber));
  userSendData(0xE5);
  
  smsWindow = window_create();
  window_set_window_handlers(smsWindow, (WindowHandlers) { .load = smsWindowLoad, .unload = smsWindowUnload });
  if (closeAfterSend) window_stack_pop_all(false);
  window_stack_push(smsWindow, true);
}

bool smsHandleDataReceived(DictionaryIterator *received) {
  Tuple *type_tuple = dict_find(received, 0);
  if (type_tuple == NULL || type_tuple->value->uint8 != 0x5E) 
    return false;
  
  for (uint8_t i=0; i<2; i++) {
    uint8_t index = dict_find(received, 1 + 10*i)->value->uint8;
    bool isLast = (index & 0x80) != 0;
    bool isEmpty = (index & 0x20) != 0;
    
    if (!isEmpty) {
      index &= 0x1F;
      if (index < MAX_SMS_COUNT) {
        Tuple *message = dict_find(received, 2 + 10*i);
        Tuple *hash = dict_find(received, 3 + 10*i);
        Sms *sms = &smsList[index];
        copyStr(sms->Message, message->value->cstring, MAX_SMS_LENGTH);
        sms->HashCode = hash->value->uint32;
      }
    }
    
    if (isLast) {
      app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
      smsCount = isEmpty ? 0 : index + 1;
      if (smsWindow != NULL && window_is_loaded(smsWindow)) {
        if (isEmpty) {
          text_layer_set_text(smsTextLayer, "No Messages");
        } else {
          layer_set_hidden(text_layer_get_layer(smsTextLayer), true);  
          menu_layer_reload_data(smsMenu);
        }
      }
      break;
    }
  }
  
  return true;
}