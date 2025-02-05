include(FetchContent)

################################################################################
# Memory check

if(ENABLE_MEMCHECK)
  find_file(MEMCHECK_SUPPRESS_FILE
    DOC "Suppression file for memory checking"
    NAMES openmpi-valgrind.supp
    PATHS
      /usr/share/openmpi
      /usr/lib64/openmpi/share
      /usr/lib64/openmpi/share/openmpi
      /usr/share)
  if(MEMCHECK_SUPPRESS_FILE)
    set(MEMCHECK_SUPPRESS "--suppressions=${PROJECT_SOURCE_DIR}/test/valgrind.supp --suppressions=${MEMCHECK_SUPPRESS_FILE}")
  else()
    set(MEMCHECK_SUPPRESS "--suppressions=${PROJECT_SOURCE_DIR}/test/valgrind.supp")
  endif()
endif()

################################################################################
# OpenMP

if(ENABLE_OPENMP)
  find_package(OpenMP REQUIRED)
  message(STATUS "Compiling with OpenMP support")
endif()

################################################################################
# MPI

if(ENABLE_MPI)
  find_package(MPI REQUIRED)
  message(STATUS "Compiling with MPI support")
endif()

################################################################################
# google test

if(PROJECT_IS_TOP_LEVEL)
  # if google test isn't installed, fetch content will download and build what is needed
  # but, we don't want to run clang tidy on google test, save those variables and reset them later
  foreach (lang IN ITEMS C CXX)
    set("CMAKE_${lang}_CLANG_TIDY_save" "${CMAKE_${lang}_CLANG_TIDY}")
    set("CMAKE_${lang}_CLANG_TIDY" "")
  endforeach ()

  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG be03d00f5f0cc3a997d1a368bee8a1fe93651f48
    # FIND_PACKAGE_ARGS GTest
  )

  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)

  foreach (lang IN ITEMS C CXX)
    set("CMAKE_${lang}_CLANG_TIDY" "${CMAKE_${lang}_CLANG_TIDY_save}")
  endforeach ()
endif()

################################################################################
# nlohmann::json

if(ENABLE_JSON)
  FetchContent_Declare(json
      GIT_REPOSITORY https://github.com/nlohmann/json.git
      GIT_TAG v3.11.2
  )
  FetchContent_MakeAvailable(json)
endif()

################################################################################
# Docs

if(BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  find_package(Sphinx REQUIRED)
endif()

################################################################################
# GPU Support

if(ENABLE_CUDA)
  find_package(CUDA REQUIRED)
  enable_language(CUDA)
endif()

if(ENABLE_OPENACC)
  find_package(OpenACC REQUIRED)
endif()

################################################################################
# LLVM Support
#
# TODO: Try to use fetch content for LLVM libraries

if(ENABLE_LLVM)
  find_package(LLVM REQUIRED CONFIG)
  if(LLVM_FOUND)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

    include_directories(${LLVM_INCLUDE_DIRS})
    separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
    add_definitions(${LLVM_DEFINITIONS_LIST})

    llvm_map_components_to_libnames(llvm_libs support core orcjit native irreader)
  else()
    set(LLVM_CMD "llvm-config --cxxflags --ldflags --system-libs --libs support core orcjit native irreader | tr '\\n' ' '")
    execute_process(COMMAND bash "-c" ${LLVM_CMD}
                    OUTPUT_VARIABLE llvm_libs)
    separate_arguments(llvm_libs)
  endif()
endif()