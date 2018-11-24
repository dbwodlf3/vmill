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

//perform concrete execution
//mark variables as symbolic in emulator and address space
//when encountered throw function into symbolic klee interpreter

namespace vmill {

//utility class that will handle calls to the vmill runtime
void Handler::handle(llvm::Instruction &instr,
              std::deque<TaskContinuation> &tasks) {
  //basically a huge switch case that performs actions based off of the instruction
  //and calls into the runtime an example would be something like
  //marking a buffer as symbolic in the read syscall runtime implementation
}

//*** FOCUS ON CONCRETE EXECUTION AND HOOKING FOR NOW IN THE INTERPRETER

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
          tasks(tasks_){}

      //will call to klee's interpreter once all buffers have been marked symbolic
      //and the handler has already added to the tasks and address space
      void symbolic_execute(llvm::Function *func, llvm::Value *args){
      
      }

      void run_and_handle(){
        while(!ECStack.empty()){
          llvm::VmillExecutionContext &SF = ECStack.back();
          llvm::Instruction &I = *SF.CurInst++;
		  if (I.getOpcode() == llvm::Instruction::Call){
          	handler.handle(I, tasks); //can appropriately hook functions 
		  } else {
            visit(I);
          }
        }
      }

      //handles emulator runtime with handler and executes function concretely
      void run_function(llvm::Function *func, llvm::ArrayRef<llvm::GenericValue> ArgValues){
		const size_t ArgCount = func->getFunctionType()->getNumParams();
        llvm::ArrayRef<llvm::GenericValue> ActualArgs =
			ArgValues.slice(0, std::min(ArgValues.size(), ArgCount));
		llvm::VmillInterpreter::callFunction(func, ActualArgs);
		run_and_handle();
	  }
    
      void concrete_execute(llvm::Function *func, llvm::Value *args){
        std::vector<llvm::GenericValue> argv;
        const uint64_t arg_count = func->getFunctionType()->getNumParams(); //should be 3
        LOG(INFO) << "arg count is " << arg_count;
        for (size_t arg_num=0; arg_num<arg_count; ++arg_num){
          argv.push_back(
                  ConstantToGeneric(llvm::dyn_cast<llvm::Constant>(args+arg_num)));
        }
        run_function(func, argv);
      }
     
    private:
      Handler handler;
      std::shared_ptr<llvm::Module> module;
      std::deque<TaskContinuation> &tasks;
};

std::unique_ptr<Interpreter> Interpreter::Create(llvm::Module *module,
        std::deque<TaskContinuation> &tasks){
  return std::unique_ptr<Interpreter>(
          new InterpreterImpl(module, tasks));
}

}  // namespace vmill
