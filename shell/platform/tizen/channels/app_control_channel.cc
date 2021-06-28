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
  FT_LOGD("AppControlChannel");
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
            FT_LOGD("OnListen");
            RegisterEventHandler(std::move(events));
            return nullptr;
          },
          [this](const flutter::EncodableValue* arguments)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            FT_LOGD("OnCancel");
            UnregisterEventHandler();
            return nullptr;
          });

  event_channel_->SetStreamHandler(std::move(event_channel_handler));
}

AppControlChannel::~AppControlChannel() {}

void AppControlChannel::NotifyAppControl(app_control_h app_control) {
  FT_LOGD("NotifyAppControl");
  auto app = std::make_unique<AppControl>(app_control);
  if (!events_) {
    queue_.push(std::move(app));
    FT_LOGE("EventChannel not set yet");
  } else {
    events_->Success(EncodableValue(app->GetOperation()));
  }
}

void AppControlChannel::HandleMethodCall(
    const MethodCall<EncodableValue>& method_call,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  FT_LOGD("HandleMethodCall : %s", method_call.method_name().data());
  // const auto& arguments = *method_call.arguments();

  if (method_call.method_name().compare("GetOperation") == 0) {
    GetOperation(std::move(result));
  } else {
    result->NotImplemented();
  }
}

void AppControlChannel::RegisterEventHandler(
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events) {
  events_ = std::move(events);
  SendAlreadyQueuedEvents();
}

void AppControlChannel::UnregisterEventHandler() {
  events_.reset();
}

void AppControlChannel::SendAlreadyQueuedEvents() {
  while (!queue_.empty()) {
    events_->Success(EncodableValue(queue_.front()->GetOperation()));
    queue_.pop();
  }
}

void AppControlChannel::GetOperation(
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  if (queue_.empty()) {
    result->Error("No app_control");
    return;
  }
  std::string operation = queue_.front()->GetOperation();
  if (operation.empty()) {
    result->Error("Could not get operation");
    return;
  }
  result->Success(EncodableValue(operation));
  return;
}

}  // namespace flutter
