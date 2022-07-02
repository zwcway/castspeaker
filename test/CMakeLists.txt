
if (ESP_PLATFORM)
  return()
endif ()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)


include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)

# Check headers
set(INCLUDES "")
macro(ck_check_include_file header var)
  check_include_files("${INCLUDES};${header}" ${var})
  if (${var})
    set(INCLUDES ${INCLUDES} ${header})
  endif (${var})
endmacro(ck_check_include_file)
###############################################################################
# Check for integer types
# (The following are used in check.h. Regardless if they are used in
# the project, they will need to be checked in order to use Check).
check_type_size(intmax_t INTMAX_T)
check_type_size(uintmax_t UINTMAX_T)

check_type_size(pid_t PID_T)
if (NOT HAVE_PID_T)
  if (WIN32)
    set(pid_t "int")
  else (WIN32)
    MESSAGE(FATAL_ERROR "pid_t doesn't exist on this platform?")
  endif (WIN32)
endif (NOT HAVE_PID_T)

###############################################################################
# Check libraries

###############################################################################
# Generate "config.h" from "cmake/config.h.cmake"
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DHAVE_CONFIG_H)
set(CONFIG_HEADER ${CMAKE_CURRENT_BINARY_DIR}/config.h)

###############################################################################
# Subdirectories
#add_subdirectory(src)
#add_subdirectory(tests)

###############################################################################
# Unit tests
enable_testing()
add_test(NAME test_main COMMAND test_main)

ck_check_include_file("stdlib.h" HAVE_STDLIB_H)
set(CHECK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})


include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# If pkg-config is not installed on the system, then the
# CHECK_INSTALL_DIR variable must be set to the install
# location of Check. For example, on Windows, this may
# be: C:/Program Files/check
# set(CHECK_INSTALL_DIR "C:/Program Files/check")

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})
link_directories(${CHECK_LIBRARY_DIRS})

set(TEST_SOURCES
    test_main.c
    test_common.c test.h)

add_executable(test ${TEST_SOURCES})
target_link_libraries(test common m rt subunit ${CHECK_LIBRARIES})


add_library(Check INTERFACE)
target_include_directories(Check INTERFACE ${CATCH_INCLUDE_DIR} ${CMAKE_SOURCE_DIR})
