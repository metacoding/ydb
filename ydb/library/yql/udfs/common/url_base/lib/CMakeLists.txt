
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(common-url_base-lib)
target_compile_options(common-url_base-lib PRIVATE
  -DUDF_ABI_VERSION_MAJOR=2
  -DUDF_ABI_VERSION_MINOR=23
  -DUDF_ABI_VERSION_PATCH=0
)
target_link_libraries(common-url_base-lib PUBLIC
  contrib-libs-cxxsupp
  yutil
  library-cpp-charset
  cpp-string_utils-quote
  cpp-string_utils-url
  library-cpp-tld
  cpp-unicode-punycode
  library-cpp-uri
  yql-public-udf
)
target_sources(common-url_base-lib PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/udfs/common/url_base/lib/url_base_udf.cpp
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/udfs/common/url_base/lib/url_parse.cpp
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/udfs/common/url_base/lib/url_query.cpp
)
