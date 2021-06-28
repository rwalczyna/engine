// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_APP_CONTROL_CHANNEL_H_
#define EMBEDDER_APP_CONTROL_CHANNEL_H_

#include <app.h>
#include <queue>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/binary_messenger.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/encodable_value.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/event_channel.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/method_channel.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/standard_method_codec.h"
#include "flutter/shell/platform/tizen/tizen_log.h"

class FlutterTizenEngine;

namespace flutter {

class AppControl {
 public:
  AppControl(app_control_h app_control) : id_(next_id_++) {
    FT_LOGD("AppControl construct: %d", id_);
    int ret = app_control_clone(&app_control_, app_control);
    if (ret != APP_CONTROL_ERROR_NONE) {
      FT_LOGE("Could not clone app control handle");
      app_control_ = nullptr;
      return;
    }
  }

  ~AppControl() {
    FT_LOGD("AppControl destruct: %d", id_);
    app_control_destroy(app_control_);
  }

  std::string GetOperation() {
    FT_LOGD("AppControl::GetOperation");
    char* op;
    int ret = app_control_get_operation(app_control_, &op);
    if (ret != APP_CONTROL_ERROR_NONE) {
      FT_LOGE("Could not get operation");
      return "";
    }
    std::string operation{op};
    free(op);
    return operation;
  }

 private:
  app_control_h app_control_;
  int id_;
  static int next_id_;
};

class AppControlChannel {
 public:
  explicit AppControlChannel(BinaryMessenger* messenger);
  virtual ~AppControlChannel();

  void NotifyAppControl(app_control_h app_control);

 private:
  void HandleMethodCall(const MethodCall<EncodableValue>& method_call,
                        std::unique_ptr<MethodResult<EncodableValue>> result);
  void RegisterEventHandler(
      std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events);
  void UnregisterEventHandler();
  void SendAlreadyQueuedEvents();
  void GetOperation(std::unique_ptr<MethodResult<EncodableValue>> result);

  std::unique_ptr<MethodChannel<EncodableValue>> method_channel_;
  std::unique_ptr<EventChannel<EncodableValue>> event_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events_;

  // We need this queue, because there is no quarantee
  // that EventChannel on Dart side will be registered
  // before native OnAppControl event
  // TODO: Add limit for queue elements
  std::queue<std::unique_ptr<AppControl>> queue_;
};

}  // namespace flutter
#endif  // EMBEDDER_APP_CONTROL_CHANNEL_H_
