// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app_control_channel.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/event_stream_handler_functions.h"

namespace flutter {

static constexpr char kChannelName[] = "tizen/app_control";
static constexpr char kEventChannelName[] = "tizen/app_control_event";
int AppControl::next_id_ = 0;

AppControlChannel::AppControlChannel(BinaryMessenger* messenger) {
  FT_LOGE("AppControlChannel");
  method_channel_ = std::make_unique<MethodChannel<EncodableValue>>(
      messenger, kChannelName, &StandardMethodCodec::GetInstance());

  method_channel_->SetMethodCallHandler([this](const auto& call, auto result) {
    this->HandleMethodCall(call, std::move(result));
  });

  event_channel_ = std::make_unique<EventChannel<EncodableValue>>(
      messenger, kEventChannelName, &StandardMethodCodec::GetInstance());

  auto event_channel_handler =
      std::make_unique<flutter::StreamHandlerFunctions<>>(
          [this](const flutter::EncodableValue* arguments,
                 std::unique_ptr<flutter::EventSink<>>&& events)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            FT_LOGE("OnListen");
            RegisterEventHandler(std::move(events));
            return nullptr;
          },
          [this](const flutter::EncodableValue* arguments)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            FT_LOGE("OnCancel");
            UnregisterEventHandler();
            return nullptr;
          });

  event_channel_->SetStreamHandler(std::move(event_channel_handler));
}

AppControlChannel::~AppControlChannel() {}

void AppControlChannel::NotifyAppControl(app_control_h app_control) {
  FT_LOGE("NotifyAppControl");
  auto app = std::make_unique<AppControl>(app_control);
  if (!events_) {
    queue_.push(app->GetId());
    FT_LOGE("EventChannel not set yet");
  } else {
    events_->Success(EncodableValue(app->GetId()));
  }
  map_.insert({app->GetId(), std::move(app)});
}

void AppControlChannel::HandleMethodCall(
    const MethodCall<EncodableValue>& method_call,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  FT_LOGE("HandleMethodCall : %s", method_call.method_name().data());
  // const auto& arguments = *method_call.arguments();

  if (method_call.method_name().compare("GetOperation") == 0) {
    GetOperation(std::move(result));
  } else {
    result->NotImplemented();
  }
}

void AppControlChannel::RegisterEventHandler(
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events) {
  FT_LOGE("RegisterEventHandler");
  events_ = std::move(events);
  SendAlreadyQueuedEvents();
}

void AppControlChannel::UnregisterEventHandler() {
  FT_LOGE("UnregisterEventHandler");
  events_.reset();
}

void AppControlChannel::SendAlreadyQueuedEvents() {
  FT_LOGE("HandleMethodCall: %d", queue_.size());
  while (!queue_.empty()) {
    events_->Success(EncodableValue(queue_.front()));
    queue_.pop();
  }
}

void AppControlChannel::GetOperation(
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  if (queue_.empty()) {
    result->Error("No app_control");
    return;
  }
  result->Success(EncodableValue(queue_.size()));
  return;
}

AppControl::AppControl(app_control_h app_control) : id_(next_id_++) {
  FT_LOGE("AppControl construct: %d", id_);
  int ret = app_control_clone(&handle_, app_control);
  if (ret != APP_CONTROL_ERROR_NONE) {
    FT_LOGE("Could not clone app control handle");
    handle_ = nullptr;
    return;
  }
}

AppControl::~AppControl() {
  FT_LOGE("AppControl destruct: %d", id_);
  app_control_destroy(handle_);
}

AppControlResult AppControl::GetString(std::string& str,
                                       int func(app_control_h, char**)) {
  FT_LOGD("AppControl::GetString");
  char* op;
  int ret = func(handle_, &op);
  if (ret != APP_CONTROL_ERROR_NONE) {
    return AppControlResult(ret);
  }
  str = std::string{op};
  free(op);
  return AppControlResult(APP_CONTROL_ERROR_NONE);
}

AppControlResult AppControl::SetString(const std::string& str,
                                       int func(app_control_h, const char*)) {
  FT_LOGD("AppControl::SetString");
  int ret = func(handle_, str.c_str());
  return AppControlResult(ret);
}

