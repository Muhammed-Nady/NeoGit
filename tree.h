#pragma once
#include <string>
#include <vector>

struct TreeEntry
{
    std::string mode;
    std::string hash;
    std::string path;
};

std::vector<TreeEntry> loadIndex();
std::string writeTree();
std::vector<TreeEntry> readTree(const std::string &treeHash);
void writeIndex(const std::vector<TreeEntry> &entries);
