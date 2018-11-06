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

#include <glog/logging.h>

#include "remill/BC/Util.h"
#include "remill/OS/FileSystem.h"

#include "vmill/Util/Compiler.h"

#include "vmill/Executor/Executor.h"

#include "vmill/BC/TraceLifter.h"
#include "vmill/Program/AddressSpace.h"

namespace vmill {

Executor::Executor(void)
    : context(new llvm::LLVMContext),
      lifted_code(new llvm::Module("lifted-code", context)),
      trace_manager(lifted_code),
      lifter(Lifter::Create(trace_manager, lifted_code)) {}

Executor::~Executor(void) {}

void Executor::Run(void) {
  auto trace = trace_manager.GetLiftedTraceDefinition(0);
  LOG(ERROR) << &lifter;
}

}  //namespace vmill
