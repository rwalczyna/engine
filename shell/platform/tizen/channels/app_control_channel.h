// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_APP_CONTROL_CHANNEL_H_
#define EMBEDDER_APP_CONTROL_CHANNEL_H_

#include <app.h>
#include <queue>
#include <unordered_map>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/binary_messenger.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/encodable_value.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/event_channel.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/method_channel.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/standard_method_codec.h"
#include "flutter/shell/platform/tizen/tizen_log.h"

class FlutterTizenEngine;

namespace flutter {

using EncodableList = std::vector<EncodableValue>;
using EncodableMap = std::map<EncodableValue, EncodableValue>;

struct AppControlResult {
  AppControlResult() : error_code(APP_CONTROL_ERROR_NONE){};
  AppControlResult(int code) : error_code(code) {}

  // Returns false on error
  operator bool() const { return APP_CONTROL_ERROR_NONE == error_code; }

  std::string message() { return get_error_message(error_code); }

  int error_code;
};

class AppControlExtraData {
 public:
  AppControlExtraData() {}
  ~AppControlExtraData() {}

  void Add(std::string key, std::string value) {
    if (strings_lists_.find(key) != strings_lists_.end()) {
      strings_lists_.erase(key);
    }
    strings_[key] = value;
  }

  void Add(std::string key, std::vector<std::string> value) {
    if (strings_.find(key) != strings_.end()) {
      strings_.erase(key);
    }
    strings_lists_[key] = value;
  }

  void Remove(std::string key) {
    strings_.erase(key);
    strings_lists_.erase(key);
  }

  bool Has(std::string key) {
    if (strings_.find(key) != strings_.end()) {
      return true;
    }
    if (strings_lists_.find(key) != strings_lists_.end()) {
      return true;
    }
    return false;
  }

  std::vector<std::string>& GetList(std::string key) {
    return strings_lists_[key];
  }

  std::string& GetString(std::string key) { return strings_[key]; }

  size_t Size() { return strings_.size() + strings_lists_.size(); }

 private:
  std::unordered_map<std::string, std::string> strings_;
  std::unordered_map<std::string, std::vector<std::string>> strings_lists_;
};

class AppControl {
 public:
  enum LaunchMode {
    Single = APP_CONTROL_LAUNCH_MODE_SINGLE,
    Group = APP_CONTROL_LAUNCH_MODE_GROUP
  };
  enum Result {
    Started = APP_CONTROL_RESULT_APP_STARTED,
    Succeeded = APP_CONTROL_RESULT_SUCCEEDED,
    Failed = APP_CONTROL_RESULT_FAILED,
    Cancelled = APP_CONTROL_RESULT_CANCELED
  };

  AppControl(app_control_h app_control);
  ~AppControl();

  int GetId() { return id_; }
  app_control_h Handle() { return handle_; }

  AppControlResult GetOperation(std::string& operation);
  AppControlResult SetOperation(const std::string& operation);
  AppControlResult GetUri(std::string& uri);
  AppControlResult SetUri(const std::string& uri);
  AppControlResult GetMime(std::string& mime);
  AppControlResult SetMime(const std::string& mime);
  AppControlResult GetCategory(std::string& category);
  AppControlResult SetCategory(const std::string& category);
  AppControlResult GetAppId(std::string& app_id);
  AppControlResult SetAppId(const std::string& app_id);
  AppControlResult GetComponentId(std::string& component_id);
  AppControlResult SetComponentId(const std::string& component_id);
  AppControlResult GetCaller(std::string& caller);
  AppControlResult GetLaunchMode(LaunchMode& launch_mode);
  AppControlResult SetLaunchMode(const LaunchMode launch_mode);

  AppControlResult Reply(AppControl* reply, Result result);

 private:
  AppControlResult GetString(std::string& str, int func(app_control_h, char**));
  AppControlResult SetString(const std::string& str,
                             int func(app_control_h, const char*));
  AppControlResult ReadExtraData();

  AppControlExtraData extra_data_;
  app_control_h handle_;
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
  std::queue<int> queue_;

  std::unordered_map<int, std::unique_ptr<AppControl>> map_;
};

}  // namespace flutter
#endif  // EMBEDDER_APP_CONTROL_CHANNEL_H_
