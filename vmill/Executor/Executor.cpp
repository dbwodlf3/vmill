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

#include "vmill/Executor/Executor.h"
#include "vmill/Executor/Interpreter.h"

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Type.h>

#include "remill/BC/Util.h"
#include "remill/OS/FileSystem.h"
#include "remill/Arch/Arch.h"
#include "remill/Arch/Instruction.h"
#include "remill/Arch/Name.h"
#include "remill/BC/Util.h"
#include "remill/OS/OS.h"

#include "vmill/BC/TraceLifter.h"
#include "vmill/Program/AddressSpace.h"
#include "vmill/Arch/Arch.h"
#include "vmill/Workspace/Workspace.h"

namespace vmill {
namespace {

static llvm::Module *LoadRuntimeBitcode(llvm::LLVMContext *context) {
  auto &runtime_bitcode_path = Workspace::RuntimeBitcodePath();
  LOG(INFO)
      << "Loading runtime bitcode file from " << runtime_bitcode_path;
  return remill::LoadModuleFromFile(context, runtime_bitcode_path,
                                    false  /* allow_failure */);
}

}  // namespace

Executor::Executor(void)
    : context(new llvm::LLVMContext),
      lifted_code(LoadRuntimeBitcode(context.get())),
      trace_manager(*lifted_code),
      lifter(*lifted_code, trace_manager),
      interpreter(Interpreter::CreateConcrete(lifted_code, this)) {}
 
void Executor::SetUp(void) {}

void Executor::TearDown(void) {}

Executor::~Executor(void) {

  // Reset all task vars to have null initializers.
  for (unsigned i = 0; ; i++) {
    const std::string task_var_name = "__vmill_task_" + std::to_string(i);
    const auto task_var = lifted_code -> getGlobalVariable(task_var_name);
    if (!task_var) {
      break;
    }
    task_var->setInitializer(llvm::Constant::getNullValue(
        task_var->getInitializer()->getType()));
  }

  // Save the runtime, including lifted bitcode, into the workspace. Next
  // execution will load up this file.
  remill::StoreModuleToFile(
      lifted_code,
      Workspace::LocalRuntimeBitcodePath(),
      false);
}

void Executor::Run(void) {
  SetUp();
  while (auto task = NextTask()) {
    interpreter->Interpret(task);
  }
  TearDown();
}

void Executor::AddInitialTask(const std::string &state, const uint64_t pc,
                              std::shared_ptr<AddressSpace> memory) {
  CHECK(memories.empty());

  const auto task_num = memories.size();
  memories.push_back(memory);

  const std::string task_var_name = "__vmill_task_" + std::to_string(task_num);
  auto task_var = lifted_code->getGlobalVariable(task_var_name);

  // Lazily create the task variable if it's missing.
  if (!task_var) {
    CHECK(task_num)
        << "Missing task variable " << task_var_name << " in runtime";

    // Make sure that task variables are no gaps in the ordering of task
    // variables.
    const std::string prev_task_var_name =
        "__vmill_task_" + std::to_string(task_num - 1);
    const auto prev_task_var = lifted_code->getGlobalVariable(
        prev_task_var_name);
    CHECK(prev_task_var != nullptr)
        << "Missing task variable " << prev_task_var_name << " in runtime";

    task_var = new llvm::GlobalVariable(
        *lifted_code, prev_task_var->getValueType(), false /* isConstant */,
        llvm::GlobalValue::ExternalLinkage,
        llvm::Constant::getNullValue(prev_task_var->getValueType()),
        task_var_name);
  }

  auto task_struct_type = llvm::dyn_cast<llvm::StructType>(
      task_var->getInitializer()->getType());
  CHECK(task_struct_type)
      << "Task variable " << task_var_name << " must have a vmill::Task type";

  auto elem_types = task_struct_type->elements();
  CHECK_GE(elem_types.size(), 1)
      << "Task structure type for " << task_var_name << " should have at least "
      << "one element";

  auto state_type = llvm::dyn_cast<llvm::ArrayType>(elem_types[0]);
  CHECK(state_type != nullptr)
      << "First element type of " << task_var_name << " should be an array";

  auto el_type = state_type->getArrayElementType();
  auto u8_type = llvm::Type::getInt8Ty(*context);
  CHECK_EQ(el_type, u8_type)
      << "First element type of " << task_var_name
      << " should be an array of uint8_t";

  CHECK_EQ(state_type->getArrayNumElements(), state.size())
      << "State structure data from protobuf has " << state.size()
      << "bytes, but runtime state structure needs "
      << state_type->getArrayNumElements();

  std::vector<uint8_t> bytes;
  bytes.reserve(state.size());
  for (auto c : state) {
    bytes.push_back(static_cast<uint8_t>(c));
  }

  std::vector<llvm::Constant *> initial_vals;
  initial_vals.push_back(llvm::ConstantDataArray::get(*context, bytes));

  // Fill out the rest of the task structure with zero-initialization.
  for (size_t i = 1; i < elem_types.size(); ++i) {
    initial_vals.push_back(llvm::Constant::getNullValue(elem_types[i]));
  }

  // Initialize this task with the data from the snapshot.
  task_var->setInitializer(
      llvm::ConstantStruct::get(task_struct_type, initial_vals));

  LOG(INFO)
      << "Added register state information to " << task_var_name;

  TaskContinuation cont;
  cont.continuation = lifter.GetLiftedFunction(memory.get(), pc);

  auto pc_arg = remill::NthArgument(
      cont.continuation, remill::kPCArgNum);
  auto mem_arg = remill::NthArgument(
      cont.continuation, remill::kMemoryPointerArgNum);
  auto state_arg = remill::NthArgument(
      cont.continuation, remill::kStatePointerArgNum);

  auto pc_type = llvm::dyn_cast<llvm::IntegerType>(pc_arg->getType());
  CHECK(pc_type != nullptr);

  auto mem_ptr_type = llvm::dyn_cast<llvm::PointerType>(mem_arg->getType());
  CHECK(mem_ptr_type != nullptr);

  auto state_ptr_type = llvm::dyn_cast<llvm::PointerType>(state_arg->getType());
  CHECK(state_ptr_type != nullptr);

  cont.args[remill::kPCArgNum] = llvm::ConstantInt::get(pc_type, pc);
  cont.args[remill::kMemoryPointerArgNum] = llvm::ConstantExpr::getIntToPtr(
      llvm::ConstantInt::get(pc_type, task_num), mem_ptr_type);
  cont.args[remill::kStatePointerArgNum] = llvm::ConstantExpr::getBitCast(
      task_var, state_ptr_type);

  AddTask(interpreter->ConvertContinuationToTask(cont));
}

AddressSpace *Executor::Memory(uintptr_t index) {
  return memories[index].get();
}

llvm::Function *Executor::GetLiftedFunction(
    AddressSpace *memory, uint64_t addr) {
  return lifter.GetLiftedFunction(memory, addr);
}

void *Executor::NextTask(void) {
  if (tasks.empty()) {
    return nullptr;
  } else {
    auto task = tasks.front();
    tasks.pop_front();
    return task;
  }
}

void Executor::AddTask(void *task) {
  tasks.push_back(task);
}

}  //namespace vmill
