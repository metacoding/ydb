
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(library-persqueue-topic_parser_public)
target_link_libraries(library-persqueue-topic_parser_public PUBLIC
  contrib-libs-cxxsupp
  yutil
)
target_sources(library-persqueue-topic_parser_public PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/library/persqueue/topic_parser_public/topic_parser.cpp
)
