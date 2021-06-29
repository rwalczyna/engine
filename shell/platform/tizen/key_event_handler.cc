// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "key_event_handler.h"

#include <app.h>

#include "flutter/shell/platform/tizen/flutter_tizen_engine.h"
#include "flutter/shell/platform/tizen/tizen_log.h"

namespace flutter {

namespace {

constexpr char kBackKey[] = "XF86Back";
constexpr char kExitKey[] = "XF86Exit";

}  // namespace

KeyEventHandler::KeyEventHandler(FlutterTizenEngine* engine) : engine_(engine) {
  key_event_handlers_.push_back(
      ecore_event_handler_add(ECORE_EVENT_KEY_DOWN, OnKey, this));
  key_event_handlers_.push_back(
      ecore_event_handler_add(ECORE_EVENT_KEY_UP, OnKey, this));
}

KeyEventHandler::~KeyEventHandler() {
  for (auto handler : key_event_handlers_) {
    ecore_event_handler_del(handler);
  }
  key_event_handlers_.clear();
}

Eina_Bool KeyEventHandler::OnKey(void* data, int type, void* event) {
  auto* self = reinterpret_cast<KeyEventHandler*>(data);
  auto* key = reinterpret_cast<Ecore_Event_Key*>(event);
  auto* engine = self->engine_;
  auto is_down = type == ECORE_EVENT_KEY_DOWN;

  FT_LOGI("Keycode: %d, name: %s, mods: %d, is_down: %d", key->keycode,
          key->keyname, key->modifiers, is_down);

  if (engine->text_input_channel) {
    if (is_down) {
      engine->text_input_channel->OnKeyDown(key);
    }
    if (engine->text_input_channel->IsSoftwareKeyboardShowing()) {
      return ECORE_CALLBACK_PASS_ON;
    }
  }

  if (engine->key_event_channel) {
    engine->key_event_channel->SendKeyEvent(
        key, is_down,
        [engine, keyname = std::string(key->keyname), is_down](bool handled) {
          if (handled) {
            return;
          }
          if (keyname == kBackKey && !is_down) {
            if (engine->navigation_channel) {
              engine->navigation_channel->PopRoute();
            }
          } else if (keyname == kExitKey && !is_down) {
            ui_app_exit();
          }
        });
  }
  return ECORE_CALLBACK_PASS_ON;
}

}  // namespace flutter