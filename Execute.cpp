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

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/ManagedStatic.h>

#include "vmill/BC/TraceLifter.h"
#include "vmill/Executor/Executor.h"
#include "vmill/Workspace/Workspace.h"
#include "vmill/Program/Snapshot.h"
#include "vmill/Program/AddressSpace.h"

int main(int argc, char **argv) {
  std::stringstream ss;
  ss << std::endl << std::endl
     << "  " << argv[0] << " \\" << std::endl
     << "    --workspace WORKSPACE_DIR \\" << std::endl
     << "    ..." << std::endl;

  google::InitGoogleLogging(argv[0]);
  google::SetUsageMessage(ss.str());
  google::ParseCommandLineFlags(&argc, &argv, true);

  auto snapshot = vmill::LoadSnapshotFromFile(vmill::Workspace::SnapshotPath());
  vmill::Executor executor;
  vmill::Workspace::LoadSnapshotIntoExecutor(snapshot, executor);
  executor.Run();
  llvm::llvm_shutdown();
}
