
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.


find_package(Python3 REQUIRED)

add_library(core-kqp-provider)
target_compile_options(core-kqp-provider PRIVATE
  -DUSE_CURRENT_UDF_ABI_VERSION
)
target_link_libraries(core-kqp-provider PUBLIC
  contrib-libs-cxxsupp
  yutil
  ydb-core-base
  kqp-provider-mkql
  ydb-core-protos
  ydb-library-aclib
  library-aclib-protos
  ydb-library-binary_json
  ydb-library-dynumber
  yql-core-services
  library-yql-minikql
  yql-public-decimal
  public-lib-scheme_types
  yql-core-expr_nodes
  providers-common-codec
  providers-common-config
  providers-common-gateway
  providers-common-proto
  providers-common-provider
  common-schema-expr
  providers-result-expr_nodes
  providers-result-provider
  tools-enum_parser-enum_serialization_runtime
)
target_sources(core-kqp-provider PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/kqp_opt_helpers.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_datasink.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_datasource.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_exec.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_gateway.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_kql.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_mkql.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_opt_build.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_opt_join.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_opt_range.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_opt_utils.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_opt.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_provider.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_query_traits.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_results.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_settings.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_type_ann.cpp
  ${CMAKE_BINARY_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.gen.h
  ${CMAKE_BINARY_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.decl.inl.h
  ${CMAKE_BINARY_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.defs.inl.h
)
add_custom_command(
  OUTPUT
  ${CMAKE_BINARY_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.gen.h
  ${CMAKE_BINARY_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.decl.inl.h
  ${CMAKE_BINARY_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.defs.inl.h
  DEPENDS
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/core/expr_nodes_gen/yql_expr_nodes_gen.jnj
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.json
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/core/expr_nodes_gen/gen/__main__.py
  COMMAND
  Python3::Interpreter
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/core/expr_nodes_gen/gen/__main__.py
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/core/expr_nodes_gen/yql_expr_nodes_gen.jnj
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.json
  ${CMAKE_BINARY_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.gen.h
  ${CMAKE_BINARY_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.decl.inl.h
  ${CMAKE_BINARY_DIR}/ydb/core/kqp/provider/yql_kikimr_expr_nodes.defs.inl.h
)
generate_enum_serilization(core-kqp-provider
  ${CMAKE_SOURCE_DIR}/ydb/core/kqp/provider/yql_kikimr_provider.h
  INCLUDE_HEADERS
  ydb/core/kqp/provider/yql_kikimr_provider.h
)
