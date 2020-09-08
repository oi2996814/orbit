// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "DataManager.h"

#include "ModuleData.h"
#include "OrbitBase/Logging.h"
#include "ProcessData.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

using orbit_client_protos::FunctionInfo;
using orbit_grpc_protos::ModuleInfo;
using orbit_grpc_protos::ProcessInfo;
using orbit_grpc_protos::TracepointInfo;

void DataManager::UpdateProcessInfos(const std::vector<ProcessInfo>& process_infos) {
  CHECK(std::this_thread::get_id() == main_thread_id_);

  // Note that at this point the data manager does not remove old processes.
  // To do it correctly we may need to implement some callback logic here
  // since the ProcessData can be in use by some views.
  for (const ProcessInfo& info : process_infos) {
    int32_t process_id = info.pid();
    auto it = process_map_.find(process_id);
    if (it != process_map_.end()) {
      it->second->SetProcessInfo(info);
    } else {
      auto [inserted_it, success] =
          process_map_.try_emplace(process_id, std::make_unique<ProcessData>(info));
      CHECK(success);
    }
  }
}

void DataManager::UpdateModuleInfos(int32_t process_id,
                                    const std::vector<ModuleInfo>& module_infos) {
  CHECK(std::this_thread::get_id() == main_thread_id_);

  for (const auto& module_info : module_infos) {
    if (module_map_.find(module_info.file_path()) == module_map_.end()) {
      const auto [inserted_it, success] = module_map_.try_emplace(
          module_info.file_path(), std::make_unique<ModuleData>(module_info));
      CHECK(success);
    }
  }

  auto it = process_map_.find(process_id);
  CHECK(it != process_map_.end());

  it->second->UpdateModuleInfos(module_infos);
}

void DataManager::SelectFunction(uint64_t function_address) {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  if (!selected_functions_.contains(function_address)) {
    selected_functions_.insert(function_address);
  }
}

void DataManager::DeselectFunction(uint64_t function_address) {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  selected_functions_.erase(function_address);
}

void DataManager::set_visible_functions(absl::flat_hash_set<uint64_t> visible_functions) {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  visible_functions_ = std::move(visible_functions);
}

void DataManager::set_selected_thread_id(int32_t thread_id) {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  selected_thread_id_ = thread_id;
}

bool DataManager::IsFunctionVisible(uint64_t function_address) const {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  return visible_functions_.contains(function_address);
}

int32_t DataManager::selected_thread_id() const {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  return selected_thread_id_;
}

const TextBox* DataManager::selected_text_box() const {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  return selected_text_box_;
}

void DataManager::set_selected_text_box(const TextBox* text_box) {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  selected_text_box_ = text_box;
}

void DataManager::ClearSelectedFunctions() {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  selected_functions_ = absl::flat_hash_set<uint64_t>();
}

ProcessData* DataManager::GetProcessByPid(int32_t process_id) const {
  CHECK(std::this_thread::get_id() == main_thread_id_);

  auto it = process_map_.find(process_id);
  if (it == process_map_.end()) {
    return nullptr;
  }

  return it->second.get();
}

[[nodiscard]] ModuleData* DataManager::GetMutableModuleByPath(const std::string& path) const {
  CHECK(std::this_thread::get_id() == main_thread_id_);

  auto it = module_map_.find(path);
  if (it == module_map_.end()) {
    return nullptr;
  }

  return it->second.get();
}

bool DataManager::IsFunctionSelected(uint64_t function_address) const {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  return selected_functions_.contains(function_address);
}

const absl::flat_hash_set<uint64_t>& DataManager::selected_functions() const {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  return selected_functions_;
}

void DataManager::set_selected_process(std::shared_ptr<Process> process) {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  CHECK(process != nullptr);
  selected_process_ = std::move(process);
}

const std::shared_ptr<Process>& DataManager::selected_process() const {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  return selected_process_;
}

void DataManager::SelectTracepoint(const TracepointInfo& info) {
  if (!IsTracepointSelected(info)) selected_tracepoints_.emplace(info);
}

void DataManager::DeselectTracepoint(const TracepointInfo& info) {
  CHECK(IsTracepointSelected(info));
  selected_tracepoints_.erase(info);
}

bool DataManager::IsTracepointSelected(const TracepointInfo& info) const {
  return selected_tracepoints_.contains(info);
}

const TracepointInfoSet& DataManager::selected_tracepoints() const { return selected_tracepoints_; }

[[nodiscard]] ModuleData* DataManager::FindModuleByAddress(int32_t process_id,
                                                           uint64_t absolute_address) {
  CHECK(std::this_thread::get_id() == main_thread_id_);

  const ProcessData* process = GetProcessByPid(process_id);
  if (process == nullptr) return nullptr;

  const auto& result = process->FindModuleByAddress(absolute_address);
  if (!result) return nullptr;

  const std::string& module_path = result.value().first;

  return module_map_.at(module_path).get();
}

const FunctionInfo* DataManager::FindFunctionByAddress(int32_t process_id,
                                                       uint64_t absolute_address,
                                                       bool is_exact) const {
  CHECK(std::this_thread::get_id() == main_thread_id_);

  const ProcessData* process = GetProcessByPid(process_id);
  if (process == nullptr) return nullptr;

  const auto& result = process->FindModuleByAddress(absolute_address);
  if (!result) return nullptr;

  const std::string& module_path = result.value().first;
  const uint64_t module_base_address = result.value().second;

  const ModuleData* module = module_map_.at(module_path).get();

  const uint64_t relative_address = absolute_address - module_base_address;
  return module->FindFunctionByRelativeAddress(relative_address, is_exact);
}
