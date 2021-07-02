// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app_control_channel.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/event_stream_handler_functions.h"

namespace flutter {

static constexpr char kChannelName[] = "tizen/internal/app_control_method";
static constexpr char kEventChannelName[] = "tizen/internal/app_control_event";
int AppControl::next_id_ = 0;

AppControlChannel::AppControlChannel(BinaryMessenger* messenger) {
  FT_LOGI("AppControlChannel");
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
            FT_LOGI("OnListen");
            RegisterEventHandler(std::move(events));
            return nullptr;
          },
          [this](const flutter::EncodableValue* arguments)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            FT_LOGI("OnCancel");
            UnregisterEventHandler();
            return nullptr;
          });

  event_channel_->SetStreamHandler(std::move(event_channel_handler));
}

AppControlChannel::~AppControlChannel() {}

void AppControlChannel::NotifyAppControl(app_control_h app_control) {
  FT_LOGI("NotifyAppControl");
  auto app = std::make_unique<AppControl>(app_control);
  if (!events_) {
    queue_.push(app->GetId());
    FT_LOGI("EventChannel not set yet");
  } else {
    events_->Success(EncodableValue(app->GetId()));
  }
  map_.insert({app->GetId(), std::move(app)});
}

void AppControlChannel::HandleMethodCall(
    const MethodCall<EncodableValue>& method_call,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  FT_LOGI("HandleMethodCall : %s", method_call.method_name().c_str());
  const auto arguments = method_call.arguments();
  const auto& method_name = method_call.method_name();

  // AppControl not needed
  if (method_name.compare("CreateAppControl") == 0) {
    CreateAppControl(arguments, std::move(result));
    return;
  }

  // AppControl needed
  auto app_control = GetAppControl(arguments);
  if (app_control == nullptr) {
    result->Error("Could not find app_control", "Invalid parameter");
    return;
  }

  AppControlResult ret;
  // Common
  if (method_name.compare("SendLaunchRequest") == 0) {
    ret = app_control->SendLaunchRequest();
  } else if (method_name.compare("SendTerminateRequest") == 0) {
    ret = app_control->SendTerminateRequest();
  }

  if (ret.valid) {
    if (ret) {
      result->Success();
    } else {
      result->Error(ret.message());
    }
    return;
  }

  // Getters
  std::string str;
  if (method_name.compare("GetAppId") == 0) {
    ret = app_control->GetAppId(str);
  } else if (method_name.compare("GetOperation") == 0) {
    ret = app_control->GetOperation(str);
  } else if (method_name.compare("GetUri") == 0) {
    ret = app_control->GetUri(str);
  } else if (method_name.compare("GetMime") == 0) {
    ret = app_control->GetMime(str);
  } else if (method_name.compare("GetCategory") == 0) {
    ret = app_control->GetMime(str);
  } else if (method_name.compare("GetCaller") == 0) {
    ret = app_control->GetMime(str);
  } else if (method_name.compare("GetLaunchMode") == 0) {
    ret = app_control->GetLaunchMode(str);
  }

  if (ret.valid) {
    if (ret) {
      result->Success(EncodableValue(str));
    } else {
      result->Error(ret.message());
    }
    return;
  }

  // Setters
  if (!GetValueFromArgs<std::string>(arguments, "argument", str)) {
    result->Error("Invalid argument");
    return;
  }

  if (method_name.compare("SetAppId") == 0) {
    ret = app_control->SetAppId(str);
  } else if (method_name.compare("SetOperation") == 0) {
    ret = app_control->SetOperation(str);
    } else if (method_name.compare("SetUri") == 0) {
    ret = app_control->SetUri(str);
  } else if (method_name.compare("SetMime") == 0) {
    ret = app_control->SetMime(str);
  } else if (method_name.compare("SetCategory") == 0) {
    ret = app_control->SetMime(str);
  } else if (method_name.compare("SetCaller") == 0) {
    ret = app_control->SetMime(str);
  } else if (method_name.compare("SetLaunchMode") == 0) {
    ret = app_control->SetLaunchMode(str);
  }

  if (ret.valid) {
    if (ret) {
      result->Success();
    } else {
      result->Error(ret.message());
    }
    result->NotImplemented();
    return;
  }
}

