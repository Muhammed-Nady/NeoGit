#include "tree.h"
#include "utils.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace std;
namespace fs = std::filesystem;

static fs::path indexPath() {
    return fs::path(".neogit") / "index";
}

static vector<TreeEntry> readIndexEntries() {
    vector<TreeEntry> out;
    fs::path p = indexPath();
    if (!fs::exists(p)) return out;

    ifstream f(p);
    string line;
    while (getline(f, line)) {
        if (line.empty()) continue;
        size_t sp1 = line.find(' ');
        if (sp1 == string::npos) continue;
        size_t sp2 = line.find(' ', sp1 + 1);
        if (sp2 == string::npos) continue;
        TreeEntry e;
        e.mode = line.substr(0, sp1);
        e.hash = line.substr(sp1 + 1, sp2 - sp1 - 1);
        e.path = line.substr(sp2 + 1);
        out.push_back(e);
    }
    return out;
}

vector<TreeEntry> loadIndex() {
    return readIndexEntries();
}

string writeTree() {
    vector<TreeEntry> entries = readIndexEntries();
    if (entries.empty()) {
        throw runtime_error("No index file found. Add some files first.");
    }

    map<string, pair<string, string>> dedup;
    for (const auto& e : entries) {
        dedup[e.path] = {e.mode, e.hash};
    }

    stringstream ss;
    for (const auto& kv : dedup) {
        ss << kv.second.first << " " << kv.second.second << " " << kv.first << "\n";
    }

    return storeObject("tree", ss.str());
}

vector<TreeEntry> readTree(const string& treeHash) {
    ParsedObject obj = readObject(treeHash);
    if (obj.type != "tree") {
        throw runtime_error("Object " + treeHash + " is not a tree (type=" + obj.type + ")");
    }

    vector<TreeEntry> out;
    istringstream iss(obj.content);
    string line;
    while (getline(iss, line)) {
        if (line.empty()) continue;
        size_t sp1 = line.find(' ');
        if (sp1 == string::npos) continue;
        size_t sp2 = line.find(' ', sp1 + 1);
        if (sp2 == string::npos) continue;
        TreeEntry e;
        e.mode = line.substr(0, sp1);
        e.hash = line.substr(sp1 + 1, sp2 - sp1 - 1);
        e.path = line.substr(sp2 + 1);
        out.push_back(e);
    }
    return out;
}

void writeIndex(const vector<TreeEntry>& entries) {
    ofstream f(indexPath(), ios::trunc);
    for (const auto& e : entries) {
        f << e.mode << " " << e.hash << " " << e.path << "\n";
    }
    f.close();
}
