// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app_control_channel.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/event_stream_handler_functions.h"

namespace flutter {

static constexpr char kChannelName[] = "tizen/internal/app_control_method";
static constexpr char kEventChannelName[] = "tizen/internal/app_control_event";
static constexpr char kReplyChannelName[] = "tizen/internal/app_control_reply";

int AppControl::next_id_ = 0;

AppControlChannel::AppControlChannel(BinaryMessenger* messenger) {
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
            RegisterEventHandler(std::move(events));
            return nullptr;
          },
          [this](const flutter::EncodableValue* arguments)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            UnregisterEventHandler();
            return nullptr;
          });

  event_channel_->SetStreamHandler(std::move(event_channel_handler));

  reply_channel_ = std::make_unique<EventChannel<EncodableValue>>(
      messenger, kReplyChannelName, &StandardMethodCodec::GetInstance());

  auto reply_channel_handler =
      std::make_unique<flutter::StreamHandlerFunctions<>>(
          [this](const flutter::EncodableValue* arguments,
                 std::unique_ptr<flutter::EventSink<>>&& events)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            RegisterReplyHandler(std::move(events));
            return nullptr;
          },
          [this](const flutter::EncodableValue* arguments)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            UnregisterReplyHandler();
            return nullptr;
          });

  reply_channel_->SetStreamHandler(std::move(reply_channel_handler));
}

AppControlChannel::~AppControlChannel() {}

void AppControlChannel::NotifyAppControl(app_control_h app_control) {
  app_control_h clone = nullptr;
  AppControlResult ret = app_control_clone(&clone, app_control);
  if (!ret) {
    FT_LOG(Error) << "Could not clone app control " << ret.message();
    return;
  }
  auto app = std::make_shared<AppControl>(clone);
  if (!event_sink_) {
    queue_.push(app);
    FT_LOG(Info) << "EventChannel not set yet ";
  } else {
    SendAppControlDataEvent(app);
  }
  map_.insert({app->GetId(), app});
}

void AppControlChannel::HandleMethodCall(
    const MethodCall<EncodableValue>& method_call,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  FT_LOG(Info) << "HandleMethodCall " << method_call.method_name();
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

  // Common
  if (method_name.compare("dispose") == 0) {
    Dispose(app_control, std::move(result));
  } else if (method_name.compare("reply") == 0) {
    Reply(app_control, arguments, std::move(result));
  } else if (method_name.compare("sendLaunchRequest") == 0) {
    SendLaunchRequest(app_control, arguments, std::move(result));
  } else if (method_name.compare("setAppControlData") == 0) {
    SetAppControlData(app_control, arguments, std::move(result));
  } else if (method_name.compare("sendTerminateRequest") == 0) {
    SendTerminateRequest(app_control, arguments, std::move(result));
  } else {
    result->NotImplemented();
  }
}

void AppControlChannel::RegisterEventHandler(
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events) {
  event_sink_ = std::move(events);
  SendAlreadyQueuedEvents();
}

void AppControlChannel::UnregisterEventHandler() {
  event_sink_.reset();
}

void AppControlChannel::SendAlreadyQueuedEvents() {
  while (!queue_.empty()) {
    SendAppControlDataEvent(queue_.front());
    queue_.pop();
  }
}

void AppControlChannel::RegisterReplyHandler(
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events) {
  reply_sink_ = std::move(events);
}

void AppControlChannel::UnregisterReplyHandler() {
  reply_sink_.reset();
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
    FT_LOG(Info) << "Key " << key << "not found";
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
    FT_LOG(Error) << "Could not find AppControl with id " << id;
    return nullptr;
  }

  if (map_.find(id) == map_.end()) {
    FT_LOG(Error) << "Could not find AppControl with id " << id;
    return nullptr;
  }
  return map_[id];
}

