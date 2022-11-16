cmake_minimum_required(VERSION 3.16.3)

project(MyQuiver VERSION 1.0.0)

add_library(ha_my_quiver MODULE ha_my_quiver.cpp)

set_property(TARGET ha_my_quiver PROPERTY CXX_STANDARD 17)
set_property(TARGET ha_my_quiver PROPERTY PREFIX "")

# Need to use -bundle_loader to resolve symbols by mysqld executable.
set(MY_QUIVER_MYSQLD "mysqld" CACHE STRING "Path to mysqld executable")
find_program(MY_QUIVER_MYSQLD_ABSOLUTE ${MY_QUIVER_MYSQLD} REQUIRED)
add_executable(mysqld IMPORTED)
set_property(TARGET mysqld PROPERTY IMPORTED_LOCATION ${MY_QUIVER_MYSQLD_ABSOLUTE})
set_property(TARGET mysqld PROPERTY ENABLE_EXPORTS TRUE)
set(MY_QUIVER_MYSQL_SOURCE_DIR "" CACHE PATH "MySQL source directory to build with")
set(MY_QUIVER_MYSQL_BUILD_DIR "" CACHE PATH "MySQL build directory to build with")
target_include_directories(mysqld INTERFACE ${MY_QUIVER_MYSQL_SOURCE_DIR} ${MY_QUIVER_MYSQL_SOURCE_DIR}/include ${MY_QUIVER_MYSQL_SOURCE_DIR}/extra/rapidjson/include ${MY_QUIVER_MYSQL_BUILD_DIR}/include)
find_package(OpenSSL REQUIRED)
target_link_libraries(mysqld INTERFACE OpenSSL::SSL)

find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)
find_package(ArrowDataset REQUIRED)

target_link_libraries(ha_my_quiver PRIVATE mysqld parquet_shared arrow_dataset_shared)

configure_file(
  "${PROJECT_SOURCE_DIR}/config.sh.in"
  "${PROJECT_BINARY_DIR}/config.sh")
