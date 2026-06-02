#pragma once
#include <string>

struct ParsedObject
{
    std::string type;
    std::string content;
};

std::string calculateSHA1(const std::string &input);
std::string compressContent(const std::string &data);
std::string decompressContent(const std::string &compressedData);

std::string storeObject(const std::string &type, const std::string &content);
ParsedObject readObject(const std::string &hash);
