
#include <iostream>

#include "vmill/BC/TraceLifter.h"
#include "vmill/Executor/Executor.h"

int main(int argc, char const * const * argv){
  std::cout << "Hello World!\n" << " " << std::endl;
  vmill::Executor executor;
  executor.Run();
}
