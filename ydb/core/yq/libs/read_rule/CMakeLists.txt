
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(yq-libs-read_rule)
target_compile_options(yq-libs-read_rule PRIVATE
  -DUSE_CURRENT_UDF_ABI_VERSION
)
target_link_libraries(yq-libs-read_rule PUBLIC
  contrib-libs-cxxsupp
  yutil
  cpp-actors-core
  ydb-core-protos
  yq-libs-events
  api-protos
  lib-operation_id-protos
  cpp-client-ydb_persqueue_core
  providers-common-proto
  dq-api-protos
  providers-pq-proto
)
target_sources(yq-libs-read_rule PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/core/yq/libs/read_rule/read_rule_creator.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/yq/libs/read_rule/read_rule_deleter.cpp
)
