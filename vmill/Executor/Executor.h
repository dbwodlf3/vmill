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

#include <cfenv>
#include <setjmp.h>

#include <iostream>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "remill/BC/Util.h"
#include "remill/OS/FileSystem.h"

#include "vmill/BC/TraceLifter.h"
#include "vmill/Executor/Memory.h"
#include "vmill/Program/AddressSpace.h"
#include "vmill/Util/Compiler.h"


DECLARE_uint64(num_io_threads);
DECLARE_string(tool);

DEFINE_uint64(num_lift_threads, 1,
              "Number of threads that can be used for lifting.");

namespace vmill {
namespace {
// Thread-specific lifters for supporting asynchronous lifting.
static thread_local std::unique_ptr<Lifter> gTLifter;

// Returns a thread-specific lifter object.
static const std::unique_ptr<Lifter> &GetLifter(
    const std::shared_ptr<llvm::LLVMContext> &context);
}
} //namespace vmill
