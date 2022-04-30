if(BUILD_MONGO_DB_PLUGIN)

  find_package(mongoc-1.0)
  find_package(mongocxx REQUIRED)

  if ( NOT (mongoc-1.0_FOUND AND  mongocxx_FOUND) )
      message(FATAL_ERROR "Could NOT find mongo-c-driver. Disable mongo support or ensure mongo-c-driver and mongo-cxx-driver is built and installed")
      return()
  endif()

  # This needs to be after the else/return in the situation that libmongoc isn't found and we need to avoid building mongo :: 'bsoncxx/builder/basic/kvp.hpp' file not found
  file(GLOB HEADERS "include/eosio/mongo_db_plugin/*.hpp")
  add_library( mongo_db_plugin
               mongo_db_plugin.cpp
               ${HEADERS} )

  target_include_directories(mongo_db_plugin
          PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )

  target_link_libraries(mongo_db_plugin
          PUBLIC chain_plugin eosio_chain appbase
          mongo::mongoc_shared mongo::mongocxx_shared
          )
else()
  message("mongo_db_plugin not selected and will be omitted.")
endif()
