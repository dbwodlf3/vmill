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

namespace vmill {
  
bool Handler::handle(
        llvm::Instruction *instr,
        llvm::Function *func,
        std::deque<TaskContinuation> &tasks) {   
  
  auto func_name = func->getName().str();

  LOG(INFO) << "call to " << func_name;
  
  if (func_name == "__remill_write_memory_64") {

  } else if (func_name == "__remill_missing_block") {

  } else if (func_name == "__remill_jump") {
      auto pc_arg = nullptr;
      auto op1 = instr->getOperand(0);
      auto op2 = instr->getOperand(1);
      auto op3 = instr->getOperand(2);
      auto op4 = instr->getOperand(3);

      llvm::dbgs() << "***********************************\n";
      llvm::dbgs() << op1 << "  "<< op2 << "  " << op3 << " " << op4;
      llvm::dbgs() << "***********************************\n";

  } else if (func_name == "__remill_function_call") {
  
  } else if (func_name == "__remill_sync_hyper_call") {

  } else if (func_name == "__remill_async_hyper_call") {

  } else if (func_name.compare(0,1,"_") != 0) {
      return false;
  }
  //  pc, memory, and state must be extracted
  //  __remill_async_hyper_call(pc, memory, state); (perhaps called in handler)
  return true;
}

class InterpreterImpl: public llvm::VmillInterpreter, 
                       //public klee::Interpreter,
                       public Interpreter {
  public:
    explicit InterpreterImpl(llvm::Module *module_, 
            std::deque<TaskContinuation> &tasks_):
      llvm::VmillInterpreter(std::unique_ptr<llvm::Module>(module_)),
      //klee::Interpreter(klee::InterpreterOptions()),
      Interpreter(),
      handler(Handler()),
      module(module_),
      tasks(tasks_) {}

      virtual ~InterpreterImpl(void) = default;

      void symbolic_execute(llvm::Function *func, llvm::Value **args){
        //  will call to klee's interpreter once all buffers have been marked symbolic
        //  and the handler has already added to the tasks and address space
      }

      void run_and_handle(){
        while(!ECStack.empty()){
          llvm::VmillExecutionContext &SF = ECStack.back();
          llvm::Instruction &I = *SF.CurInst++;
          if (I.getOpcode() == llvm::Instruction::Call){
            auto ins = llvm::cast<llvm::CallInst>(&I);
            llvm::GenericValue src = getOperandValue(ins->getCalledValue(),SF);
            auto called_func_ptr = (llvm::Function*)llvm::GVTOP(src);
            bool is_handled = handler.handle(&I, called_func_ptr, tasks); //  can hook functions 
            if (!is_handled) {
              visit(I);
            }
          } else {
            visit(I);
          }
        }
      }

      void run_function(llvm::Function *func, llvm::ArrayRef<llvm::GenericValue> ArgValues){
        const size_t ArgCount = func->getFunctionType()->getNumParams();
        llvm::ArrayRef<llvm::GenericValue> ActualArgs =
            ArgValues.slice(0, std::min(ArgValues.size(), ArgCount));
        llvm::VmillInterpreter::callFunction(func, ActualArgs);
        run_and_handle();
      }

      void concrete_execute(llvm::Function *func, llvm::Value **args){
        std::vector<llvm::GenericValue> argv;
        const uint64_t arg_count = func->getFunctionType()->getNumParams(); //  should be 3
        LOG(INFO) << "arg count is " << arg_count << " in concrete_execute function";
        for (size_t arg_num=0; arg_num<arg_count; ++arg_num){
          argv.push_back(ConstantToGeneric(
                      llvm::dyn_cast<llvm::Constant>(args[arg_num])));
        }
        run_function(func, argv);
      }
     
  private:
    Handler handler;
    std::shared_ptr<llvm::Module> module;
    std::deque<TaskContinuation> &tasks;
};

Interpreter *Interpreter::Create(llvm::Module *module,
        std::deque<TaskContinuation> &tasks){
  return new InterpreterImpl(module, tasks);
}
}  //  namespace vmill
