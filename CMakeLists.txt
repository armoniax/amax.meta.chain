cmake_minimum_required( VERSION 3.8 )

project( AMAX )
include(CTest) # suppresses DartConfiguration.tcl error
enable_testing()

if ("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/local")
   message(WARNING "CMAKE_INSTALL_PREFIX is set to /usr/local. This is not recommended.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

if (UNIX AND APPLE)
   list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/gettext")
endif()

include( GNUInstallDirs )
include( InstallDirectoryPermissions )
include( MASSigning )

set( BLOCKCHAIN_NAME "AMAX" )
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_EXTENSIONS ON )
set( CXX_STANDARD_REQUIRED ON)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 2)
# set(VERSION_SUFFIX alpha3)

if(VERSION_SUFFIX)
    set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}-${VERSION_SUFFIX}")
else()
    set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
endif()

set( CLI_CLIENT_EXECUTABLE_NAME amcli )
set( NODE_EXECUTABLE_NAME amnod )
set( KEY_STORE_EXECUTABLE_NAME amkey )
set( WALLET_FOLDER_NAME amax-wallet )

# http://stackoverflow.com/a/18369825
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
        message(FATAL_ERROR "GCC version must be at least 8.0!")
    endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
        message(FATAL_ERROR "Clang version must be at least 5.0!")
    endif()
endif()

if ("${CMAKE_GENERATOR}" STREQUAL "Ninja")
   add_compile_options(-fdiagnostics-color=always)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")
set(BUILD_DOXYGEN FALSE CACHE BOOL "Build doxygen documentation on every make")
set(BUILD_MONGO_DB_PLUGIN FALSE CACHE BOOL "Build mongo database plugin")
set(ENABLE_MULTIVERSION_PROTOCOL_TEST FALSE CACHE BOOL "Enable amnod multiversion protocol test")

# add defaults for openssl
if(APPLE AND UNIX AND "${OPENSSL_ROOT_DIR}" STREQUAL "")
   set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@1.1")
   set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OPENSSL_ROOT_DIR}/lib/pkgconfig")
endif()

# WASM runtimes to enable. Each runtime in this list will have:
#  * definition AMAX_<RUNTIME>_RUNTIME_ENABLED defined in public libchain interface
#  * ctest entries with --runtime
list(APPEND AMAX_WASM_RUNTIMES wabt)
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm64" AND APPLE)
   message(STATUS "eos-vm can not be compiled on Apple M1 Prosessor, CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")
