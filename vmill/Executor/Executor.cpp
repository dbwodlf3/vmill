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

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

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
#include "vmill/Workspace/Workspace.h"

//#include "vmill/Workspace/Workspace.h"
 
namespace vmill {
namespace {

//void MoveGlobalFuncsToBC(void) {
//  //Potentially keep global llvm var and translate this over to the file that I dump code to
//  llvm::LLVMContext context;
//  llvm::Module module("lifted_code", context);
//  //std::stringstream ss;
//  //ss << Workspace::BitcodeDir() << remill::PathSeparator()
//  //  << remill::ModuleName(&module);
//  auto file_name = "bitcodecache.bc";  //ss.str();
//  auto arch = remill::GetTargetArch();
//  arch->PrepareModuleDataLayout(&module);
//
//  for (const auto& f : gLiftedFuncs) {
//    remill::MoveFunctionIntoModule(f.second.lifted_func, &module);
//  }
//
//  remill::StoreModuleToFile(&module, file_name);
//}

} //namespace

Executor::Executor(void)
    : context(new llvm::LLVMContext),
      lifted_code(remill::LoadModuleFromFile(
          context.get(),
          Workspace::RuntimeBitcodePath(),
          false  /* allow_failure */)),
      trace_manager(*lifted_code.get()),
      lifter(*lifted_code.get(), trace_manager){}
 
void Executor::SetUp(void) {}

void Executor::TearDown(void) {}

Executor::~Executor(void) {

  // Reset all task vars to have null initializers.
  for (unsigned i = 0; ; i++) {
    const std::string task_var_name = "__vmill_task_" + std::to_string(i);
    const auto task_var = lifted_code->getGlobalVariable(task_var_name);
    if (!task_var) {
      break;
    }
    task_var->setInitializer(llvm::Constant::getNullValue(
        task_var->getInitializer()->getType()));
  }

  remill::StoreModuleToFile(
      lifted_code.get(),
      Workspace::LocalRuntimeBitcodePath(),
      false);
}

void Executor::Run(void){
  SetUp();
  //for (const auto &info : initial_tasks){
    
  //}
  TearDown();
}

void Executor::AddInitialTask(const std::string &state, PC pc,
                              std::shared_ptr<AddressSpace> memory) {
  CHECK(memories.empty());

  auto task_num = memories.size();
  memories.push_back(memory);

  const std::string task_var_name = "__vmill_task_" + std::to_string(task_num);
  const auto task_var = lifted_code->getGlobalVariable(task_var_name);
  CHECK(task_var != nullptr)
      << "Missing task variable " << task_var_name << " in runtime";

  auto task_struct_type = llvm::dyn_cast<llvm::StructType>(
      task_var->getInitializer()->getType());
  CHECK(task_struct_type)
      << "Task variable " << task_var_name << " must have a vmill::Task type";

  auto elem_types = task_struct_type->elements();
  CHECK_GE(elem_types.size(), 3)
      << "Task structure type for " << task_var_name << " should have at least "
      << "three elements";

  auto pc_type = llvm::dyn_cast<llvm::IntegerType>(elem_types[0]);
  CHECK(pc_type != nullptr)
      << "First element type of " << task_var_name << " should be integral";

  auto mem_type = llvm::dyn_cast<llvm::IntegerType>(elem_types[1]);
  CHECK(mem_type != nullptr)
      << "Second element type of " << task_var_name << " should be integral";

  auto state_type = llvm::dyn_cast<llvm::ArrayType>(elem_types[0]);
  CHECK(state_type != nullptr)
      << "Third element type of " << task_var_name << " should be an array";

  auto el_type = state_type->getArrayElementType();
  auto u8_type = llvm::Type::getInt8Ty(*context);
  CHECK_EQ(el_type, u8_type)
      << "Third element type of " << task_var_name
      << " should be an array of uint8_t";

  CHECK_EQ(state_type->getArrayNumElements(), state.size())
      << "State structure data from protobuf has " << state.size()
      << "bytes, but runtime state structure needs "
      << state_type->getArrayNumElements();

  std::vector<llvm::Constant *> initial_vals;
  initial_vals.push_back(
      llvm::ConstantInt::get(pc_type, static_cast<uint64_t>(pc)));
  initial_vals.push_back(llvm::ConstantInt::get(mem_type, task_num));
  initial_vals.push_back(
      llvm::ConstantDataArray::getRaw(state, state.size(), u8_type));

  // Fill out the rest of the task structure with zero-initialization.
  for (size_t i = 3; i < elem_types.size(); ++i) {
    initial_vals.push_back(llvm::Constant::getNullValue(elem_types[i]));
  }

  // Initialize this task with the data from the snapshot.
  task_var->setInitializer(
      llvm::ConstantStruct::get(task_struct_type, initial_vals));
}

LiftedBitcodeInfo Executor::GetLiftedFunction(Task *task) {
  const auto memory = task->memory;
  const auto pc = task->pc;
  const auto task_pc_uint = static_cast<uint64_t>(pc);

  LiftedBitcodeInfo info;

  info.pc = pc;
  info.version = 0;
  info.lifted_func = trace_manager.GetLiftedTraceDefinition(task_pc_uint);

  if (!info.lifted_func) {
    info.lifted_func = lifter.Lift(memory, task_pc_uint);
  }

  return info;
}

}  //namespace vmill
