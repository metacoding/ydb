
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(tx-replication-ydb_proxy)
target_compile_options(tx-replication-ydb_proxy PRIVATE
  -DUSE_CURRENT_UDF_ABI_VERSION
)
target_link_libraries(tx-replication-ydb_proxy PUBLIC
  contrib-libs-cxxsupp
  yutil
  ydb-core-base
  ydb-core-protos
  cpp-client-ydb_driver
  cpp-client-ydb_scheme
  cpp-client-ydb_table
  client-ydb_types-credentials
)
target_sources(tx-replication-ydb_proxy PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/core/tx/replication/ydb_proxy/ydb_proxy.cpp
)
