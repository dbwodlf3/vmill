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
#include <gflags/gflags.h>
#include <glog/logging.h>

#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <utility>

#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include "remill/Arch/Arch.h"
#include "remill/Arch/Instruction.h"
#include "remill/Arch/Name.h"

#include "remill/BC/ABI.h"
#include "remill/BC/IntrinsicTable.h"
#include "remill/BC/Lifter.h"
#include "remill/BC/Util.h"
#include "remill/BC/Optimizer.h"

#include "remill/OS/OS.h"

#include "vmill/Program/AddressSpace.h"

#pragma once

namespace vmill {
namespace {

 class Lifter {
 public:
  virtual ~Lifter(void);

  static std::unique_ptr<Lifter> Create(
      const std::shared_ptr<llvm::LLVMContext> &context);

  // Lift a list of decoded traces into a new LLVM bitcode module, and
  // return the resulting module.
  virtual  std::unique_ptr<llvm::Module> Lift(
      uint64_t addr_) = 0;

 protected:
  Lifter(void);
};

class VmillTraceManager: public remill::TraceManager{
  public:
    explicit VmillTraceManager(AddressSpace &addr_space);
    virtual void ForEachDevirtualizedTarget( 
            const remill::Instruction &inst, 
        std::function<void(uint64_t,
            remill::DevirtualizedTargetKind)> func) override;

    virtual bool TryReadExecutableByte(uint64_t addr, uint8_t *byte) override;

  protected:
    virtual void SetLiftedTraceDefinition(
        uint64_t addr, llvm::Function *lifted_func) override;

    llvm::Function *GetLiftedTraceDeclaration(uint64_t addr) override;

    llvm::Function *GetLiftedTraceDefinition(uint64_t addr) override;
    
  public:
    AddressSpace &memory;
    std::unordered_map<uint64_t, llvm::Function *> traces;
};

class VmillTraceLifter: public Lifter, public remill::TraceLifter{
    //The goal here is to get the Lift function working
    public:
      inline VmillTraceLifter(remill::InstructionLifter &inst_lifter_,
                              VmillTraceManager &manager_)
          : remill::TraceLifter(&inst_lifter_, &manager_) {}

      VmillTraceLifter(remill::InstructionLifter *inst_lifter_,
                      VmillTraceManager *manager_);
      
      VmillTraceLifter(const std::shared_ptr<llvm::LLVMContext> &context_);

      std::unique_ptr<llvm::Module> Lift(uint64_t addr_) final;
    
    private:
      std::unique_ptr<llvm::Module> VmillLift(uint64_t addr_);
      std::shared_ptr<VmillTraceManager> manager_ptr;
};
}
} //namespace vmill
