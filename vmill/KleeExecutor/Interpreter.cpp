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

#include "third_party/klee/klee.h"
#include "third_party/klee/Interpreter.h"
#include "third_party/klee/lib/Core/Executor.h"

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
    llvm::Function *function;
    int argc;
    char **argv;
    char **envp;
};

class InterpreterHandler: public klee::InterpreterHandler {
  public
};


class KleeInterpreter: public Interpreter {
  public:
    KleeInterpreter(
      llvm::LLVMContext &context,
      const klee::InterpreterOptions &opts,
      klee::InterpreterHandler *ih,
      llvm::Module *module_):
        InterpreterImpl(new klee::Executor(context, opts, ih )),
        module(module_) {}

    ~KleeInterpreter() = default;

    void Interpret(void *task_) override {
      const auto task = reinterpret_cast<ConcreteTask *>(task_);
      InterpreterImpl -> runFunctionAsMain(task->function,task->argc,task->argv );
    }
    
    void *ConvertContinuationToTask(const TaskContinuation &cont) override {

      
    }

  private:
    std::unique_ptr<klee::Executor> InterpreterImpl;
    llvm::Module *module;
};

} // namespace vmill