void AppControlChannel::CreateAppControl(
    const EncodableValue* args,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
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

void AppControlChannel::Dispose(
    std::shared_ptr<AppControl> app_control,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  map_.erase(app_control->GetId());
  result->Success();
}

void AppControlChannel::Reply(
    std::shared_ptr<AppControl> app_control,
    const flutter::EncodableValue* arguments,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  int request_id;
  if (!GetValueFromArgs<int>(arguments, "requestId", request_id) ||
      map_.find(request_id) == map_.end()) {
    result->Error("Could not reply", "Invalid request app control");
    return;
  }

  auto request_app_control = map_[request_id];
  std::string result_str;
  if (!GetValueFromArgs<std::string>(arguments, "result", result_str)) {
    result->Error("Could not reply", "Invalid result parameter");
    return;
  }
  AppControlResult ret = app_control->Reply(request_app_control, result_str);
  if (ret) {
    result->Success();
  } else {
    result->Error("Could not reply to app control", ret.message());
  }
}

void AppControlChannel::SendLaunchRequest(
    std::shared_ptr<AppControl> app_control,
    const flutter::EncodableValue* arguments,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  bool wait_for_reply = false;
  GetValueFromArgs<bool>(arguments, "waitForReply", wait_for_reply);
  AppControlResult ret;
  if (wait_for_reply) {
    ret = app_control->SendLaunchRequestWithReply(std::move(reply_sink_), this);
  } else {
    ret = app_control->SendLaunchRequest();
  }

  if (ret) {
    result->Success();
  } else {
    result->Error(ret.message());
  }
}

void AppControlChannel::SendTerminateRequest(
    std::shared_ptr<AppControl> app_control,
    const flutter::EncodableValue* arguments,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  AppControlResult ret = app_control->SendTerminateRequest();
  if (ret) {
    result->Success();
  } else {
    result->Error("Could not terminate", ret.message());
  }
}

void AppControlChannel::SetAppControlData(
    std::shared_ptr<AppControl> app_control,
    const flutter::EncodableValue* arguments,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  std::string app_id, operation, mime, category, uri, launch_mode;
  EncodableValue extra_data;
  GetValueFromArgs<std::string>(arguments, "appId", app_id);
  GetValueFromArgs<std::string>(arguments, "operation", operation);
  GetValueFromArgs<std::string>(arguments, "mime", mime);
  GetValueFromArgs<std::string>(arguments, "category", category);
  GetValueFromArgs<std::string>(arguments, "launchMode", launch_mode);
  GetValueFromArgs<std::string>(arguments, "uri", uri);
  if (std::holds_alternative<flutter::EncodableMap>(*arguments)) {
    flutter::EncodableMap map = std::get<flutter::EncodableMap>(*arguments);
    EncodableValue key = EncodableValue("extraData");
    if (map.find(key) != map.end()) {
      extra_data = map[key];
    }
  }
  AppControlResult results[7];
  results[0] = app_control->SetAppId(app_id);
  if (!operation.empty()) {
    results[1] = app_control->SetOperation(operation);
  }
  if (!mime.empty()) {
    results[2] = app_control->SetMime(mime);
  }
  if (!category.empty()) {
    results[3] = app_control->SetCategory(category);
  }
  if (!uri.empty()) {
    results[4] = app_control->SetUri(uri);
  }
  if (!launch_mode.empty()) {
    results[5] = app_control->SetLaunchMode(launch_mode);
  }
  results[6] = app_control->SetExtraData(extra_data);
  for (int i = 0; i < 7; i++) {
    if (!results[i]) {
      result->Error("Could not set value for app control",
                    results[i].message());
    }
  }
  result->Success();
}

void AppControlChannel::SendAppControlDataEvent(
    std::shared_ptr<AppControl> app_control) {
  EncodableValue map = app_control->SerializeAppControlToMap();
  if (!map.IsNull()) {
    event_sink_->Success(map);
  }
}

AppControl::AppControl(app_control_h app_control) : id_(next_id_++) {
  handle_ = app_control;
}

AppControl::~AppControl() {
  app_control_destroy(handle_);
}

AppControlResult AppControl::GetString(std::string& str,
                                       int func(app_control_h, char**)) {
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
    FT_LOG(Error) << "app_control_is_extra_data_array() failed at key " << key;
    return false;
  }

  if (is_array) {
    char** strings = NULL;
    int length = 0;
    ret = app_control_get_extra_data_array(app, key, &strings, &length);
    if (ret != APP_CONTROL_ERROR_NONE) {
      FT_LOG(Error) << "app_control_get_extra_data() failed at key " << key;
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
      FT_LOG(Error) << "app_control_get_extra_data() failed at key " << key;
      return false;
    }
    extra_data->insert(
        {EncodableValue(std::string(key)), EncodableValue(std::string(value))});
    free(value);
  }

  return true;
}

AppControlResult AppControl::GetExtraData(EncodableValue& value) {
  EncodableMap extra_data;
  int ret = app_control_foreach_extra_data(handle_, _app_control_extra_data_cb,
                                           &extra_data);
  if (ret == APP_CONTROL_ERROR_NONE) {
    value = EncodableValue(extra_data);
  }
  return AppControlResult(ret);
}

AppControlResult AppControl::SetExtraData(EncodableValue& value) {
  if (std::holds_alternative<flutter::EncodableMap>(value)) {
    EncodableMap map = std::get<EncodableMap>(value);
    for (const auto& v : map) {
      if (!std::holds_alternative<std::string>(v.first)) {
        FT_LOG(Error) << "Key for extra data has to be string, omitting";
        continue;
      }
      std::string key = std::get<std::string>(v.first);
      AppControlResult ret = AddExtraData(key, v.second);
      if (!ret) {
      FT_LOG(Error) << "Invalid data at " << key << ", omitting";
      continue;
      }
    }
  } else {
    return AppControlResult(APP_ERROR_INVALID_PARAMETER);
  }
  return AppControlResult();
}

