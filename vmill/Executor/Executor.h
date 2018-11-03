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

#include <cfenv>
#include <setjmp.h>

#include <iostream>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "remill/BC/Util.h"
#include "remill/OS/FileSystem.h"

#include "vmill/BC/TraceLifter.h"
#include "vmill/Program/AddressSpace.h"
#include "vmill/Util/Compiler.h"

namespace llvm {
class LLVMContext;
}

namespace vmill {
class AddressSpace;
class Lifter;

class Executor {
  public:
   Executor(void);
   void Run(void);
  private:
   std::shared_ptr<llvm::LLVMContext> context;
};
} //namespace vmill
