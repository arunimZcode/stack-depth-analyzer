cmake_minimum_required(VERSION 3.20)
project(StackAnalyzer)

find_package(LLVM REQUIRED CONFIG
  PATHS /usr/lib/llvm17/lib/cmake/llvm
  NO_DEFAULT_PATH)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

# Fix broken Alpine LLVM17 package - create missing dummy lib
if(NOT EXISTS "/usr/lib/llvm17/lib/libLLVMTestingAnnotations.a")
  file(WRITE /tmp/dummy.c "")
  execute_process(COMMAND ar rcs
    /usr/lib/llvm17/lib/libLLVMTestingAnnotations.a)
endif()

include_directories(${LLVM_INCLUDE_DIRS} include/)
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(LLVM_LIBS
  core support irreader analysis codegen passes
  selectiondag mcparser mc target
  transformutils scalaropts instcombine
  bitreader bitwriter linker ipo)

add_library(StackAnalyzerPasses STATIC
  lib/StackFrameEstimator.cpp
  lib/CallChainAnalyzer.cpp)
target_link_libraries(StackAnalyzerPasses ${LLVM_LIBS})

add_executable(stack-analyzer tools/stack-analyzer.cpp)
target_link_libraries(stack-analyzer
  -Wl,--whole-archive
  StackAnalyzerPasses
  -Wl,--no-whole-archive
  ${LLVM_LIBS}
  -lrt -ldl -lm)
