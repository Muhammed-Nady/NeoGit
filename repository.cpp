#include "repository.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstdlib>

using namespace std;
namespace fs = std::filesystem;

// --- Helper Functions (Internal) ---
string createBlobFromFile(const string& filename) {
    ifstream file(filename, ios::binary);
    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();

    string sha1_hash = calculateSHA1(content);
    
    string dirName = sha1_hash.substr(0, 2);
    string fileName = sha1_hash.substr(2);
    fs::path objectDir = fs::path(".neogit") / "objects" / dirName;
    fs::create_directories(objectDir);

    string compressed = compressContent(content);
    ofstream outFile(objectDir / fileName, ios::binary);
    outFile << compressed;
    outFile.close();

    return sha1_hash;
}

string writeTree() {
    fs::path indexPath = fs::path(".neogit") / "index";
    if (!fs::exists(indexPath)) {
        throw runtime_error("No index file found. Add some files first.");
    }

    ifstream indexFile(indexPath);
    stringstream buffer;
    buffer << indexFile.rdbuf();
    string indexContent = buffer.str();
    indexFile.close();

    string treeHash = calculateSHA1(indexContent);

    string dirName = treeHash.substr(0, 2);
    string fileName = treeHash.substr(2);
    fs::path objectDir = fs::path(".neogit") / "objects" / dirName;
    fs::create_directories(objectDir);

    string compressed = compressContent(indexContent);
    ofstream outFile(objectDir / fileName, ios::binary);
    outFile << compressed;
    outFile.close();

    return treeHash;
}

// --- Core Commands ---
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
        string sha1_hash = calculateSHA1(content);
        cout << sha1_hash << endl;

        if (writeToStore) {
            string dirName = sha1_hash.substr(0, 2);
            string fileName = sha1_hash.substr(2);
            fs::path objectDir = fs::path(".neogit") / "objects" / dirName;
            fs::create_directories(objectDir);

            string compressed = compressContent(content);
            ofstream outFile(objectDir / fileName, ios::binary);
            outFile << compressed;
            outFile.close();
            cout << "Object stored successfully." << endl;
        }
    } catch (const exception& e) {
        cerr << "Error hashing object: " << e.what() << endl;
    }
}

void handleCatFile(const string& hash) {
    if (hash.length() != 40) {
        cerr << "Fatal: Not a valid object name " << hash << endl;
        return;
    }
    string dirName = hash.substr(0, 2);
    string fileName = hash.substr(2);
    fs::path objectPath = fs::path(".neogit") / "objects" / dirName / fileName;

    if (!fs::exists(objectPath)) {
        cerr << "Fatal: Not a valid object name " << hash << endl;
        return;
    }

    ifstream file(objectPath, ios::binary);
    stringstream buffer;
    buffer << file.rdbuf();
    string compressedContent = buffer.str();

    try {
        string originalContent = decompressContent(compressedContent);
        cout << originalContent; 
    } catch (const exception& e) {
        cerr << "Error reading object: " << e.what() << endl;
    }
}

void handleAdd(const string& filename) {
    if (!fs::exists(filename)) {
        cerr << "Fatal: pathspec '" << filename << "' did not match any files" << endl;
        return;
    }
    string hash = createBlobFromFile(filename);
    fs::path indexPath = fs::path(".neogit") / "index";
    ofstream indexFile(indexPath, ios::app); 
    indexFile << "100644 " << hash << " " << filename << "\n";
    indexFile.close();
    cout << "Added '" << filename << "' to index." << endl;
}

void handleCommit(const string& message) {
    try {
        string treeHash = writeTree();
        stringstream commitData;
        commitData << "tree " << treeHash << "\n";
        
        const char* userEnv = getenv("USER"); 
        if (userEnv == nullptr) {
            userEnv = getenv("USERNAME"); 
        }
        string authorName = userEnv ? string(userEnv) : "Unknown User";

        commitData << "author " << authorName << "\n";
        commitData << "\n" << message << "\n";

        string content = commitData.str();
        string commitHash = calculateSHA1(content);
        
        string dirName = commitHash.substr(0, 2);
        string fileName = commitHash.substr(2);
        fs::path objectDir = fs::path(".neogit") / "objects" / dirName;
        fs::create_directories(objectDir);

        string compressed = compressContent(content);
        ofstream outFile(objectDir / fileName, ios::binary);
        outFile << compressed;
        outFile.close();

        cout << "[main " << commitHash.substr(0, 7) << "] " << message << endl;

    } catch (const exception& e) {
        cerr << "Error during commit: " << e.what() << endl;
    }
}