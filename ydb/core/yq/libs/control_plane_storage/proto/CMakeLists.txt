
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(libs-control_plane_storage-proto)
target_link_libraries(libs-control_plane_storage-proto PUBLIC
  contrib-libs-cxxsupp
  yutil
  yq-libs-protos
  api-protos
  yql-dq-proto
  dq-api-protos
  contrib-libs-protobuf
)
target_proto_messages(libs-control_plane_storage-proto PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/core/yq/libs/control_plane_storage/proto/yq_internal.proto
)
target_proto_addincls(libs-control_plane_storage-proto
  ./
  ${CMAKE_SOURCE_DIR}/
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/contrib/libs/protobuf/src
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/contrib/libs/protobuf/src
)
target_proto_outs(libs-control_plane_storage-proto
  --cpp_out=${CMAKE_BINARY_DIR}/
  --cpp_styleguide_out=${CMAKE_BINARY_DIR}/
)
