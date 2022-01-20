# will be implictly used for any compilation unit if not overrided by SYSTEM_INCLUDE_FOLDERS parameter
# these directories go as -isystem <dir> to avoid warnings from code of third-party libraries

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/amax.bios/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/amax.bios/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/amax.msig/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/amax.msig/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/amax.system/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/amax.system/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/amax.token/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/amax.token/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/amax.wrap/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/amax.wrap/)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/old_versions/v1.6.0-rc3/amax.bios/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/old_versions/v1.6.0-rc3/amax.bios/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/old_versions/v1.7.0-develop-preactivate_feature/amax.bios/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/old_versions/v1.7.0-develop-preactivate_feature/amax.bios/)
