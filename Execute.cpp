#include "vmill/BC/TraceLifter.h"
#include "vmill/Executor/Executor.h"

int main(int argc, char const * const * argv){
  auto context_ptr = std::shared_ptr<llvm::LLVMContext>(new llvm::LLVMContext);
  auto &lifter = vmill::GetLifter(context_ptr);
  std::cout << "Hello World!\n" << " " << &lifter << std::endl;
}
