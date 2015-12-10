//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include "service/ipc/binder/bluetooth_low_energy_binder_server.h"

#include <base/logging.h>

#include "service/adapter.h"

namespace ipc {
namespace binder {

namespace {
const int kInvalidInstanceId = -1;
}  // namespace

BluetoothLowEnergyBinderServer::BluetoothLowEnergyBinderServer(
    bluetooth::Adapter* adapter) : adapter_(adapter) {
  CHECK(adapter_);
}

BluetoothLowEnergyBinderServer::~BluetoothLowEnergyBinderServer() {
}

bool BluetoothLowEnergyBinderServer::RegisterClient(
    const android::sp<IBluetoothLowEnergyCallback>& callback) {
  VLOG(2) << __func__;
  bluetooth::LowEnergyClientFactory* ble_factory =
      adapter_->GetLowEnergyClientFactory();

  return RegisterInstanceBase(callback, ble_factory);
}

void BluetoothLowEnergyBinderServer::UnregisterClient(int client_id) {
  VLOG(2) << __func__;
  UnregisterInstanceBase(client_id);
}

void BluetoothLowEnergyBinderServer::UnregisterAll() {
  VLOG(2) << __func__;
  UnregisterAllBase();
}

bool BluetoothLowEnergyBinderServer::StartMultiAdvertising(
    int client_id,
    const bluetooth::AdvertiseData& advertise_data,
    const bluetooth::AdvertiseData& scan_response,
    const bluetooth::AdvertiseSettings& settings) {
  VLOG(2) << __func__ << " client_id: " << client_id;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto client = GetLEClient(client_id);
  if (!client) {
    LOG(ERROR) << "Unknown client_id: " << client_id;
    return false;
  }

  // Create a weak pointer and pass that to the callback to prevent a potential
  // use after free.
  android::wp<BluetoothLowEnergyBinderServer> weak_ptr_to_this(this);
  auto settings_copy = settings;
  auto callback = [=](bluetooth::BLEStatus status) {
    auto sp_to_this = weak_ptr_to_this.promote();
    if (!sp_to_this.get()) {
      VLOG(2) << "BluetoothLowEnergyBinderServer was deleted";
      return;
    }

    std::lock_guard<std::mutex> lock(*maps_lock());

    auto cb = GetLECallback(client_id);
    if (!cb.get()) {
      VLOG(1) << "Client was removed before callback: " << client_id;
      return;
    }

    cb->OnMultiAdvertiseCallback(status, true /* is_start */, settings_copy);
  };

  if (!client->StartAdvertising(
      settings, advertise_data, scan_response, callback)) {
    LOG(ERROR) << "Failed to initiate call to start advertising";
    return false;
  }

  return true;
}

bool BluetoothLowEnergyBinderServer::StopMultiAdvertising(int client_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto client = GetLEClient(client_id);
  if (!client) {
    LOG(ERROR) << "Unknown client_id: " << client_id;
    return false;
  }

  // Create a weak pointer and pass that to the callback to prevent a potential
  // use after free.
  android::wp<BluetoothLowEnergyBinderServer> weak_ptr_to_this(this);
  auto settings_copy = client->settings();
  auto callback = [=](bluetooth::BLEStatus status) {
    auto sp_to_this = weak_ptr_to_this.promote();
    if (!sp_to_this.get()) {
      VLOG(2) << "BluetoothLowEnergyBinderServer was deleted";
      return;
    }

    auto cb = GetLECallback(client_id);
    if (!cb.get()) {
      VLOG(2) << "Client was unregistered - client_id: " << client_id;
      return;
    }

    std::lock_guard<std::mutex> lock(*maps_lock());

    cb->OnMultiAdvertiseCallback(status, false /* is_start */, settings_copy);
  };

  if (!client->StopAdvertising(callback)) {
    LOG(ERROR) << "Failed to initiate call to start advertising";
    return false;
  }

  return true;
}

android::sp<IBluetoothLowEnergyCallback>
BluetoothLowEnergyBinderServer::GetLECallback(int client_id) {
  auto cb = GetCallback(client_id);
  return android::sp<IBluetoothLowEnergyCallback>(
      static_cast<IBluetoothLowEnergyCallback*>(cb.get()));
}

std::shared_ptr<bluetooth::LowEnergyClient>
BluetoothLowEnergyBinderServer::GetLEClient(int client_id) {
  return std::static_pointer_cast<bluetooth::LowEnergyClient>(
      GetInstance(client_id));
}

void BluetoothLowEnergyBinderServer::OnRegisterInstanceImpl(
    bluetooth::BLEStatus status,
    android::sp<IInterface> callback,
    bluetooth::BluetoothInstance* instance) {
  VLOG(1) << __func__ << " status: " << status;
  android::sp<IBluetoothLowEnergyCallback> cb(
      static_cast<IBluetoothLowEnergyCallback*>(callback.get()));
  cb->OnClientRegistered(
      status,
      (status == bluetooth::BLE_STATUS_SUCCESS) ?
          instance->GetInstanceId() : kInvalidInstanceId);
}

}  // namespace binder
}  // namespace ipc