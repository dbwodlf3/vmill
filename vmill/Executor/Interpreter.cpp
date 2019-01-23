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
#include "klee/Expr.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Support/Debug.h"

#include "klee/Internal/Support/ErrorHandling.h"

#include "klee/Internal/Support/FileHandling.h"
#include "klee/Internal/Support/ModuleUtil.h"
#include "klee/Internal/Support/PrintVersion.h"
#include "klee/Internal/System/Time.h"

#include "klee/Config/Version.h"
#include "klee/Internal/ADT/KTest.h"
#include "klee/Internal/ADT/TreeStream.h"                                                                        
#include "klee/Statistics.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Errno.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Signals.h"
#include <llvm/Bitcode/BitcodeReader.h>

#include "remill/BC/ABI.h"
#include "remill/BC/Util.h"

#include <memory>
#include <cxxabi.h>
#include <sstream>

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
    llvm::Function *first_func;
    int argc;
    char *argv[3];
    char **envp;
};

class VmillHandler: public klee::InterpreterHandler {
    public:
      VmillHandler();
      ~VmillHandler() = default;
      void setInterpreter(klee::Interpreter *i);
      llvm::raw_ostream &getInfoStream() const override { return *info_file; }
      uint64_t getNumPathsExplored() { return paths_explored; }
      void incPathsExplored() override { paths_explored++; }
      std::string getOutputFilename(const std::string &filename) override;
      std::unique_ptr<llvm::raw_fd_ostream> openOutputFile(const std::string &filename) override;
      void processTestCase(const klee::ExecutionState &state,
                                const char *err,
                                const char *suffix) override;

    private:
      klee::Interpreter *interpreter;

      uint64_t total_tests;
      uint64_t num_generated_tests;
      uint64_t paths_explored;

      std::unique_ptr<llvm::raw_ostream> info_file;
};

VmillHandler::VmillHandler()//int argc, char **argv)
    : klee::InterpreterHandler(), 
      interpreter(0), total_tests(0), num_generated_tests(0),
      paths_explored(0) {
	
	info_file = openOutputFile("info");
}

void VmillHandler::setInterpreter(klee::Interpreter *i){
  interpreter = i;
}

std::string VmillHandler::getOutputFilename(const std::string &filename) {
  llvm::SmallString<128> path("./");
  llvm::sys::path::append(path,filename);
  return path.str();
}

std::unique_ptr<llvm::raw_fd_ostream> 
VmillHandler::openOutputFile(const std::string &filename) {
   std::string Error;
   std::string path = getOutputFilename(filename);
   auto f = klee::klee_open_output_file(path, Error);
   if (!f) {
     LOG(FATAL) << "error opening file \"%s\".  KLEE may have run out of file "
                 << "descriptors: try to increase the maximum number of open file "
                  << "descriptors by using ulimit (%s).";
     return nullptr;
   }
   return f;

}


void VmillHandler::processTestCase(const klee::ExecutionState &state,
						const char *err,
						const char *suffix){ return;}


#define LIBKLEE_PATH  "libklee-libc.bca"
class KleeInterpreter : public Interpreter {
  public:
    KleeInterpreter(
      llvm::LLVMContext &context,
      llvm::Module *module_, 
      Executor *exe_):
		Interpreter(),
        module(module_),
        exe(exe_) {
		
		llvm::InitializeNativeTarget();
		
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
	
	   #define target_file	"/home/sai/ToB/remill-build/bin/target.bc"

       std::vector<llvm::Function *> lifted_funcs;
       for (auto &g: module->functions()){
           LOG(INFO) << g.getName().data();
           if(std::string(g.getName().data()).compare(0,4,"sub_") == 0){
               lifted_funcs.push_back(&g);
           }
       }

       LOG(INFO) << "There are " << lifted_funcs.size() << " lifted functions";

  	   remill::StoreModuleToFile(module, target_file);

       if (!klee::loadFile(target_file, context, loadedModules, errorMsg)) {
	     LOG(FATAL) << "error loading program " << target_file;   
       } 
	   

  	   LOG(INFO) << "Extracted target file";
	    
	   std::unique_ptr<llvm::Module> M(klee::linkModules( 
         loadedModules, "" /* link all modules together */, errorMsg));
       if (!M) {
         LOG(FATAL) << "error loading program";
       }
      	
	   llvm::Module * mainModule = M.get();
	   LOG(INFO) << "main module passed";	
       loadedModules.emplace_back(std::move(M));
/*		
       llvm::SmallString<128> Path(Opts.LibraryDir);
       llvm::sys::path::append(Path, LIBKLEE_PATH );
       LOG(INFO) << "KLEE LIBC LINKAGE IS OCCURING";
       if (!klee::loadFile(Path.c_str(), mainModule->getContext(), 
	   		loadedModules, errorMsg )){
            LOG(FATAL) << "error loading klee libc " << Path.c_str(),  errorMsg.c_str();
	  }
*/
     llvm::SmallString<128> Path(Opts.LibraryDir);
     llvm::sys::path::append(Path, "libkleeRuntimeFreeStanding.bca");
     if (!klee::loadFile(Path.c_str(), mainModule->getContext(), loadedModules,
                         errorMsg)){
       LOG(FATAL) << "BAD LINK"; 
	 }
     
     LOG(INFO) << "PASSED LINK !";
     klee::Interpreter::InterpreterOptions IOpts;
     IOpts.MakeConcreteSymbolic = false;
     VmillHandler *handler =  new VmillHandler(); //  delete later
	 LOG(INFO) << "Handler has been created";
   
     interp_impl = std::unique_ptr<klee::Interpreter>(klee::Interpreter::create(context, IOpts, handler));
     handler->setInterpreter(interp_impl.get());
     
     interp_impl->setLiftedFunctions(lifted_funcs);
     interp_impl->setVmillExecutor(exe);
     module = interp_impl -> setModule(loadedModules, Opts);
  //  module->dump(); debugging utility
    }

    ~KleeInterpreter() = default;

    void Interpret(void *task_) override {
      interp_impl->setInhibitForking(true); //  inhibits forking; for concrete interpretation
      auto c_task = static_cast<ConcreteTask *>(task_);
	  llvm::Function * entrypoint = module ->
								getFunction("__vmill_entrypoint");

      llvm::dbgs() << '\n';

      LOG(INFO) << c_task->argc;
      LOG(INFO) << c_task->argv[0];
      LOG(INFO) << *(c_task->argv[0]);

      interp_impl->runFunctionAsMain( entrypoint, 
                                     c_task->argc,
                                     c_task->argv,
                                     c_task->envp );
    
    }
    
    void *ConvertContinuationToTask(const TaskContinuation &cont) override {
      auto task = new ConcreteTask; //(cont.continuation, cont.args); // d later
      
      task->argc = 1;
      char * str = new char[cont.state.size()];
      for(int i=0; i< cont.state.size();++i) {
        str[i] = cont.state.c_str()[i]; //{"vmill",}
      }

      task->argv[0] = str;
      task->envp = {}; //{"vmill",}

      return task;
    }

  private:
    llvm::Module *module;
    Executor *exe;
    std::unique_ptr<klee::Interpreter> interp_impl;
	std::vector<std::unique_ptr<llvm::Module>> loadedModules;
};

Interpreter::~Interpreter(void) {}

Interpreter *Interpreter::CreateConcrete(
	llvm::Module *module, Executor *exe) {
    return new KleeInterpreter(module -> getContext(), module, exe);
}

} // namespace vmill
