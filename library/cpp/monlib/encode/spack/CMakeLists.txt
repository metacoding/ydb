
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.


find_package(ZLIB REQUIRED)

add_library(monlib-encode-spack)
target_link_libraries(monlib-encode-spack PUBLIC
  contrib-libs-cxxsupp
  yutil
  monlib-encode-buffered
  cpp-monlib-exception
  contrib-libs-lz4
  contrib-libs-xxhash
  ZLIB::ZLIB
  contrib-libs-zstd
)
target_sources(monlib-encode-spack PRIVATE
  ${CMAKE_SOURCE_DIR}/library/cpp/monlib/encode/spack/spack_v1_decoder.cpp
  ${CMAKE_SOURCE_DIR}/library/cpp/monlib/encode/spack/spack_v1_encoder.cpp
  ${CMAKE_SOURCE_DIR}/library/cpp/monlib/encode/spack/varint.cpp
  ${CMAKE_SOURCE_DIR}/library/cpp/monlib/encode/spack/compression.cpp
)
