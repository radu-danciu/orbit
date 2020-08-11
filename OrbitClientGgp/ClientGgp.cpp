// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ClientGgp.h"

#include "capture_data.pb.h"

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>

#include "Capture.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/Result.h"

ClientGgp::ClientGgp(ClientGgpOptions&& options) 
    : options_(std::move(options)) {}

bool ClientGgp::InitClient() {
  if (options_.grpc_server_address.empty()){
    ERROR("gRPC server address not provided");
    return false;
  }

  // Create channel
  grpc::ChannelArguments channel_arguments;
  channel_arguments.SetMaxReceiveMessageSize(
      std::numeric_limits<int32_t>::max());

  grpc_channel_ = grpc::CreateCustomChannel(
      options_.grpc_server_address, 
      grpc::InsecureChannelCredentials(), 
      channel_arguments);
  if (!grpc_channel_) {
    ERROR("Unable to create GRPC channel to %s",
          options_.grpc_server_address);
    return false;
  }
  LOG("Created GRPC channel to %s", options_.grpc_server_address);

  // Initialisations needed for capture to work
  InitCapture();

  capture_client_ = std::make_unique<CaptureClient>(grpc_channel_, this);
  return true;
}

// Prepare the Capture object to be able to request a capture
bool ClientGgp::PrepareStartCapture() {
  CHECK(!Capture::IsCapturing());

  ErrorMessageOr<void> result = Capture::StartCapture();
  if (result.has_error()) {
    ERROR("Error starting capture %s", result.error().message());
    return false;
  }
  return true;
}

// Client requests to start the capture
void ClientGgp::RequestStartCapture() {
  int32_t pid = Capture::GProcessId;
  LOG("Capture pid %d", pid);

  // TODO: selected_functions available when UploadSymbols is included
  std::map<uint64_t, orbit_client_protos::FunctionInfo*> selected_functions =
      Capture::GSelectedFunctionsMap;
  capture_client_->Capture(pid, selected_functions);
}

void ClientGgp::StopCapture() {
  CHECK(Capture::IsCapturing());
  Capture::StopCapture();

  capture_client_->StopCapture();
  LOG("Stop capture requested");
}

void ClientGgp::InitCapture() {
  Capture::Init();
  std::shared_ptr<Process> process = GetOrbitProcessByPid(options_.capture_pid);
  CHECK(process != nullptr);
  Capture::SetTargetProcess(std::move(process));
}

std::shared_ptr<Process> ClientGgp::GetOrbitProcessByPid(int32_t pid) {
  std::shared_ptr<Process> process = std::make_shared<Process>();
  // TODO: study if we need more information. Requires extra steps
  process->SetID(pid);
  return process;
}

// CaptureListener implementation
void ClientGgp::OnCaptureStarted() {
  LOG("Capture started");
}

void ClientGgp::OnCaptureComplete() {
  LOG("Capture completed");
}

void ClientGgp::OnTimer(const orbit_client_protos::TimerInfo&) {}

void ClientGgp::OnKeyAndString(uint64_t, std::string) {}

void ClientGgp::OnCallstack(CallStack) {}

void ClientGgp::OnCallstackEvent(orbit_client_protos::CallstackEvent) {}

void ClientGgp::OnThreadName(int32_t, std::string) {}

void ClientGgp::OnAddressInfo(orbit_client_protos::LinuxAddressInfo) {}