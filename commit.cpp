#include "commit.h"
#include "utils.h"
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace std;

static string getAuthorString() {
    const char* userEnv = getenv("USER");
    if (userEnv == nullptr) {
        userEnv = getenv("USERNAME");
    }
    string name = userEnv ? string(userEnv) : "Unknown User";
    time_t now = time(nullptr);
    return name + " " + to_string(static_cast<long long>(now));
}

string writeCommit(const string& treeHash, const string& parentHash, const string& message) {
    if (treeHash.empty()) {
        throw runtime_error("writeCommit: tree hash is required");
    }

    stringstream ss;
    ss << "tree " << treeHash << "\n";
    if (!parentHash.empty()) {
        ss << "parent " << parentHash << "\n";
    }
    ss << "author " << getAuthorString() << "\n";
    ss << "\n";
    ss << message << "\n";

    return storeObject("commit", ss.str());
}

CommitInfo readCommit(const string& hash) {
    ParsedObject obj = readObject(hash);
    if (obj.type != "commit") {
        throw runtime_error("Object " + hash + " is not a commit (type=" + obj.type + ")");
    }

    CommitInfo info;
    info.tree.clear();
    info.parent.clear();
    info.author.clear();
    info.timestamp.clear();
    info.message.clear();

    istringstream iss(obj.content);
    string line;
    bool inMessage = false;
    stringstream msg;
    while (std::getline(iss, line)) {
        if (inMessage) {
            if (!msg.str().empty()) msg << "\n";
            msg << line;
            continue;
        }
        if (line.empty()) {
            inMessage = true;
            continue;
        }
        size_t sp = line.find(' ');
        if (sp == string::npos) continue;
        string key = line.substr(0, sp);
        string val = line.substr(sp + 1);
        if (key == "tree") {
            info.tree = val;
        } else if (key == "parent") {
            info.parent = val;
        } else if (key == "author") {
            istringstream as(val);
            vector<string> tokens;
            string token;
            while (as >> token) tokens.push_back(token);
            if (!tokens.empty()) {
                info.timestamp = tokens.back();
                tokens.pop_back();
            }
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (i > 0) info.author += " ";
                info.author += tokens[i];
            }
        }
    }
    info.message = msg.str();
    return info;
}