else()
   list(APPEND AMAX_WASM_RUNTIMES eos-vm)   #always enable wabt, eos-vm; it works everywhere and parts of amax still assume it's always available
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT WIN32)
   if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" AND "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
      list(APPEND AMAX_WASM_RUNTIMES eos-vm-oc)
      # EOS VM OC requires LLVM, but move the check up here to a central location so that the EosioTester.cmakes
      # can be created with the exact version found
      find_package(LLVM REQUIRED CONFIG)
      if(LLVM_VERSION_MAJOR VERSION_LESS 7 OR LLVM_VERSION_MAJOR VERSION_GREATER_EQUAL 12)
	      message(FATAL_ERROR "AMAX requires an LLVM version 7 through 11")
      endif()
   endif()
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT WIN32)
   if(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
      list(APPEND AMAX_WASM_RUNTIMES eos-vm-jit)
   endif()
endif()

list(APPEND EOSIO_WASM_RUNTIMES AMAX_WASM_RUNTIMES) # be compatible for wasm-spec-tests

if(UNIX)
  if(APPLE)
    set(whole_archive_flag "-force_load")
    set(no_whole_archive_flag "")
  else()
    set(whole_archive_flag "--whole-archive")
    set(no_whole_archive_flag "--no-whole-archive")
  endif()
else()
  set(whole_archive_flag "--whole-archive")
  set(no_whole_archive_flag "--no-whole-archive")
endif()

if(NOT DEFINED BOOST_ROOT AND DEFINED ENV{BOOST_ROOT})
  message(STATUS "reset BOOST_ROOT=ENV{BOOST_ROOT}:$ENV{BOOST_ROOT}")
  set( BOOST_ROOT $ENV{BOOST_ROOT} CACHE STRING "BOOST_ROOT")
endif()

set( Boost_USE_STATIC_LIBS ON CACHE STRING "ON or OFF" )
# Most boost deps get implictly picked up via fc, as just about everything links to fc. In addition we pick up
# the pthread dependency through fc.
find_package(Boost 1.67 REQUIRED COMPONENTS program_options unit_test_framework)

# fix compile error of <boost/container_hash/hash.hpp>, avoid to use std::unary_function
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_HAS_AUTO_PTR_ETC=0" )

if( APPLE AND UNIX )
# Apple Specific Options Here
    message( STATUS "Configuring AMAX on macOS" )
    set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -Wall -Wno-deprecated-declarations" )
else()
    # Linux Specific Options Here
    message( STATUS "Configuring AMAX on Linux" )
    set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -Wall" )
    if ( FULL_STATIC_BUILD )
      set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
    endif ( FULL_STATIC_BUILD )

    if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
        if( CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL 4.0.0 OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.0.0 )
            set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-partial-specialization" )
        endif()
    endif()
endif()

# based on http://www.delorie.com/gnu/docs/gdb/gdb_70.html
# uncomment this line to tell GDB about macros (slows compile times)
# set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-2 -g3" )

set(ENABLE_COVERAGE_TESTING FALSE CACHE BOOL "Build AMAX for code coverage analysis")

if(ENABLE_COVERAGE_TESTING)
  SET(CMAKE_CXX_FLAGS "--coverage ${CMAKE_CXX_FLAGS}")
  find_program( LCOV_PATH lcov )
  find_program( LLVMCOV_PATH  llvm-cov )
  find_program( GENHTML_PATH NAMES genhtml)
endif()

include(utils)

if ("${CORE_SYMBOL_NAME}" STREQUAL "")
  set( CORE_SYMBOL_NAME "SYS" )
endif()
string(TOUPPER ${CORE_SYMBOL_NAME} CORE_SYMBOL_NAME)

string(LENGTH ${CORE_SYMBOL_NAME} CORE_SYMBOL_NAME_LENGTH)
if (CORE_SYMBOL_NAME_LENGTH GREATER 7)
  message(FATAL_ERROR "CORE_SYMBOL_NAME length must be between 1 and 7 characters")
endif()

message( STATUS "Using '${CORE_SYMBOL_NAME}' as CORE symbol name" )

if ("${AMAX_ROOT_KEY}" STREQUAL "")
   set(AMAX_ROOT_KEY "AM6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV")
endif()

message( STATUS "Using '${AMAX_ROOT_KEY}' as public key for 'amax' account" )

add_subdirectory( libraries )
add_subdirectory( plugins )
add_subdirectory( programs )
add_subdirectory( scripts )
add_subdirectory( unittests )
add_subdirectory( tests )
add_subdirectory( tools )

if (NOT DISABLE_WASM_SPEC_TESTS)
add_subdirectory( wasm-spec-tests/generated-tests )
endif()

install_directory_permissions(DIRECTORY ${CMAKE_INSTALL_FULL_SYSCONFDIR}/amax)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/testnet.template DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/amax/launcher)
install_directory_permissions(DIRECTORY ${CMAKE_INSTALL_FULL_SYSCONFDIR}/amax)
install_directory_permissions(DIRECTORY ${CMAKE_INSTALL_FULL_SYSCONFDIR}/amax/launcher)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/testnet.template ${CMAKE_CURRENT_BINARY_DIR}/etc/amax/launcher/testnet.template COPYONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/amax.version.in ${CMAKE_CURRENT_BINARY_DIR}/amax.version.hpp)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/amax.version.hpp DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR})

