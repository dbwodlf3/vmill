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

#pragma once

#include <memory>
#include <tuple>

#include "vmill/Executor/TraceManager.h"
#include "vmill/Executor/Runtime.h"
#include "vmill/Executor/Executor.h"

#include "vmill/Program/AddressSpace.h"

#include "third_party/klee/klee.h"
#include "third_party/klee/Interpreter.h"
#include "third_party/llvm/Interpreter.h"

namespace remill {
class IntrinsicTable;
}

namespace llvm {
class ExecutionEngine;
class VmillInterpreter;
class CallInst;
class Function;
class Module;
class Value;
class VmillExecutionContext;
}  //  namespace llvm

namespace klee {
class Interpreter;
class InterpreterOptions;
class InterpreterHandler;
class ExecutionState;
}  //  namespace klee

namespace vmill {

class Executor;

class Interpreter {
 public:
  virtual ~Interpreter(void);

  static Interpreter *CreateConcrete(llvm::Module *module, Executor *exe);

  virtual void Interpret(void *) = 0;

  virtual void *ConvertContinuationToTask(const TaskContinuation &cont) = 0;

 protected:
  Interpreter(void) = default;
};

}  //  namespace vmill
