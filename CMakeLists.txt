cmake_minimum_required(VERSION 3.10)
project(neogit)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(neogit main.cpp)

target_link_libraries(neogit OpenSSL::Crypto ZLIB::ZLIB)