set(AMA_ROOT_DIR ${CMAKE_BINARY_DIR})
configure_file(${CMAKE_SOURCE_DIR}/CMakeModules/amax-config.cmake.in ${CMAKE_BINARY_DIR}/lib/cmake/amax/amax-config.cmake @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/CMakeModules/EosioTesterBuild.cmake.in ${CMAKE_BINARY_DIR}/lib/cmake/amax/EosioTester.cmake @ONLY)

set(AMA_ROOT_DIR ${CMAKE_INSTALL_PREFIX})
configure_file(${CMAKE_SOURCE_DIR}/CMakeModules/amax-config.cmake.in ${CMAKE_BINARY_DIR}/modules/amax-config.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/modules/amax-config.cmake DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/amax)
configure_file(${CMAKE_SOURCE_DIR}/CMakeModules/EosioTester.cmake.in ${CMAKE_BINARY_DIR}/modules/EosioTester.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/modules/EosioTester.cmake DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/amax)

configure_file(${CMAKE_SOURCE_DIR}/LICENSE
               ${CMAKE_BINARY_DIR}/licenses/amax/LICENSE COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/wabt/LICENSE
               ${CMAKE_BINARY_DIR}/licenses/amax/LICENSE.wabt COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/softfloat/COPYING.txt
               ${CMAKE_BINARY_DIR}/licenses/amax/LICENSE.softfloat COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/wasm-jit/LICENSE
               ${CMAKE_BINARY_DIR}/licenses/amax/LICENSE.wavm COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/fc/secp256k1/upstream/COPYING
               ${CMAKE_BINARY_DIR}/licenses/amax/LICENSE.secp256k1 COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/fc/include/fc/crypto/webauthn_json/license.txt
               ${CMAKE_BINARY_DIR}/licenses/amax/LICENSE.rapidjson COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/fc/src/network/LICENSE.go
               ${CMAKE_BINARY_DIR}/licenses/amax/LICENSE.go COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/yubihsm/LICENSE
               ${CMAKE_BINARY_DIR}/licenses/amax/LICENSE.yubihsm COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/eos-vm/LICENSE
               ${CMAKE_BINARY_DIR}/licenses/amax/LICENSE.eos-vm COPYONLY)

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/amax/ COMPONENT base)
install(FILES libraries/wabt/LICENSE DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/amax/ RENAME LICENSE.wabt COMPONENT base)
install(FILES libraries/softfloat/COPYING.txt DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/amax/ RENAME LICENSE.softfloat COMPONENT base)
install(FILES libraries/wasm-jit/LICENSE DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/amax/ RENAME LICENSE.wavm COMPONENT base)
install(FILES libraries/fc/secp256k1/upstream/COPYING DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/amax/ RENAME LICENSE.secp256k1 COMPONENT base)
install(FILES libraries/fc/include/fc/crypto/webauthn_json/license.txt DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/amax/ RENAME LICENSE.rapidjson COMPONENT base)
install(FILES libraries/fc/src/network/LICENSE.go DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/amax/ COMPONENT base)
install(FILES libraries/yubihsm/LICENSE DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/amax/ RENAME LICENSE.yubihsm COMPONENT base)
install(FILES libraries/eos-vm/LICENSE DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/amax/ RENAME LICENSE.eos-vm COMPONENT base)

add_custom_target(base-install
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
  COMMAND "${CMAKE_COMMAND}" -DCMAKE_INSTALL_COMPONENT=base -P "${CMAKE_BINARY_DIR}/cmake_install.cmake"
  USES_TERMINAL
)

get_property(_CTEST_CUSTOM_TESTS_IGNORE GLOBAL PROPERTY CTEST_CUSTOM_TESTS_IGNORE)
file(WRITE "${CMAKE_BINARY_DIR}/CTestCustom.cmake" "SET(CTEST_CUSTOM_TESTS_IGNORE ${_CTEST_CUSTOM_TESTS_IGNORE})")

include(package)
include(doxygen)
