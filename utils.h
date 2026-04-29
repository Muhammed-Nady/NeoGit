#pragma once
#include <string>

std::string calculateSHA1(const std::string &input);
std::string compressContent(const std::string &data);
std::string decompressContent(const std::string &compressedData);