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
#include "Executor.h"
#include "vmill/BC/TraceLifter.h"

namespace vmill {
thread_local Executor *gExecutor = nullptr;

namespace {

static thread_local std::unique_ptr<Lifter> gTLifter;
// Returns a thread-specific lifter object.
static const std::unique_ptr<Lifter> &GetLifter(
    const std::shared_ptr<llvm::LLVMContext> &context) {
  if (unlikely(!gTLifter)) {
    Lifter::Create(context).swap(gTLifter);
  }
  return gTLifter;
}
} //namespace

Executor::Executor(void)
  : context(new llvm::LLVMContext) {}

void  Executor::Run(void){
    auto &lifter = GetLifter(context);
    std::cout << &lifter << std::endl;
}

} //namespace vmill
