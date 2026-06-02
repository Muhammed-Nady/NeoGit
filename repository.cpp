#include "repository.h"
#include "utils.h"
#include "tree.h"
#include "commit.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <set>
#include <cctype>

using namespace std;
namespace fs = std::filesystem;

// --- Path helpers ---

static string normalizePath(const string& p) {
    string s = p;
    for (auto& c : s) {
        if (c == '\\') c = '/';
    }
    while (s.rfind("./", 0) == 0) {
        s = s.substr(2);
    }
    return s;
}

static fs::path toNative(const string& p) {
    fs::path out(p);
    return out.make_preferred();
}

// --- HEAD helpers ---

static fs::path headPath() {
    return fs::path(".neogit") / "HEAD";
}

static string getHeadCommit() {
    fs::path p = headPath();
    if (!fs::exists(p)) return "";
    ifstream f(p);
    stringstream ss;
    ss << f.rdbuf();
    string h = ss.str();
    // trim whitespace
    while (!h.empty() && (h.back() == '\n' || h.back() == '\r' || h.back() == ' ')) {
        h.pop_back();
    }
    if (h.size() != 40) return "";
    bool allHex = true;
    for (char c : h) {
        if (!isxdigit(static_cast<unsigned char>(c))) { allHex = false; break; }
    }
    return allHex ? h : "";
}

static void setHeadCommit(const string& hash) {
    ofstream f(headPath(), ios::trunc);
    f << hash << "\n";
    f.close();
}

static bool isValidHash(const string& h) {
    if (h.size() != 40) return false;
    for (char c : h) {
        if (!isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// --- Core commands ---

void handleInit() {
    try {
        fs::path repo_path = ".neogit";
        if (fs::exists(repo_path)) {
            cout << "NeoGit repository already exists." << endl;
            return;
        }
        fs::create_directory(repo_path);
        fs::create_directory(repo_path / "objects");
        fs::create_directory(repo_path / "refs");
        cout << "Initialized empty NeoGit repository in " << fs::absolute(repo_path) << endl;
    } catch (const fs::filesystem_error& e) {
        cerr << "Error initializing repository: " << e.what() << endl;
    }
}

void handleHashObject(const string& filename, bool writeToStore) {
    if (!fs::exists(filename)) {
        cerr << "Fatal: Cannot open '" << filename << "': No such file" << endl;
        return;
    }
    try {
        ifstream file(filename, ios::binary);
        stringstream buffer;
        buffer << file.rdbuf();
        string content = buffer.str();
        string hash = storeObject("blob", content);
        cout << hash << endl;
        if (writeToStore) {
            cout << "Object stored successfully." << endl;
        }
    } catch (const exception& e) {
        cerr << "Error hashing object: " << e.what() << endl;
    }
}

void handleCatFile(const string& hash) {
    if (!isValidHash(hash)) {
        cerr << "Fatal: Not a valid object name " << hash << endl;
        return;
    }
    try {
        ParsedObject obj = readObject(hash);
        if (obj.type != "blob") {
            cerr << "Fatal: Object " << hash << " is type '" << obj.type << "', not a blob" << endl;
            return;
        }
        cout << obj.content;
        if (obj.content.empty() || obj.content.back() != '\n') cout << endl;
    } catch (const exception& e) {
        cerr << "Error reading object: " << e.what() << endl;
    }
}

void handleAdd(const string& filename) {
    if (!fs::exists(filename)) {
        cerr << "Fatal: pathspec '" << filename << "' did not match any files" << endl;
        return;
    }
    try {
        ifstream file(filename, ios::binary);
        stringstream buffer;
        buffer << file.rdbuf();
        string content = buffer.str();
        string hash = storeObject("blob", content);

        string normPath = normalizePath(filename);

        // Read current index, drop existing entry for this path
        vector<TreeEntry> existing;
        try {
            existing = loadIndex();
        } catch (...) {
            existing.clear();
        }
        vector<TreeEntry> filtered;
        for (const auto& e : existing) {
            if (e.path != normPath) filtered.push_back(e);
        }
        TreeEntry ne;
        ne.mode = "100644";
        ne.hash = hash;
        ne.path = normPath;
        filtered.push_back(ne);
        writeIndex(filtered);

        cout << "Added '" << filename << "' to index." << endl;
    } catch (const exception& e) {
        cerr << "Error adding file: " << e.what() << endl;
    }
}

void handleCommit(const string& message) {
    try {
        string treeHash = writeTree();
        string parent = getHeadCommit();

        string commitHash = writeCommit(treeHash, parent, message);

        setHeadCommit(commitHash);

        // Clear the index (matching git: index is empty after commit)
        fs::path idx = fs::path(".neogit") / "index";
        if (fs::exists(idx)) {
            ofstream clear(idx, ios::trunc);
            clear.close();
        }

        cout << "[" << commitHash.substr(0, 7) << "] " << message << endl;
    } catch (const exception& e) {
        cerr << "Error during commit: " << e.what() << endl;
    }
}

void handleLog() {
    string current = getHeadCommit();
    if (current.empty()) {
        cout << "No commits yet." << endl;
        return;
    }
    try {
        while (!current.empty()) {
            CommitInfo info = readCommit(current);
            cout << "commit " << current << endl;
            cout << "Author: " << info.author << endl;
            cout << "Date:   " << info.timestamp << endl;
            cout << endl;
            cout << "    " << info.message << endl;
            cout << endl;
            current = info.parent;
        }
    } catch (const exception& e) {
        cerr << "Error reading log: " << e.what() << endl;
    }
}

void handleCheckout(const string& target) {
    if (!isValidHash(target)) {
        cerr << "Fatal: Not a valid commit hash " << target << endl;
        return;
    }
    try {
        // Verify the target is a commit
        ParsedObject obj = readObject(target);
        if (obj.type != "commit") {
            cerr << "Fatal: Object " << target << " is not a commit" << endl;
            return;
        }
        CommitInfo info = readCommit(target);
        vector<TreeEntry> newEntries = readTree(info.tree);

        // Collect old tree paths to detect files that need to be removed
        set<string> newPaths;
        for (const auto& e : newEntries) newPaths.insert(e.path);

        set<string> oldPaths;
        string oldHead = getHeadCommit();
        if (!oldHead.empty()) {
            try {
                CommitInfo oldInfo = readCommit(oldHead);
                vector<TreeEntry> oldEntries = readTree(oldInfo.tree);
                for (const auto& e : oldEntries) oldPaths.insert(e.path);
            } catch (...) {
                // old HEAD is broken; ignore cleanup
            }
        }

        // Remove files that were tracked before but are not in the new tree
        for (const auto& p : oldPaths) {
            if (newPaths.count(p)) continue;
            fs::path fp = toNative(p);
            if (fs::exists(fp) && fs::is_regular_file(fp)) {
                fs::remove(fp);
            }
        }

        // Write the new tree's files
        for (const auto& e : newEntries) {
            ParsedObject blob = readObject(e.hash);
            if (blob.type != "blob") {
                cerr << "Warning: tree entry " << e.path << " is not a blob" << endl;
                continue;
            }
            fs::path fp = toNative(e.path);
            if (fp.has_parent_path()) {
                fs::create_directories(fp.parent_path());
            }
            ofstream out(fp, ios::binary | ios::trunc);
            out << blob.content;
            out.close();
        }

        // Update index to match the new tree
        writeIndex(newEntries);

        // Update HEAD to point at the new commit
        setHeadCommit(target);

        cout << "Checked out " << target << endl;
    } catch (const exception& e) {
        cerr << "Error during checkout: " << e.what() << endl;
    }
}
