#pragma once
#include <string>

void handleInit();
void handleHashObject(const std::string &filename, bool writeToStore = false);
void handleCatFile(const std::string &hash);
void handleAdd(const std::string &filename);
void handleCommit(const std::string &message);