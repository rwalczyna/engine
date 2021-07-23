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
#include "flutter/shell/platform/tizen/logger.h"

class FlutterTizenEngine;

namespace flutter {

struct AppControlResult {
  AppControlResult() : error_code(APP_CONTROL_ERROR_NONE){};
  AppControlResult(int code) : error_code(code) {}

  // Returns false on error
  operator bool() const { return (APP_CONTROL_ERROR_NONE == error_code); }

  std::string message() { return get_error_message(error_code); }

  int error_code;
};

class AppControlChannel;

class AppControl {
 public:
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
  AppControlResult GetLaunchMode(std::string& launch_mode);
  AppControlResult SetLaunchMode(const std::string& launch_mode);

  EncodableValue SerializeAppControlToMap();

  AppControlResult SendLaunchRequest();
  AppControlResult SendLaunchRequestWithReply(
      std::shared_ptr<EventSink<EncodableValue>> reply_sink,
      AppControlChannel* manager);
  AppControlResult SendTerminateRequest();

  AppControlResult Reply(std::shared_ptr<AppControl> reply,
                         const std::string& result);

  AppControlResult GetExtraData(EncodableValue& value);
  AppControlResult SetExtraData(EncodableValue& value);

  void SetManager(AppControlChannel* m);
  AppControlChannel* GetManager();

 private:
  AppControlResult GetString(std::string& str, int func(app_control_h, char**));
  AppControlResult SetString(const std::string& str,
                             int func(app_control_h, const char*));
  AppControlResult WriteExtraDataStringToHandle();
  AppControlResult WriteExtraDataToHandle();

  AppControlResult AddExtraData(std::string key, EncodableValue value);
  AppControlResult AddExtraDataList(std::string& key, EncodableList& list);

  app_control_h handle_;
  int id_;
  static int next_id_;
  std::shared_ptr<EventSink<EncodableValue>> reply_sink_;

  AppControlChannel* manager_;
};

class AppControlChannel {
 public:
  explicit AppControlChannel(BinaryMessenger* messenger);
  virtual ~AppControlChannel();

  void NotifyAppControl(app_control_h app_control);

  void AddExistingAppControl(std::shared_ptr<AppControl> app_control) {
    map_.insert({app_control->GetId(), app_control});
  }

 private:
  void HandleMethodCall(const MethodCall<EncodableValue>& method_call,
                        std::unique_ptr<MethodResult<EncodableValue>> result);
  void RegisterEventHandler(
      std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events);
  void UnregisterEventHandler();
  void SendAlreadyQueuedEvents();

  void RegisterReplyHandler(
      std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events);
  void UnregisterReplyHandler();

  template <typename T>
  bool GetValueFromArgs(const flutter::EncodableValue* args,
                        const char* key,
                        T& out);
  bool GetEncodableValueFromArgs(const flutter::EncodableValue* args,
                                 const char* key,
                                 flutter::EncodableValue& out);

  std::shared_ptr<AppControl> GetAppControl(const EncodableValue* args);

  void CreateAppControl(const EncodableValue* args,
                        std::unique_ptr<MethodResult<EncodableValue>> result);

  void Dispose(std::shared_ptr<AppControl> app_control,
               std::unique_ptr<MethodResult<EncodableValue>> result);
  void Reply(std::shared_ptr<AppControl> app_control,
             const flutter::EncodableValue* arguments,
             std::unique_ptr<MethodResult<EncodableValue>> result);
  void SendLaunchRequest(std::shared_ptr<AppControl> app_control,
                         const flutter::EncodableValue* arguments,
                         std::unique_ptr<MethodResult<EncodableValue>> result);
  void SendTerminateRequest(
      std::shared_ptr<AppControl> app_control,
      const flutter::EncodableValue* arguments,
      std::unique_ptr<MethodResult<EncodableValue>> result);

  void SetAppControlData(std::shared_ptr<AppControl> app_control,
                         const flutter::EncodableValue* arguments,
                         std::unique_ptr<MethodResult<EncodableValue>> result);
  void SendAppControlDataEvent(std::shared_ptr<AppControl> app_control);

  std::unique_ptr<MethodChannel<EncodableValue>> method_channel_;
  std::unique_ptr<EventChannel<EncodableValue>> event_channel_;
  std::unique_ptr<EventChannel<EncodableValue>> reply_channel_;
  std::unique_ptr<EventSink<EncodableValue>> event_sink_;
  std::shared_ptr<EventSink<EncodableValue>> reply_sink_;

  // We need this queue, because there is no quarantee
  // that EventChannel on Dart side will be registered
  // before native OnAppControl event
  std::queue<std::shared_ptr<AppControl>> queue_;

  std::unordered_map<int, std::shared_ptr<AppControl>> map_;
};

}  // namespace flutter
#endif  // EMBEDDER_APP_CONTROL_CHANNEL_H_
