cmake_minimum_required(VERSION 3.16)
project(ClientSupportSystem VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CSMS_BUILD_TESTS "Build the GoogleTest unit tests" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(SQLite3 REQUIRED)

# libsodium: the established password-hashing library (Argon2id).
# Found through pkg-config when available, otherwise by header/library search.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SODIUM IMPORTED_TARGET libsodium)
endif()
if(NOT TARGET PkgConfig::SODIUM)
  find_path(SODIUM_INCLUDE_DIR sodium.h)
  find_library(SODIUM_LIBRARY NAMES sodium libsodium)
  if(NOT SODIUM_INCLUDE_DIR OR NOT SODIUM_LIBRARY)
    message(FATAL_ERROR
      "libsodium not found. Install it (e.g. 'sudo apt install libsodium-dev' or "
      "'brew install libsodium') and re-run CMake.")
  endif()
  add_library(PkgConfig::SODIUM INTERFACE IMPORTED)
  set_target_properties(PkgConfig::SODIUM PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SODIUM_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${SODIUM_LIBRARY}")
endif()

# ---- Core library: everything except main(), so tests can link against it ----
add_library(csms_core STATIC
  src/common/TimeUtils.cpp
  src/common/Validation.cpp
  src/models/Enums.cpp
  src/database/Statement.cpp
  src/database/Database.cpp
  src/database/Transaction.cpp
  src/database/Schema.cpp
  src/repositories/ClientRepository.cpp
  src/repositories/StaffRepository.cpp
  src/repositories/CaseRepository.cpp
  src/repositories/HistoryRepository.cpp
  src/repositories/ReportRepository.cpp
  src/services/Session.cpp
  src/services/CaseWorkflow.cpp
  src/services/ClientService.cpp
  src/services/CaseService.cpp
  src/services/PasswordHasher.cpp
  src/services/StaffService.cpp
  src/services/AuthService.cpp
  src/services/ReportService.cpp
  src/cli/Console.cpp
  src/cli/Format.cpp
  src/cli/ClientMenu.cpp
  src/cli/CaseMenu.cpp
  src/cli/StaffMenu.cpp
  src/cli/Application.cpp
)
target_include_directories(csms_core PUBLIC include)
target_link_libraries(csms_core PUBLIC SQLite::SQLite3 PkgConfig::SODIUM)

if(MSVC)
  target_compile_options(csms_core PUBLIC /W4)
else()
  target_compile_options(csms_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

# ---- Application ----
add_executable(csms src/main.cpp)
target_link_libraries(csms PRIVATE csms_core)

# ---- Tests ----
if(CSMS_BUILD_TESTS)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    message(STATUS "System GoogleTest not found - fetching it with FetchContent")
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
  add_subdirectory(tests)
endif()
