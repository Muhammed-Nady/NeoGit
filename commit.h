#pragma once
#include <string>

struct CommitInfo
{
    std::string tree;
    std::string parent;
    std::string author;
    std::string timestamp;
    std::string message;
};

std::string writeCommit(const std::string &treeHash,
                        const std::string &parentHash,
                        const std::string &message);

CommitInfo readCommit(const std::string &hash);
