#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/ManagedStatic.h>

#include "vmill/BC/TraceLifter.h"
#include "vmill/Executor/Executor.h"
#include "vmill/Workspace/Workspace.h"
#include "vmill/Program/Snapshot.h"
#include "vmill/Program/AddressSpace.h"

int main(int argc, char const * const * argv){
  auto snapshot = vmill::LoadSnapshotFromFile("/home/sai/ToB/remill/tools/tlift/vmill/snapshot");
  vmill::Executor executor;
  vmill::Workspace::LoadSnapshotIntoExecutor(snapshot, executor);
  executor.Run();
  llvm::llvm_shutdown();
}
