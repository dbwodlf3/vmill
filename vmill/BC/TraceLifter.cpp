/*
 * Copyright (c) 2017 Trail of Bits, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TraceLifter.h"

namespace vmill {
void VmillTraceManager::ForEachDevirtualizedTarget(
    const remill::Instruction &inst,
    std::function<void(uint64_t, remill::DevirtualizedTargetKind)> func){}

VmillTraceManager::VmillTraceManager(AddressSpace &addr_space)
    : memory(addr_space) {}
 
bool VmillTraceManager::TryReadExecutableByte(uint64_t addr, uint8_t *byte){
  return memory.TryReadExecutable(static_cast<PC>(addr), byte);
}

void VmillTraceManager::SetLiftedTraceDefinition(
        uint64_t addr, llvm::Function *lifted_func){
  traces[addr] = lifted_func;
}

llvm::Function *VmillTraceManager::GetLiftedTraceDeclaration(uint64_t addr){
  auto trace_it = traces.find(addr);
  if (trace_it != traces.end()){
    return trace_it -> second;
  } else {
    return nullptr;
  }
}

llvm::Function *VmillTraceManager::GetLiftedTraceDefinition(uint64_t addr){
  return GetLiftedTraceDeclaration(addr);
}

VmillTraceLifter::VmillTraceLifter(remill::InstructionLifter *inst_lifter_,
                 VmillTraceManager *manager_)
    : Lifter(),
      remill::TraceLifter(inst_lifter_, manager_),
      manager_ptr(std::shared_ptr<VmillTraceManager>(manager_)){}

 VmillTraceLifter::VmillTraceLifter(const std::shared_ptr<llvm::LLVMContext> &context_):
   Lifter(),
   remill::TraceLifter(nullptr, nullptr),
   manager_ptr(nullptr) {
     std::unique_ptr<llvm::Module> module(remill::LoadTargetSemantics(context_.get()));
     auto arch = remill::GetTargetArch();
     thread_local AddressSpace memory;
     auto tm = new VmillTraceManager(memory);
     manager_ptr = std::shared_ptr<VmillTraceManager>(tm);
     remill::IntrinsicTable intrinsics(module);
     remill::InstructionLifter inst_lifter(arch, intrinsics);
     remill::TraceLifter(inst_lifter, *tm);
}

std::unique_ptr<llvm::Module> VmillTraceLifter::VmillLift(uint64_t addr_) {
  //assumes remill::TraceLifter has all protected fields and no private fields
  remill::TraceLifter::Lift(addr_);
  remill::OptimizationGuide guide = {};
  guide.slp_vectorize = true;
  guide.loop_vectorize = true;
  guide.verify_input = true;
  guide.eliminate_dead_stores = false; //avoids buggy DSE for now

  remill::OptimizeModule(module, manager_ptr->traces, guide);
  llvm::Module dest_module("lifted_code", context);
  arch->PrepareModuleDataLayout(&dest_module);

  for (auto &lifted_entry: manager_ptr->traces) {
    remill::MoveFunctionIntoModule(lifted_entry.second, &dest_module);
  }

  return std::unique_ptr<llvm::Module>(&dest_module);
}

std::unique_ptr<llvm::Module> VmillTraceLifter::Lift(uint64_t addr_){
  return VmillLift(addr_);
}

std::unique_ptr<Lifter> Lifter::Create(
    const std::shared_ptr<llvm::LLVMContext> &context){
    return std::unique_ptr<Lifter>(new VmillTraceLifter(context));
}

Lifter::Lifter(void){}
Lifter::~Lifter(void){}

} //namespace vmill