void AppControlChannel::RegisterEventHandler(
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events) {
  FT_LOGI("RegisterEventHandler");
  events_ = std::move(events);
  SendAlreadyQueuedEvents();
}

void AppControlChannel::UnregisterEventHandler() {
  FT_LOGI("UnregisterEventHandler");
  events_.reset();
}

void AppControlChannel::SendAlreadyQueuedEvents() {
  FT_LOGI("SendAlreadyQueuedEvents: %d", queue_.size());
  while (!queue_.empty()) {
    events_->Success(EncodableValue(queue_.front()));
    queue_.pop();
  }
}

template <typename T>
bool AppControlChannel::GetValueFromArgs(const flutter::EncodableValue* args,
                                         const char* key,
                                         T& out) {
  if (std::holds_alternative<flutter::EncodableMap>(*args)) {
    flutter::EncodableMap map = std::get<flutter::EncodableMap>(*args);
    if (map.find(flutter::EncodableValue(key)) != map.end()) {
      flutter::EncodableValue value = map[flutter::EncodableValue(key)];
      if (std::holds_alternative<T>(value)) {
        out = std::get<T>(value);
        return true;
      }
    }
    FT_LOGI("Key %s not found", key);
  }
  return false;
}

bool AppControlChannel::GetEncodableValueFromArgs(
    const flutter::EncodableValue* args,
    const char* key,
    flutter::EncodableValue& out) {
  if (std::holds_alternative<flutter::EncodableMap>(*args)) {
    flutter::EncodableMap map = std::get<flutter::EncodableMap>(*args);
    if (map.find(flutter::EncodableValue(key)) != map.end()) {
      out = map[flutter::EncodableValue(key)];
      return true;
    }
  }
  return false;
}

std::shared_ptr<AppControl> AppControlChannel::GetAppControl(
    const EncodableValue* args) {
  int id;
  if (!GetValueFromArgs<int>(args, "id", id)) {
    FT_LOGE("Could not find AppControl with id %d", id);
    return nullptr;
  }

  if (map_.find(id) == map_.end()) {
    FT_LOGE("Could not find AppControl with id %d", id);
    return nullptr;
  }
  FT_LOGI("Found AppControl: %d", id);
  return map_[id];
}

bool AppControlChannel::ValidateAppControlResult(
    AppControlResult ret,
    MethodResult<EncodableValue>* result) {
  if (ret.valid) {
    if (ret) {
      result->Success();
    } else {
      result->Error(ret.message());
    }
    return true;
  }
  return false;
}

void AppControlChannel::CreateAppControl(
    const EncodableValue* args,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  FT_LOGI("AppControlChannel::CreateAppControl");
  app_control_h app_control = nullptr;
  AppControlResult ret = app_control_create(&app_control);
  if (!ret) {
    result->Error("Could not create AppControl", ret.message());
  }
  auto app = std::make_unique<AppControl>(app_control);
  int id = app->GetId();
  map_.insert({app->GetId(), std::move(app)});
  result->Success(EncodableValue(id));
}

AppControl::AppControl(app_control_h app_control) : id_(next_id_++) {
  FT_LOGI("AppControl construct: %d", id_);
  int ret = app_control_clone(&handle_, app_control);
  if (ret != APP_CONTROL_ERROR_NONE) {
    FT_LOGE("Could not clone app control handle");
    handle_ = nullptr;
    return;
  }
}

AppControl::~AppControl() {
  FT_LOGI("AppControl destruct: %d", id_);
  app_control_destroy(handle_);
}

AppControlResult AppControl::GetString(std::string& str,
                                       int func(app_control_h, char**)) {
  FT_LOGI("AppControl::GetString");
  char* op;
  AppControlResult ret = func(handle_, &op);
  if (!ret) {
    return ret;
  }
  if (op != nullptr) {
    str = std::string{op};
    free(op);
  } else {
    str = "";
  }
  return AppControlResult(APP_CONTROL_ERROR_NONE);
}

AppControlResult AppControl::SetString(const std::string& str,
                                       int func(app_control_h, const char*)) {
  FT_LOGI("AppControl::SetString: %s", str.c_str());
  int ret = func(handle_, str.c_str());
  return AppControlResult(ret);
}

