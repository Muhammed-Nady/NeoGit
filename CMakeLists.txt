cmake_minimum_required(VERSION 3.10)
project(NeoGit)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(neogit main.cpp utils.cpp repository.cpp)

target_link_libraries(neogit OpenSSL::Crypto ZLIB::ZLIB)