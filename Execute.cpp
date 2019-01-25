/*
 * Copyright (c) 2019 Trail of Bits, Inc.
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

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "remill/Arch/Arch.h"

#include "vmill/Workspace/Workspace.h"
#include "vmill/Program/Snapshot.h"
#include "vmill/Program/AddressSpace.h"

#include "klee/klee.h"
#include "klee/Interpreter.h"
#include "klee/Expr.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Support/Debug.h"

#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Internal/Support/FileHandling.h"
#include "klee/Internal/Support/ModuleUtil.h"

#include "klee/Config/Version.h"
#include "klee/Internal/ADT/KTest.h"
#include "klee/Internal/ADT/TreeStream.h"                                                                        
#include "klee/Statistics.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Errno.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Signals.h"

#include "remill/BC/ABI.h"
#include "remill/BC/Util.h"

#include <memory>
#include <cxxabi.h>
#include <sstream>

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


DECLARE_string(os);
DECLARE_string(arch);
#define LIBKLEE_PATH  "libklee-libc.bca"

llvm::Module *LoadRuntimeBitcode(llvm::LLVMContext *context) {
  auto &runtime_bitcode_path = vmill::Workspace::RuntimeBitcodePath();
  LOG(INFO)
      << "Loading runtime bitcode file from " << runtime_bitcode_path;
  return remill::LoadModuleFromFile(context, runtime_bitcode_path,
                                    false  /* allow_failure */);
}

int main(int argc, char **argv) {
  std::stringstream ss;
  ss << std::endl << std::endl
     << "  " << argv[0] << " \\" << std::endl
     << "    --workspace WORKSPACE_DIR \\" << std::endl
     << "    ..." << std::endl;

  google::InitGoogleLogging(argv[0]);
  google::SetUsageMessage(ss.str());
  google::ParseCommandLineFlags(&argc, &argv, true);

  FLAGS_logtostderr = true;

  auto snapshot = vmill::LoadSnapshotFromFile(vmill::Workspace::SnapshotPath());

  //Take in the OS and arch names from the snapshot.
  FLAGS_os = snapshot->os();
  FLAGS_arch = snapshot->arch();

  //Make sure that we support the snapshotted arch/os combination.
  CHECK(remill::GetTargetArch() != nullptr)
      << "Can't find architecture for " << FLAGS_os << " and " << FLAGS_arch;


  auto module = LoadRuntimeBitcode(new llvm::LLVMContext);

  std::vector<std::unique_ptr<llvm::Module>> loadedModules;
  llvm::LLVMContext &context = module->getContext();
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
 
  klee::Interpreter::InterpreterOptions IOpts;
  IOpts.MakeConcreteSymbolic = false;
  VmillHandler *handler =  new VmillHandler(); //  delete later
  LOG(INFO) << "Handler has been created";

  auto executor = std::unique_ptr<klee::Interpreter>(klee::Interpreter::create(context, IOpts, handler));
  handler->setInterpreter(executor.get());
  module = executor->setModule(loadedModules, Opts);
  vmill::Workspace::LoadSnapshotIntoExecutor(snapshot, executor.get());

  executor->Run();
  
  llvm::llvm_shutdown();
  return EXIT_SUCCESS;
}
