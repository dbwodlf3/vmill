/*
 * Copyright (c) 2018 Trail of Bits, Inc.
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

#include <glog/logging.h>
#include <gflags/gflags.h>

#include "remill/BC/Util.h"
#include "remill/OS/FileSystem.h"
#include "remill/Arch/Arch.h"
#include "remill/Arch/Instruction.h"
#include "remill/Arch/Name.h"
#include "remill/BC/Util.h"
#include "remill/OS/OS.h"

#include "vmill/Executor/Executor.h"

#include "vmill/BC/TraceLifter.h"
#include "vmill/Program/AddressSpace.h"
#include "vmill/Arch/Arch.h"

//#include "vmill/Workspace/Workspace.h"

DECLARE_uint64(num_io_threads);
DECLARE_string(tool);
 
DEFINE_uint64(num_lift_threads, 1,
               "Number of threads that can be used for lifting.");
 
namespace vmill {

thread_local Executor *gExecutor = nullptr;
LiftedFunctions global_function_map;

namespace {
void MoveGlobalFuncsToBC(void){
  //Potentially keep global llvm var and translate this over to the file that I dump code to
  llvm::LLVMContext context;
  llvm::Module module("lifted_code",context);
  //std::stringstream ss;
  //ss << Workspace::BitcodeDir() << remill::PathSeparator()
  //  << remill::ModuleName(&module);
  auto file_name = "bitcodecache.bc";//ss.str();
  auto arch = remill::GetTargetArch(); 
  arch->PrepareModuleDataLayout(&module);
  
  for (const auto& f: global_function_map){
    remill::MoveFunctionIntoModule(f.second.lifted_func, &module); 
  }
  remill::StoreModuleToFile(&module, file_name); 
}

} //namespace

Executor::Executor(void)
    : context(new llvm::LLVMContext),
      lifted_code(new llvm::Module("lifted_code", *context.get())),
      trace_manager(*lifted_code.get()),
      lifter(*lifted_code.get(), trace_manager){}
 

void Executor::SetUp(void){
  gExecutor = this;

}

void Executor::TearDown(void) {
  gExecutor = nullptr;
}   

Executor::~Executor(void) {}

void Executor::Run(void){
  SetUp();
  //for (const auto &info : initial_tasks){
    
  //}
  TearDown();
}

void Executor::AddInitialTask(const std::string &state, PC pc,
        std::shared_ptr<AddressSpace> memory){
  InitialTaskInfo info = {state, pc, memory};
  initial_tasks.push_back(std::move(info));
}

LiftedBitcodeInfo Executor::GetLiftedFunction(Task *task) {
  const auto memory = task->memory;
  const auto pc = task->pc;
  const auto task_pc_uint = static_cast<uint64_t>(pc);

  LiftedBitcodeInfo lifted_info;
  if (global_function_map.find(pc) != global_function_map.end()){
    return lifted_info;
  }

  auto lifted_trace = 
      trace_manager.GetLiftedTraceDefinition(task_pc_uint);

  if (!lifted_trace){
      auto lifted_trace = lifter.Lift(memory, task_pc_uint);
  }
  lifted_info.pc = pc; 
  lifted_info.lifted_func = lifted_trace;
  
  return lifted_info;
}

}  //namespace vmill
