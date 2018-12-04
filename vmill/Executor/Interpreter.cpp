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
#include "vmill/Executor/Executor.h"
#include "vmill/Executor/Interpreter.h"

#include "vmill/Program/AddressSpace.h"

#include "third_party/klee/klee.h"
#include "third_party/klee/Interpreter.h"
#include "third_party/llvm/Interpreter.h"

#include "remill/BC/ABI.h"
#include "remill/BC/IntrinsicTable.h"
#include "remill/Arch/Name.h"

#include "remill/Arch/Instruction.h"

#include "remill/BC/Util.h"
#include "remill/BC/Optimizer.h"
#include "remill/OS/OS.h"


namespace vmill {

class ConcreteTask {
 public:
  std::vector<llvm::VmillExecutionContext> stack;
};

class ConcreteInterpreter : public llvm::VmillInterpreter,
                            public Interpreter {
 public:
  explicit ConcreteInterpreter(llvm::Module *module_,
                               Executor *exe_)
      : llvm::VmillInterpreter(std::unique_ptr<llvm::Module>(module_)),
        Interpreter(),
        intrinsics(module_),
        exe(exe_) {}

  virtual ~ConcreteInterpreter(void) = default;

  bool HandleFunctionCall(llvm::CallInst *call) {
    llvm::VmillExecutionContext &frame = ECStack->back();
    llvm::GenericValue called_func = getOperandValue(call->getCalledValue(),
                                                     frame);
    llvm::Function * const called_func_ptr = llvm::dyn_cast<llvm::Function>(
        reinterpret_cast<llvm::Value *>(llvm::GVTOP(called_func)));

    const auto ret_type = called_func_ptr->getReturnType();

    //CHECK_EQ( &mod ,(called_func_ptr->getParent()));

    if (!called_func_ptr) {
      return false;
    }
    
    if (intrinsics.read_memory_8 == called_func_ptr) {

#define DO_READ(val_type, num_bytes, const_type) \
    const auto mem_ptr_val = call->getArgOperand(0); \
    const auto addr_val = call->getArgOperand(1); \
    const auto mem_ptr = getOperandValue(mem_ptr_val, frame); \
    const auto addr = getOperandValue(addr_val, frame); \
    const auto addr_uint = addr.IntVal.getZExtValue(); \
    auto memory = exe->Memory(reinterpret_cast<uint64_t>(mem_ptr.PointerVal)); \
    val_type val = 0; \
    if (memory->TryRead(addr_uint, &val)) { \
      frame.Values[call] = ConstantToGeneric( \
          llvm::const_type::get(ret_type, val)); \
    } else { \
      LOG(FATAL) \
          << "Invalid read of " << num_bytes << " bytes at address " \
          << std::hex << addr_uint << std::dec; \
      frame.Values[call] = ConstantToGeneric(llvm::UndefValue::get(ret_type)); \
    }

      DO_READ(uint8_t, 1, ConstantInt)

    } else if (intrinsics.read_memory_16 == called_func_ptr) {
      DO_READ(uint16_t, 2, ConstantInt)

    } else if (intrinsics.read_memory_32 == called_func_ptr) {
      DO_READ(uint32_t, 4, ConstantInt)

    } else if (intrinsics.read_memory_64 == called_func_ptr) {
      DO_READ(uint64_t, 8, ConstantInt)

    } else if (intrinsics.read_memory_f32 == called_func_ptr) {
      DO_READ(float, 4, ConstantFP)

    } else if (intrinsics.read_memory_f64 == called_func_ptr) {
      DO_READ(double, 8, ConstantFP)

#undef DO_READ

    } else if (intrinsics.read_memory_f80 == called_func_ptr) {
      // TODO

    } else if (intrinsics.write_memory_8 == called_func_ptr) {

#define DO_WRITE(val_type, val_accessor, val_size) \
    const auto mem_ptr_val = call->getArgOperand(0); \
    const auto addr_val = call->getArgOperand(1); \
    const auto val_val = call->getArgOperand(3); \
    const auto mem_ptr = getOperandValue(mem_ptr_val, frame); \
    const auto addr = getOperandValue(addr_val, frame); \
    const auto val = getOperandValue(val_val, frame); \
    const auto addr_uint = addr.IntVal.getZExtValue(); \
    const auto val_uint = static_cast<val_type>(val.val_accessor); \
    auto memory = exe->Memory(reinterpret_cast<uint64_t>(mem_ptr.PointerVal)); \
    if (!memory->TryWrite(addr_uint, val_uint)) { \
      LOG(FATAL) \
          << "Invalid write of " << val_size << " bytes to address " \
          << std::hex << addr_uint << std::dec; \
    } \
    frame.Values[call] = mem_ptr;

      DO_WRITE(uint8_t, IntVal.getZExtValue(), 1)

    } else if (intrinsics.write_memory_16 == called_func_ptr) {
      DO_WRITE(uint16_t, IntVal.getZExtValue(), 2)

    } else if (intrinsics.write_memory_32 == called_func_ptr) {
      DO_WRITE(uint32_t, IntVal.getZExtValue(), 4)

    } else if (intrinsics.write_memory_64 == called_func_ptr) {
      DO_WRITE(uint64_t, IntVal.getZExtValue(), 8)

    } else if (intrinsics.write_memory_f32 == called_func_ptr) {
      DO_WRITE(float, FloatVal, 4)

    } else if (intrinsics.write_memory_f64 == called_func_ptr) {
      DO_WRITE(double, DoubleVal, 8)

#undef DO_WRITE

    } else if (intrinsics.write_memory_f80 == called_func_ptr) {

      // TODO

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

      auto memory = exe->Memory(reinterpret_cast<uint64_t>(mem_ptr.PointerVal));
      auto target = exe->GetLiftedFunction(memory, pc_uint);

      // TODO(sae): Emulate call of `target` with `mem_ptr`, `state`, and
      //            `pc`, just as you did with converting a continuation to
      //            a `ConcreteTask`.
      LOG(FATAL)
          << "Trying to execute code at " << std::hex << pc_uint << std::dec;

    } else if (intrinsics.error == called_func_ptr) {
      auto pc_val = call->getArgOperand(remill::kPCArgNum);
      auto pc = getOperandValue(pc_val, frame);
      LOG(ERROR)
          << "Execution errored out at "
          << std::hex << pc.IntVal.getZExtValue() << std::dec;

      // TODO(sai): "exit" the interpreter.

    } else if (intrinsics.async_hyper_call == called_func_ptr) {
      LOG(FATAL)
          << "Should be implemented in runtime.";


    // Returns (by setting the value associated with the `CallInst`) the
    // passed in memory pointer.
    //
    // NOTE(sae): We don't do anything special for memory barriers.
    } else if (intrinsics.barrier_load_load == called_func_ptr ||
               intrinsics.barrier_load_store == called_func_ptr ||
               intrinsics.barrier_store_load == called_func_ptr ||
               intrinsics.barrier_store_store == called_func_ptr ||
               intrinsics.atomic_begin == called_func_ptr ||
               intrinsics.atomic_end == called_func_ptr) {

      auto mem_ptr_val = call->getArgOperand(0);
      auto mem_ptr = getOperandValue(mem_ptr_val, frame);
      frame.Values[call] = mem_ptr;

    // Returns (by setting the value associated with the `CallInst`) an
    // undefined value.
    } else if (intrinsics.undefined_8 == called_func_ptr ||
               intrinsics.undefined_16 == called_func_ptr ||
               intrinsics.undefined_32 == called_func_ptr ||
               intrinsics.undefined_64 == called_func_ptr ||
               intrinsics.undefined_f32 == called_func_ptr ||
               intrinsics.undefined_f64 == called_func_ptr) {

      auto mem_ptr_val = call->getArgOperand(0);
      auto mem_ptr = getOperandValue(mem_ptr_val, frame);
      frame.Values[call] = ConstantToGeneric(llvm::UndefValue::get(ret_type));

    } else {
      return false;
    }

    return true;
  }

  void RunInstructions(void) {
    while (!ECStack->empty()) {
      llvm::VmillExecutionContext &SF = ECStack->back();

      for (auto &val_genval : SF.Values) {
        std::stringstream ss;
        if (val_genval.first->getType()->isIntegerTy()) {
          ss << std::hex << val_genval.second.IntVal.getZExtValue();

        } else if (val_genval.first->getType()->isFloatTy()) {
          ss <<  val_genval.second.FloatVal;

        } else if (val_genval.first->getType()->isDoubleTy()) {
          ss <<  val_genval.second.DoubleVal;
        }

        if (val_genval.first->hasName()) {
          llvm::dbgs() << val_genval.first->getName() << " = " << ss.str() << '\n';
        } else {
          llvm::dbgs() << *(val_genval.first) << " = " << ss.str() << '\n';
        }
      }

      llvm::Instruction &I = *SF.CurInst++;
      llvm::dbgs() << "EXECUTING: " << I << '\n';
      if (auto call = llvm::dyn_cast<llvm::CallInst>(&I)) {
        if (!HandleFunctionCall(call)) {
          LOG(ERROR) << remill::LLVMThingToString(call);
          visit(I);
        }
      } else {
        visit(I);
      }
    }
  }

  void Interpret(void *task_) override {
    const auto task = reinterpret_cast<ConcreteTask *>(task_);

    // Swap onto the task stack.
    const auto prev_stack = ECStack;
    ECStack = &(task->stack);

    RunInstructions();

    // Swap back to the previous stack (likely `MainStack`).
    ECStack = prev_stack;
  }

  void *ConvertContinuationToTask(const TaskContinuation &cont) override {
    auto task = new ConcreteTask;
    task->stack.reserve(32);
    task->stack.resize(1);

    auto &frame = task->stack[0];
    frame.CurFunction = cont.continuation;
    frame.CurBB = &*(frame.CurFunction->begin());
    frame.CurInst = frame.CurBB->begin();

    // Pass in the arguments.
    for (size_t i = 0; i < remill::kNumBlockArgs; ++i) {
      auto arg_val = remill::NthArgument(frame.CurFunction, i);
      frame.Values[arg_val] = ConstantToGeneric(cont.args[i]);
      frame.arg_consts[i] = cont.args[i];
    }

    return task;
  }

 private:
  const remill::IntrinsicTable intrinsics;
  Executor *exe;
};

Interpreter::~Interpreter(void) {}

Interpreter *Interpreter::CreateConcrete(
    llvm::Module *module, Executor *exe) {
  return new ConcreteInterpreter(module, exe);
}

}  //  namespace vmill
