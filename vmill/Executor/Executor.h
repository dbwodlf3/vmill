/*
 * Copyright (c) 2017 Trail of Bits, Inc.
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

#include <deque>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "remill/BC/ABI.h"

#include "vmill/BC/TraceLifter.h"
#include "vmill/Executor/TraceManager.h"

namespace llvm {
class Constant;
class Function;
class LLVMContext;
}  // namespace llvm

namespace vmill {

class AddressSpace;
class Lifter;
class Interpreter;

class TaskContinuation {
 public:
  llvm::Constant *args[remill::kNumBlockArgs];
  llvm::Function *continuation;
};

class Executor {
 public:
  Executor(void);
  ~Executor(void);
  void SetUp(void);
  void TearDown(void);
  void Run(void);
  void AddInitialTask(const std::string &state, const uint64_t pc,
                      std::shared_ptr<AddressSpace> memory);

 private:
  std::shared_ptr<llvm::LLVMContext> context;
  llvm::Module *lifted_code;
  TraceManager trace_manager;
  TraceLifter lifter;
  std::unique_ptr<Interpreter> interpreter;
  std::vector<std::shared_ptr<AddressSpace>> memories;
  std::deque<void *> tasks;
};

}  // namespace vmill