bool _app_control_extra_data_cb(app_control_h app,
                                const char* key,
                                void* user_data) {
  auto extra_data = static_cast<AppControlExtraData*>(user_data);
  bool is_array = false;
  int ret = app_control_is_extra_data_array(app, key, &is_array);
  if (ret != APP_CONTROL_ERROR_NONE) {
    FT_LOGE("app_control_is_extra_data_array() failed at key %s", key);
    return false;
  }

  if (is_array) {
    char** strings = NULL;
    int length = 0;
    ret = app_control_get_extra_data_array(app, key, &strings, &length);
    if (ret != APP_CONTROL_ERROR_NONE) {
      FT_LOGE("app_control_get_extra_data() failed at key %s", key);
      return false;
    }
    std::vector<std::string> vec;
    for (int i = 0; i < length; i++) {
      vec.push_back(strings[i]);
      free(strings[i]);
    }
    free(strings);
    extra_data->Add(key, vec);
  } else {
    char* value;
    ret = app_control_get_extra_data(app, key, &value);
    if (ret != APP_CONTROL_ERROR_NONE) {
      FT_LOGE("app_control_get_extra_data() failed at key %s", key);
      return false;
    }
    extra_data->Add(key, value);
    free(value);
  }

  return true;
}

AppControlResult AppControl::ReadExtraData() {
  int ret = app_control_foreach_extra_data(handle_, _app_control_extra_data_cb,
                                           &extra_data_);
  return AppControlResult(ret);
}

AppControlResult AppControl::GetOperation(std::string& operation) {
  return GetString(operation, app_control_get_operation);
}

AppControlResult AppControl::SetOperation(const std::string& operation) {
  return SetString(operation, app_control_set_operation);
}

AppControlResult AppControl::GetUri(std::string& uri) {
  return GetString(uri, app_control_get_uri);
}

AppControlResult AppControl::SetUri(const std::string& uri) {
  return SetString(uri, app_control_set_uri);
}

AppControlResult AppControl::GetMime(std::string& mime) {
  return GetString(mime, app_control_get_mime);
}

AppControlResult AppControl::SetMime(const std::string& mime) {
  return SetString(mime, app_control_set_mime);
}

AppControlResult AppControl::GetCategory(std::string& category) {
  return GetString(category, app_control_get_category);
}

AppControlResult AppControl::SetCategory(const std::string& category) {
  return SetString(category, app_control_set_category);
}

AppControlResult AppControl::GetAppId(std::string& app_id) {
  return GetString(app_id, app_control_get_app_id);
}

AppControlResult AppControl::SetAppId(const std::string& app_id) {
  return SetString(app_id, app_control_set_app_id);
}

AppControlResult AppControl::GetComponentId(std::string& component_id) {
  // Since 5.5
  return GetString(component_id, app_control_get_component_id);
}

AppControlResult AppControl::SetComponentId(const std::string& component_id) {
  // Since 5.5
  return SetString(component_id, app_control_set_component_id);
}

AppControlResult AppControl::GetCaller(std::string& caller) {
  return GetString(caller, app_control_get_caller);
}

AppControlResult AppControl::GetLaunchMode(LaunchMode& launch_mode) {
  app_control_launch_mode_e launch_mode_e;
  int ret = app_control_get_launch_mode(handle_, &launch_mode_e);
  if (ret != APP_CONTROL_ERROR_NONE) {
    return AppControlResult(ret);
  }
  launch_mode = static_cast<LaunchMode>(launch_mode_e);
  return AppControlResult(APP_CONTROL_ERROR_NONE);
}

AppControlResult AppControl::SetLaunchMode(const LaunchMode launch_mode) {
  app_control_launch_mode_e launch_mode_e =
      static_cast<app_control_launch_mode_e>(launch_mode);
  int ret = app_control_set_launch_mode(handle_, launch_mode_e);
  return AppControlResult(ret);
}

AppControlResult AppControl::Reply(AppControl* reply, Result result) {
  app_control_result_e result_e = static_cast<app_control_result_e>(result);
  int ret = app_control_reply_to_launch_request(reply->Handle(), this->handle_,
                                                result_e);
  return AppControlResult(ret);
}

}  // namespace flutter
