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

#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Constant.h"

#include "vmill/Executor/TraceManager.h"
#include "vmill/Executor/Runtime.h"
#include "vmill/Executor/Interpreter.h"

#include "vmill/Program/AddressSpace.h"

#include "third_party/klee/klee.h"
#include "third_party/klee/Interpreter.h"
#include "third_party/llvm/Interpreter.h"

#include "remill/BC/ABI.h"
#include "remill/BC/IntrinsicTable.h"

namespace vmill {

class ConcreteTask {
 public:
  std::vector<llvm::VmillExecutionContext> stack;
};

class ConcreteInterpreter : public llvm::VmillInterpreter,
                            public Interpreter {
 public:
  explicit ConcreteInterpreter(llvm::Module *module_,
                               std::deque<void *> &tasks_)
      : llvm::VmillInterpreter(std::unique_ptr<llvm::Module>(module_)),
        Interpreter(),
        intrinsics(module_),
        module(module_),
        tasks(reinterpret_cast<std::deque<ConcreteTask *> &>(tasks_)) {}

  virtual ~ConcreteInterpreter(void) = default;

  bool HandleFunctionCall(llvm::CallInst *call) {
    llvm::VmillExecutionContext &frame = ECStack.back();
    llvm::GenericValue called_func = getOperandValue(call->getCalledValue(), frame);
    auto called_func_ptr = llvm::dyn_cast<llvm::Function>(
        reinterpret_cast<llvm::Value *>(llvm::GVTOP(called_func)));

    if (!called_func_ptr) {
      return false;
    }

    if (intrinsics.read_memory_8 == called_func_ptr) {

    } else if (intrinsics.read_memory_16 == called_func_ptr) {

    } else if (intrinsics.read_memory_32 == called_func_ptr) {

    } else if (intrinsics.read_memory_64 == called_func_ptr) {

    } else if (intrinsics.read_memory_f32 == called_func_ptr) {

    } else if (intrinsics.read_memory_f64 == called_func_ptr) {

    } else if (intrinsics.read_memory_f80 == called_func_ptr) {

    } else if (intrinsics.write_memory_8 == called_func_ptr) {

    } else if (intrinsics.write_memory_16 == called_func_ptr) {

    } else if (intrinsics.write_memory_32 == called_func_ptr) {

    } else if (intrinsics.write_memory_64 == called_func_ptr) {

    } else if (intrinsics.write_memory_f32 == called_func_ptr) {

    } else if (intrinsics.write_memory_f64 == called_func_ptr) {

    } else if (intrinsics.write_memory_f80 == called_func_ptr) {

    } else if (intrinsics.jump == called_func_ptr ||
               intrinsics.function_call == called_func_ptr ||
               intrinsics.function_return == called_func_ptr ||
               intrinsics.missing_block == called_func_ptr) {

      auto state_ptr_val = call->getArgOperand(remill::kStatePointerArgNum);
      auto pc_val = call->getArgOperand(remill::kPCArgNum);
      auto mem_ptr_val = call->getArgOperand(remill::kMemoryPointerArgNum);

      auto state = getOperandValue(state_ptr_val, frame);
      auto mem_ptr = getOperandValue(mem_ptr_val, frame);

      auto pc = getOperandValue(pc_val, frame);
      auto pc_uint = pc.IntVal.getZExtValue();

    } else if (intrinsics.error == called_func_ptr) {
      auto pc_val = call->getArgOperand(remill::kPCArgNum);
      auto pc = getOperandValue(pc_val, frame);
      LOG(ERROR)
          << "Execution errored out at "
          << std::hex << pc.IntVal.getZExtValue() << std::dec;

      // TODO(sae): "exit" the interpreter.

    } else if (intrinsics.async_hyper_call == called_func_ptr) {
      LOG(FATAL)
          << "Should be implemented in runtime.";

    } else if (intrinsics.barrier_load_load == called_func_ptr ||
               intrinsics.barrier_load_store == called_func_ptr ||
               intrinsics.barrier_store_load == called_func_ptr ||
               intrinsics.barrier_store_store == called_func_ptr ||
               intrinsics.atomic_begin == called_func_ptr ||
               intrinsics.atomic_end == called_func_ptr) {

      // TODO(sae): Return the memory pointer (function argument 1).

    } else if (intrinsics.undefined_8 == called_func_ptr ||
               intrinsics.undefined_16 == called_func_ptr ||
               intrinsics.undefined_32 == called_func_ptr ||
               intrinsics.undefined_64 == called_func_ptr ||
               intrinsics.undefined_f32 == called_func_ptr ||
               intrinsics.undefined_f64 == called_func_ptr) {

      // TODO(sae): Return this...
      (void) llvm::UndefValue::get(called_func_ptr->getReturnType());

    // Not handled by us.
    } else {
      return false;
    }

    return true;
  }

  void RunInstructions(void) {
    while (!ECStack.empty()) {
      llvm::VmillExecutionContext &SF = ECStack.back();
      llvm::Instruction &I = *SF.CurInst++;
      if (auto call = llvm::dyn_cast<llvm::CallInst>(&I)) {
        if (!HandleFunctionCall(call)) {
          visit(I);
        }
      } else {
        visit(I);
      }
    }
  }

  void Interpret(llvm::Function *func, llvm::Constant **args) override {
    const size_t num_args = func->getFunctionType()->getNumParams();
    llvm::SmallVector<llvm::GenericValue, 3> argv(num_args);
    for (size_t i = 0; i < num_args; ++i) {
      argv[i] = ConstantToGeneric(args[i]);
    }
    llvm::VmillInterpreter::callFunction(func, argv);
    RunInstructions();
  }

  void *ConvertContinuationToTask(const TaskContinuation &cont) override {

  }

 private:
  const remill::IntrinsicTable intrinsics;
  llvm::Module * const module;
  std::deque<ConcreteTask *> &tasks;
};

Interpreter::~Interpreter(void) {}

Interpreter *Interpreter::CreateConcrete(
    llvm::Module *module, std::deque<TaskContinuation> &tasks) {
  return new ConcreteInterpreter(module, tasks);
}
}  //  namespace vmill
