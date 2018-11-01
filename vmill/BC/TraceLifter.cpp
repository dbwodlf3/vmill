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

#include <iostream>
#include "TraceLifter.h"

namespace vmill {
namespace {
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
    : remill::TraceLifter(inst_lifter_, manager_) {}


std::unique_ptr<llvm::Module> VmillTraceLifter::VmillLift(uint64_t addr_,
        VmillTraceManager *manager) {
  //assumes remill::TraceLifter has all protected fields and no private fields
  remill::TraceLifter::Lift(addr_);
  remill::OptimizationGuide guide = {};
  guide.slp_vectorize = true;
  guide.loop_vectorize = true;
  guide.verify_input = true;
  guide.eliminate_dead_stores = false; //avoids buggy DSE for now
  remill::OptimizeModule(module, manager->traces, guide);

  llvm::Module dest_module("lifted_code", context);
  arch->PrepareModuleDataLayout(&dest_module);

  for (auto &lifted_entry: manager->traces) {
    remill::MoveFunctionIntoModule(lifted_entry.second, &dest_module);
  }

  return std::unique_ptr<llvm::Module>(&dest_module);
}

} //namespace

std::unique_ptr<VmillTraceLifter> VmillTraceLifter::Create(
        std::unique_ptr<llvm::LLVMContext> &context_){
    
    std::unique_ptr<llvm::Module> module(remill::LoadTargetSemantics(context_.get()));
    auto arch = remill::GetTargetArch();
    thread_local AddressSpace memory;
    thread_local VmillTraceManager manager(memory);
    thread_local remill::IntrinsicTable intrinsics(module);
    thread_local remill::InstructionLifter inst_lifter(arch, intrinsics);

    return std::unique_ptr<VmillTraceLifter>(
            new VmillTraceLifter(inst_lifter, manager));
}

} //namespace vmill


using namespace vmill;

int main(){
  std::unique_ptr<llvm::LLVMContext> context;
  std::cout << "Hello World" << std::endl;
}