bool _app_control_extra_data_cb(app_control_h app,
                                const char* key,
                                void* user_data) {
  auto extra_data = static_cast<EncodableMap*>(user_data);
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
    EncodableList list;
    for (int i = 0; i < length; i++) {
      list.push_back(EncodableValue(std::string(strings[i])));
      free(strings[i]);
    }
    free(strings);
    extra_data->insert(
        {EncodableValue(std::string(key)), EncodableValue(list)});
  } else {
    char* value;
    ret = app_control_get_extra_data(app, key, &value);
    if (ret != APP_CONTROL_ERROR_NONE) {
      FT_LOGE("app_control_get_extra_data() failed at key %s", key);
      return false;
    }
    extra_data->insert(
        {EncodableValue(std::string(key)), EncodableValue(std::string(value))});
    free(value);
  }

  return true;
}

AppControlResult AppControl::ReadAllExtraData(EncodableValue& value) {
  EncodableMap extra_data;
  int ret = app_control_foreach_extra_data(handle_, _app_control_extra_data_cb,
                                           &extra_data);
  if (ret == APP_CONTROL_ERROR_NONE) {
    value = EncodableValue(extra_data);
  }
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

AppControlResult AppControl::GetLaunchMode(std::string& launch_mode) {
  app_control_launch_mode_e launch_mode_e;
  int ret = app_control_get_launch_mode(handle_, &launch_mode_e);
  if (ret != APP_CONTROL_ERROR_NONE) {
    return AppControlResult(ret);
  }
  launch_mode =
      (launch_mode_e == APP_CONTROL_LAUNCH_MODE_SINGLE ? "Single" : "Group");
  return AppControlResult(APP_CONTROL_ERROR_NONE);
}

AppControlResult AppControl::SetLaunchMode(const std::string& launch_mode) {
  app_control_launch_mode_e launch_mode_e;
  if (launch_mode.compare("Single")) {
    launch_mode_e = APP_CONTROL_LAUNCH_MODE_SINGLE;
  } else {
    launch_mode_e = APP_CONTROL_LAUNCH_MODE_GROUP;
  }
  int ret = app_control_set_launch_mode(handle_, launch_mode_e);
  return AppControlResult(ret);
}

AppControlResult AppControl::SendLaunchRequest() {
  AppControlResult ret =
      app_control_send_launch_request(handle_, nullptr, nullptr);
  return ret;
}

AppControlResult AppControl::SendTerminateRequest() {
  AppControlResult ret = app_control_send_terminate_request(handle_);
  return ret;
}

AppControlResult AppControl::Reply(AppControl* reply, Result result) {
  app_control_result_e result_e = static_cast<app_control_result_e>(result);
  int ret = app_control_reply_to_launch_request(reply->Handle(), this->handle_,
                                                result_e);
  return AppControlResult(ret);
}

AppControlResult AppControl::AddExtraData(std::string key,
                                          EncodableValue value) {
  bool is_array = std::holds_alternative<EncodableList>(value);
  if (is_array) {
    EncodableList& list = std::get<EncodableList>(value);
    return AddExtraDataList(key, list);
  } else {
    bool is_string = std::holds_alternative<std::string>(value);
    if (is_string) {
      int ret = app_control_add_extra_data(
          handle_, key.c_str(), std::get<std::string>(value).c_str());
      return AppControlResult(ret);
    } else {
      return AppControlResult(APP_ERROR_INVALID_PARAMETER);
    }
  }
  return AppControlResult(APP_CONTROL_ERROR_NONE);
}

AppControlResult AppControl::AddExtraDataList(std::string& key,
                                              EncodableList& list) {
  size_t length = list.size();
  auto strings = new const char*[length];
  for (size_t i = 0; i < length; i++) {
    bool is_string = std::holds_alternative<std::string>(list[i]);
    if (is_string) {
      strings[i] = std::get<std::string>(list[i]).c_str();
    } else {
      delete[] strings;
      return AppControlResult(APP_ERROR_INVALID_PARAMETER);
    }
  }
  int ret =
      app_control_add_extra_data_array(handle_, key.c_str(), strings, length);
  delete[] strings;
  return AppControlResult(ret);
}
}  // namespace flutter
