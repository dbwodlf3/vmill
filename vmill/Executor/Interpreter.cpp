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
                               std::deque<void *> &tasks_,
                               const remill::IntrinsicTable &intrinsic_table)
      : llvm::VmillInterpreter(std::unique_ptr<llvm::Module>(module_)),
        Interpreter(),
        intrinsics(intrinsic_table),
        tasks(reinterpret_cast<std::deque<ConcreteTask *> &>(tasks_)) {}

  virtual ~ConcreteInterpreter(void) = default;

  bool HandleFunctionCall(llvm::CallInst *call) {
    llvm::VmillExecutionContext &frame = ECStack.back();
    llvm::GenericValue called_func = getOperandValue(call->getCalledValue(), frame);
    llvm::Function * const called_func_ptr = 
        llvm::dyn_cast<llvm::Function>(reinterpret_cast<llvm::Value *>(llvm::GVTOP(called_func)));

	//CHECK_EQ( &mod ,(called_func_ptr->getParent()));

    if (!called_func_ptr) {
      return false;
    }
    
    llvm::dbgs() << "func name is" << called_func_ptr -> getName() <<'\n';

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

      // TODO(sai): "exit" the interpreter.

    } else if (intrinsics.async_hyper_call == called_func_ptr) {
      LOG(FATAL)
          << "Should be implemented in runtime.";

    } else if (intrinsics.barrier_load_load == called_func_ptr ||
               intrinsics.barrier_load_store == called_func_ptr ||
               intrinsics.barrier_store_load == called_func_ptr ||
               intrinsics.barrier_store_store == called_func_ptr ||
               intrinsics.atomic_begin == called_func_ptr ||
               intrinsics.atomic_end == called_func_ptr) {

      // TODO(sai): Return the memory pointer (function argument 1).

    } else if (intrinsics.undefined_8 == called_func_ptr ||
               intrinsics.undefined_16 == called_func_ptr ||
               intrinsics.undefined_32 == called_func_ptr ||
               intrinsics.undefined_64 == called_func_ptr ||
               intrinsics.undefined_f32 == called_func_ptr ||
               intrinsics.undefined_f64 == called_func_ptr) {

      // TODO(sai): Return this...
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
          llvm::dbgs() << "Fuction Handler" << '\n';
          visit(I);
        }
      } else {
        llvm::dbgs() << I << '\n';      
        visit(I);
      }
    }
  }

  void Interpret(llvm::Function *func, llvm::Constant **args) override {
	std::vector<llvm::GenericValue> argv;
	const uint64_t arg_count = func->getFunctionType()->getNumParams();
	for (size_t arg_num=0; arg_num<arg_count; ++arg_num){
	  argv.push_back(ConstantToGeneric(args[arg_num]));
	}
	llvm::VmillInterpreter::callFunction(func, argv);
	RunInstructions();
  }

  void *ConvertContinuationToTask(const TaskContinuation &cont) override {
    auto frame = new llvm::VmillExecutionContext();
    frame->CurFunction = cont.continuation;
    frame->CurBB = &*(frame->CurFunction -> begin());
    frame->CurInst = frame->CurBB -> begin();

    for(size_t i=0; i<3; ++i){
     frame->Values[cont.args[i]] = ConstantToGeneric(cont.args[i]); 
     frame->arg_consts[i] = cont.args[i];
    }
    return reinterpret_cast<void *>(frame);
  }

  std::pair<llvm::Function*, llvm::Constant **>
	GetFuncFromTaskContinuation(void *cont) override {
    auto frame = reinterpret_cast<llvm::VmillExecutionContext *>(cont);
    return std::pair<llvm::Function *,llvm::Constant **>
			(frame->CurFunction,frame->arg_consts);
  }

 private:
  const remill::IntrinsicTable &intrinsics;
  std::deque<ConcreteTask *> &tasks;
};

Interpreter::~Interpreter(void) {}

Interpreter *Interpreter::CreateConcrete(
    llvm::Module *module, std::deque<void *> &tasks, 
    const remill::IntrinsicTable& intrinsic) {
  return new ConcreteInterpreter(module, tasks, intrinsic);
}

}  //  namespace vmill
