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
#include "vmill/Executor/Interpreter.h"

#include "klee/klee.h"
#include "klee/Interpreter.h"
#include "klee/lib/Core/Executor.h"
#include "klee/Expr.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Support/Debug.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Internal/Support/FileHandling.h"
#include "klee/Internal/Support/ModuleUtil.h"
#include "klee/Internal/Support/PrintVersion.h"
#include "klee/Internal/System/Time.h"

#include "remill/BC/ABI.h"

#include <memory>
#include <cxxabi.h>

namespace llvm {
  class Module;
  class Function;
  class LLVMContext;
} // namespace llvm

namespace klee {
  class Executor;
  class Interpreter;
  class ExecutionState;
  class MemoryObject;
  class InterpreterHandler;
} // namespace klee

namespace vmill {

class ConcreteTask {
  public:
    llvm::Function * first_func;
    llvm::Constant args[remill::kNumBlockArgs];
    int argc;
    char **argv;
    char **envp;
  
  ConcreteTask(llvm::Function *func_, 
          llvm::Constant *args_[remill::kNumBlockArgs]):
      first_func(func_),
      args(args_){
          argc(2);
          argv = {"vmill filler string",}
          envp = {"vmill filler string",}
      }

};

class VmillHandler: public klee::InterpreterHandler {
    public:
      VmillHandler() ;
      ~VmillHandler() = default;
      void setInterpreter(klee::Interpreter *i) override;
      llvm::raw_ostream &getInfoStream() const override { return *info_file; };
      uint64_t getNumPathsExplored() { return paths_explored; }
      void incPathsExplored override{ paths_explored++; }
      std::string getOutputFilename(const std::string &filename) override;
      std::unique_ptr<llvm::raw_fd_ostream> openOutputFile(const std::string &filename) override;

    private:
      klee::Interpreter *interpreter;
      std::unique_ptr<llvm::raw_ostream> info_file;

      uint64_t total_tests;
      uint64_t num_generated_tests;
      uint64_t paths_explored;
};

VmillHandler::VmillHandler()//int argc, char **argv)
    : interpreter(0), total_tests(0), num_generated_tests(0),
      paths_explored(0), info_file(nullptr) {}

VmillHandler::setInterpreter(klee::Interpreter *i) override {
  interpreter = i;
}

std::string VmillHandler::getOutputFilename(const std::string &filename) {
  SmallString<128> path = "";
  sys::path::append(path,filename);
  return path.str();
}

std::unique_ptr<llvm::raw_fd_ostream> VmillHandler::openOutputFile(const std::string &filename) override{
  return nullptr;
}

#define LIBKLEE_PATH  "libklee-libc.bca"
class KleeInterpreter: public Interpreter {
  public:
    KleeInterpreter(
      llvm::LLVMContext &context,
      llvm::Module *module_):
        module(module_) {
		
		std::string LibraryDir = 
			"/home/sai/ToB/remill-build/Debug+Asserts/lib/";
        std::string EntryPoint = "__vmill_entrypoint";
        bool Optimize = true;
        bool CheckDivZero = true;
        bool CheckOvershift = true;
		std::string errorMsg = "Linking Error with Klee Libc";		

		klee::Interpreter::ModuleOptions Opts(LibraryDir.c_str(), 
											  EntryPoint,
											  Optimize,
											  CheckDivZero,
											  CheckOvershift);
		
		
	   loadedModules.emplace_back(std::unique_ptr<llvm::Module>(module));
       SmallString<128> Path(Opts.LibraryDir);
       llvm::sys::path::append(Path, "libklee-libc.bca");
       if (!klee::loadFile(Path.c_str(), mainModule->getContext(), loadedMod     ules, errorMsg)){
         klee_error("error loading klee libc '%s': %s", Path.c_str(),
                  errorMsg.c_str());
	  }

     klee::Interpreter::InterpreterOptions IOpts;
     IOpts.MakeConcreteSymbolic = false;
     VmillHandler *handler =  new VmillHandler(); //  delete later
   
     intero_impl = klee::Interpreter::create(context, IOpts, handler);
     handler->setInterpreter(interp_impl);
     module = interp_impl -> setModule(loadedModules);

}

    ~KleeInterpreter() = default;

    void Interpret(void *task_) override {
      interp_impl->setInhibitForking(true); //inhibits forking; for concrete interpretation
      auto c_task = static_cast<ConcreteTask *>(task_);
	  llvm::Function * entrypoint = module ->
								getFunction("__vmill_entrypoint");
      
	  interpreter->runFunctionAsMain( entrypoint, 
                                     c_task->argc,
                                     c_task->argv,
                                     c_task->envp );
    
    }
    
    void *ConvertContinuationToTask(const TaskContinuation &cont) override {
    auto task = new ConcreteTask(cont.continuation, cont.args);
    return task;
    }

  private:
    llvm::Module *module;
    std::unique_ptr<klee::Interpreter> interp_impl;
	std::vector<std::unique_ptr<llvm::Module>> loadedModules;
};

} // namespace vmill