void AppControl::SetManager(AppControlChannel* m) {
  manager_ = m;
}

AppControlChannel* AppControl::GetManager() {
  return manager_;
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

EncodableValue AppControl::SerializeAppControlToMap() {
  std::string app_id, operation, mime, category, uri, caller_id, launch_mode;
  AppControlResult results[7];
  results[0] = GetAppId(app_id);
  results[1] = GetOperation(operation);
  results[2] = GetMime(mime);
  results[3] = GetCategory(category);
  results[4] = GetUri(uri);
  results[5] = GetLaunchMode(launch_mode);
  // Caller Id is optional
  GetCaller(caller_id);
  EncodableValue extra_data;
  results[6] = GetExtraData(extra_data);
  for (int i = 0; i < 7; i++) {
    if (!results[i]) {
      return EncodableValue();
    }
  }
  EncodableMap map;
  map[EncodableValue("id")] = EncodableValue(GetId());
  map[EncodableValue("appId")] = EncodableValue(app_id);
  map[EncodableValue("operation")] = EncodableValue(operation);
  map[EncodableValue("mime")] = EncodableValue(mime);
  map[EncodableValue("category")] = EncodableValue(category);
  map[EncodableValue("uri")] = EncodableValue(uri);
  map[EncodableValue("callerId")] = EncodableValue(caller_id);
  map[EncodableValue("launchMode")] = EncodableValue(launch_mode);
  map[EncodableValue("extraData")] = extra_data;

  return EncodableValue(map);
}

AppControlResult AppControl::SendLaunchRequest() {
  AppControlResult ret =
      app_control_send_launch_request(handle_, nullptr, nullptr);
  return ret;
}

AppControlResult AppControl::SendLaunchRequestWithReply(
    std::shared_ptr<EventSink<EncodableValue>> reply_sink,
    AppControlChannel* manager) {
  SetManager(manager);
  auto on_reply = [](app_control_h request, app_control_h reply,
                     app_control_result_e result, void* user_data) {
    AppControl* app_control = static_cast<AppControl*>(user_data);
    app_control_h clone = nullptr;
    AppControlResult ret = app_control_clone(&clone, reply);
    if (!ret) {
      FT_LOG(Error) << "Could not clone app_control: " << ret.message();
      return;
    }

    std::shared_ptr<AppControl> app_control_reply =
        std::make_shared<AppControl>(clone);
    EncodableMap map;
    map[EncodableValue("id")] = EncodableValue(app_control->GetId());
    map[EncodableValue("reply")] =
        app_control_reply->SerializeAppControlToMap();
    if (result == APP_CONTROL_RESULT_APP_STARTED) {
      map[EncodableValue("result")] = EncodableValue("AppStarted");
    } else if (result == APP_CONTROL_RESULT_SUCCEEDED) {
      map[EncodableValue("result")] = EncodableValue("Succeeded");
    } else if (result == APP_CONTROL_RESULT_FAILED) {
      map[EncodableValue("result")] = EncodableValue("Failed");
    } else if (result == APP_CONTROL_RESULT_CANCELED) {
      map[EncodableValue("result")] = EncodableValue("Cancelled");
    }

    app_control->reply_sink_->Success(EncodableValue(map));
    app_control->GetManager()->AddExistingAppControl(
        std::move(app_control_reply));
  };
  reply_sink_ = std::move(reply_sink);
  AppControlResult ret =
      app_control_send_launch_request(handle_, on_reply, this);
  return ret;
}

AppControlResult AppControl::SendTerminateRequest() {
  AppControlResult ret = app_control_send_terminate_request(handle_);
  return ret;
}

AppControlResult AppControl::Reply(std::shared_ptr<AppControl> reply,
                                   const std::string& result) {
  app_control_result_e result_e;
  if (result == "AppStarted") {
    result_e = APP_CONTROL_RESULT_APP_STARTED;
  } else if (result == "Succeeded") {
    result_e = APP_CONTROL_RESULT_SUCCEEDED;
  } else if (result == "Failed") {
    result_e = APP_CONTROL_RESULT_FAILED;
  } else if (result == "Cancelled") {
    result_e = APP_CONTROL_RESULT_CANCELED;
  } else {
    return AppControlResult(APP_CONTROL_ERROR_INVALID_PARAMETER);
  }
  AppControlResult ret = app_control_reply_to_launch_request(
      reply->Handle(), this->handle_, result_e);
  return ret;
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
