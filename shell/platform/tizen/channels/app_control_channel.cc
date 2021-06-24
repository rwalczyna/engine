// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app_control_channel.h"

#include "flutter/shell/platform/tizen/tizen_log.h"

namespace flutter {

static constexpr char kChannelName[] = "tizen/app_control";

AppControlChannel::AppControlChannel(BinaryMessenger* messenger)
    : app_control_(nullptr) {
  FT_LOGD("AppControlChannel");
  method_channel_ = std::make_unique<MethodChannel<EncodableValue>>(
      messenger, kChannelName, &StandardMethodCodec::GetInstance());

  method_channel_->SetMethodCallHandler([this](const auto& call, auto result) {
    this->HandleMethodCall(call, std::move(result));
  });
}

AppControlChannel::~AppControlChannel() {}

void AppControlChannel::NotifyAppControl(app_control_h app_control) {
  FT_LOGD("NotifyAppControl");
  int ret = app_control_clone(&app_control_, app_control);
  if (ret != APP_CONTROL_ERROR_NONE) {
    FT_LOGE("Could not clone app control handle");
    return;
  }
}

void AppControlChannel::HandleMethodCall(
    const MethodCall<EncodableValue>& method_call,
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  FT_LOGD("HandleMethodCall : %s", method_call.method_name().data());
  // const auto& arguments = *method_call.arguments();

  if (method_call.method_name().compare("getOperation") == 0) {
    getOperation(std::move(result));
  } else {
    result->NotImplemented();
  }
}

void AppControlChannel::getOperation(
    std::unique_ptr<MethodResult<EncodableValue>> result) {
  char* op;
  int ret = app_control_get_operation(app_control_, &op);
  if (ret != APP_CONTROL_ERROR_NONE) {
    result->Error("Could not get operation");
    return;
  }
  result->Success(EncodableValue(std::string(op)));
  free(op);
}

}  // namespace flutter
