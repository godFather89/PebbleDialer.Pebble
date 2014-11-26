#include "call.h"
#include "sms.h"
#include "util.h"

static Window *callWindow = NULL;
static GBitmap *iconHang, *iconAnswer, *iconMute;
static ActionBarLayer *actionbarlayer;
static TextLayer *phoneTextLayer;
static TextLayer *nameTextLayer;
static TextLayer *headerTextLayer;

static AppTimer *vibrateTimer;
static char callName[30];
static char callNumber[15];
static uint8_t callType;
static bool waitForResponse;

#define CALL_ENDED      0x00
#define CALL_RINGING    0x01
#define CALL_ANSWERED   0x02

static void stopVibrate() {
  if (vibrateTimer) {
    accel_tap_service_unsubscribe();
    vibes_cancel();
    app_timer_cancel(vibrateTimer);
    vibrateTimer = NULL;
  }
}

static void muteCall() {
  //mute the call
  stopVibrate();
  userSendData(5);
}

static void onTap(AccelAxisType axis, int32_t direction) {
  muteCall();
}

static void callButtonUpHandler(ClickRecognizerRef recognizer, void *context) {
  if (callType == CALL_RINGING)
    muteCall();
}

static void callButtonSelectHandler(ClickRecognizerRef recognizer, void *context) {
  if (callType == CALL_RINGING) { 
    //answer the call
    stopVibrate();
    userSendData(4);
  }
}

static void callButtonDownHandler(ClickRecognizerRef recognizer, void *context) {
  //hang up the call
  stopVibrate();
  userSendData(3);
}

static void onMessageSent(DictionaryIterator *iterator, void *context) {
    app_message_register_outbox_sent(NULL);
    smsShow(callNumber, true);
}

static void callButtonLongClickHandler(ClickRecognizerRef recognizer, void *context) {
  if (callType == CALL_RINGING) { 
    app_message_register_outbox_sent(onMessageSent);
    waitForResponse = true;
    callButtonDownHandler(recognizer, context);
  }
}

static void callWindowClickConfigProvider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, callButtonUpHandler);
  window_single_click_subscribe(BUTTON_ID_SELECT, callButtonSelectHandler);
  window_single_click_subscribe(BUTTON_ID_DOWN, callButtonDownHandler);
  window_long_click_subscribe(BUTTON_ID_DOWN, 600, callButtonLongClickHandler, NULL);
}

static void callWindowLoad(Window *window) {
  iconHang = gbitmap_create_with_resource(RESOURCE_ID_ICON_HANG);
  iconAnswer = gbitmap_create_with_resource(RESOURCE_ID_ICON_ANSWER);
  iconMute = gbitmap_create_with_resource(RESOURCE_ID_ICON_MUTE);
  
  // actionbarlayer
  actionbarlayer = action_bar_layer_create();
  action_bar_layer_add_to_window(actionbarlayer, callWindow);
  action_bar_layer_set_background_color(actionbarlayer, GColorBlack);
  
  action_bar_layer_set_icon(actionbarlayer, BUTTON_ID_UP, iconMute);
  action_bar_layer_set_icon(actionbarlayer, BUTTON_ID_SELECT, iconAnswer);
  action_bar_layer_set_icon(actionbarlayer, BUTTON_ID_DOWN, iconHang);
  
  action_bar_layer_set_click_config_provider(actionbarlayer, callWindowClickConfigProvider);
  layer_add_child(window_get_root_layer(callWindow), (Layer *)actionbarlayer);
  
  // headerTextLayer
  headerTextLayer = text_layer_create(GRect(3, 3, 112, 22));
  text_layer_set_font(headerTextLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(window_get_root_layer(callWindow), (Layer *)headerTextLayer);
  
  // nameTextLayer
  nameTextLayer = text_layer_create(GRect(3, 25, 115, 85));
  text_layer_set_text(nameTextLayer, callName);
  text_layer_set_font(nameTextLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(window_get_root_layer(callWindow), (Layer *)nameTextLayer);
  
  // phoneTextLayer
  phoneTextLayer = text_layer_create(GRect(3, 122, 112, 22));
  text_layer_set_text(phoneTextLayer, callNumber);
  text_layer_set_font(phoneTextLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_get_root_layer(callWindow), (Layer *)phoneTextLayer);
}

static void vibeTimerCallback(void *data) {
  vibes_short_pulse();
  vibrateTimer = app_timer_register(2000, vibeTimerCallback, data);
}

static void callWindowUnload(Window* window) {
  stopVibrate();
  action_bar_layer_destroy(actionbarlayer);
  text_layer_destroy(phoneTextLayer);
  text_layer_destroy(nameTextLayer);
  text_layer_destroy(headerTextLayer);
  gbitmap_destroy(iconHang);
  gbitmap_destroy(iconAnswer);
  gbitmap_destroy(iconMute);
  window_destroy(callWindow);
  callWindow = NULL;
}
  
void callShow(void) {
  waitForResponse = false;
  callWindow = window_create();
  window_set_window_handlers(callWindow, (WindowHandlers) {
    .load = callWindowLoad,
    .unload = callWindowUnload,
  });
  window_stack_pop_all(false);
  window_stack_push(callWindow, true);
}

bool callHandleDataReceived(DictionaryIterator *received) {
  Tuple *type_tuple = dict_find(received, 0);
  if (type_tuple == NULL || type_tuple->value->uint8 != 0xCA) 
    return false;
  
  Tuple *showtuple = dict_find(received, 1);
  callType = showtuple->value->uint8;
  if (callType == 0) {
    if (callWindow != NULL && window_is_loaded(callWindow) && !waitForResponse)
      window_stack_remove(callWindow, true);
  } else {
    Tuple *tname = dict_find(received, 2);
    Tuple *tnumber = dict_find(received, 3);
    if (tname && tnumber) {
      copyStr(callName, tname->value->cstring, sizeof(callName));
      copyStr(callNumber, tnumber->value->cstring, sizeof(callNumber));
      if (callWindow != NULL && window_is_loaded(callWindow)) {
        text_layer_set_text(nameTextLayer, callName);
        text_layer_set_text(phoneTextLayer, callNumber);
        layer_mark_dirty(window_get_root_layer(callWindow));
      } else {
        callShow();
      }
    }
    
    if (callType == CALL_RINGING) {
      text_layer_set_text(headerTextLayer, "Incoming call");
      layer_mark_dirty(text_layer_get_layer(headerTextLayer));
      light_enable_interaction();
      accel_tap_service_subscribe(onTap);
      vibeTimerCallback(NULL);
    } else if (callType == CALL_ANSWERED) {
      stopVibrate();
      text_layer_set_text(headerTextLayer, "In call");
      layer_mark_dirty(text_layer_get_layer(headerTextLayer));
      
      action_bar_layer_clear_icon(actionbarlayer, BUTTON_ID_UP); //no more mute button
      action_bar_layer_clear_icon(actionbarlayer, BUTTON_ID_SELECT); //no more answer button
    }
  }
  
  return true;
}
