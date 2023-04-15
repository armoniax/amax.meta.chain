file(GLOB HEADERS "include/eosio/unique_pubkey_plugin/*.hpp")
add_library( unique_pubkey_plugin
             unique_pubkey_plugin.cpp
             ${HEADERS} )

target_link_libraries( unique_pubkey_plugin producer_plugin chain_plugin history_plugin )
target_include_directories( unique_pubkey_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
