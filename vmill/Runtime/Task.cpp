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

#include "vmill/Runtime/Task.h"
#include "remill/Arch/Runtime/State.h"
#include "remill/Arch/Runtime/Intrinsics.cpp"

namespace vmill {

  extern "C" {
    Task __vmill_task_0 = {};

    int __vmill_entrypoint(int argc, char *argv[], char *envp[]) {
      auto state = &(__vmill_task_0 -> state);
      __remill_jump(state,state -> gpr.rip.aword, nullptr);
      return 0;
    }

}  // namespace vmill
