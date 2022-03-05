add_contract(amax.basic amax.basic ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.basic.cpp)

target_include_directories(amax.basic
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.basic
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.basic.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.basic.contracts.md @ONLY )

target_compile_options( amax.basic PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
