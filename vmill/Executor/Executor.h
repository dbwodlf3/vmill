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

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "vmill/BC/TraceLifter.h"
#include "vmill/Executor/TraceManager.h"
#include "vmill/Runtime/Task.h"

namespace llvm {
class LLVMContext;
}

namespace vmill {

class AddressSpace;
class Lifter;

struct LiftedBitcodeInfo {
  inline LiftedBitcodeInfo(void)
      : pc(static_cast<PC>(0)),
        lifted_func(nullptr),
        version(0) {}

  PC pc;
  llvm::Function *lifted_func;
  uint64_t version;
};

class Executor {
 public:
  Executor(void);
  ~Executor(void);
  void SetUp(void);
  void TearDown(void);
  void Run(void);
  LiftedBitcodeInfo GetLiftedFunction(Task *task);

  void AddInitialTask(const std::string &state, PC pc,
                      std::shared_ptr<AddressSpace> memory);

 private:
  std::shared_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::Module> lifted_code;
  TraceManager trace_manager;
  TraceLifter lifter;
  std::vector<std::shared_ptr<AddressSpace>> memories;
};

}  // namespace vmill
