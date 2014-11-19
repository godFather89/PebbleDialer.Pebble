#include <pebble.h>
#include "contacts.h"
#include "call.h"
#include "sms.h"

static Window *window;
static MenuLayer* mainMenu;
static char *menu[2] = { "History", "Contacts" };
static GBitmap *menuIcons[2];

static void mainMenu_select_click(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  contactsShow(cell_index->row);
}

static void mainMenu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context) { 
  uint16_t row = cell_index->row;
  menu_cell_basic_draw(ctx, cell_layer, menu[row], NULL, menuIcons[row]);
}

static uint16_t mainMenu_get_num_rows(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
	return 2;
}

static void in_received_handler(DictionaryIterator *received, void *context) {
  if (contactsHandleDataReceived(received)) return;
  if (callHandleDataReceived(received)) return;
  if (smsHandleDataReceived(received)) return;
}

static void mainLoad(Window *window) {
  GRect frame = layer_get_bounds(window_get_root_layer(window));
  mainMenu = menu_layer_create(GRect(0,0,frame.size.w,frame.size.h));
  MenuLayerCallbacks cbacks = {
    .get_num_rows = &mainMenu_get_num_rows,
	  .select_click = &mainMenu_select_click,
	  .draw_row = &mainMenu_draw_row,
  };
  menu_layer_set_callbacks(mainMenu, NULL, cbacks); 
  menu_layer_set_click_config_onto_window(mainMenu, window);
	layer_add_child(window_get_root_layer(window), menu_layer_get_layer(mainMenu));
  menuIcons[0] = gbitmap_create_with_resource(RESOURCE_ID_MENU_HISTORY);
  menuIcons[1] = gbitmap_create_with_resource(RESOURCE_ID_MENU_CONTACTS);
}

static void mainUnload(Window *window) {
  menu_layer_destroy(mainMenu);
  gbitmap_destroy(menuIcons[0]);
  gbitmap_destroy(menuIcons[1]);
}

static void init(void) {
  app_message_register_inbox_received(in_received_handler);
  app_message_open(124, 64);
  
  contactsRefresh();
  
  window = window_create();
  WindowHandlers handlers = { .load = mainLoad, .unload = mainUnload };
  window_set_window_handlers(window, handlers);  
  window_stack_push(window, true);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  //start
  init();
  app_event_loop();
  deinit();
}
