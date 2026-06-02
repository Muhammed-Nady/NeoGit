#include "utils.h"
#include <openssl/sha.h>
#include <zlib.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

string calculateSHA1(const string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash);
    stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

string compressContent(const string& data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        throw(runtime_error("deflateInit failed while compressing."));
    }
    zs.next_in = (Bytef*)data.data();
    zs.avail_in = data.size();
    int ret;
    char outbuffer[32768];
    string outstring;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = deflate(&zs, Z_FINISH);
        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    deflateEnd(&zs);
    if (ret != Z_STREAM_END) {
        throw(runtime_error("Exception during zlib compression."));
    }
    return outstring;
}

string decompressContent(const string& compressedData) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK) {
        throw(runtime_error("inflateInit failed while decompressing."));
    }
    zs.next_in = (Bytef*)compressedData.data();
    zs.avail_in = compressedData.size();
    int ret;
    char outbuffer[32768];
    string outstring;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = inflate(&zs, Z_NO_FLUSH);
        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    inflateEnd(&zs);
    if (ret != Z_STREAM_END) {
        throw(runtime_error("Exception during zlib decompression."));
    }
    return outstring;
}

string storeObject(const string& type, const string& content) {
    string header = type + " " + to_string(content.size()) + '\0';
    string fullObject;
    fullObject.reserve(header.size() + content.size());
    fullObject.append(header);
    fullObject.append(content);

    string hash = calculateSHA1(fullObject);

    string dirName = hash.substr(0, 2);
    string fileName = hash.substr(2);
    fs::path objectDir = fs::path(".neogit") / "objects" / dirName;
    fs::create_directories(objectDir);

    string compressed = compressContent(fullObject);
    ofstream outFile(objectDir / fileName, ios::binary);
    outFile << compressed;
    outFile.close();

    return hash;
}

ParsedObject readObject(const string& hash) {
    if (hash.length() != 40) {
        throw runtime_error("Not a valid object name " + hash);
    }

    string dirName = hash.substr(0, 2);
    string fileName = hash.substr(2);
    fs::path objectPath = fs::path(".neogit") / "objects" / dirName / fileName;

    if (!fs::exists(objectPath)) {
        throw runtime_error("Not a valid object name " + hash);
    }

    ifstream file(objectPath, ios::binary);
    stringstream buffer;
    buffer << file.rdbuf();
    string compressedContent = buffer.str();
    file.close();

    string fullObject = decompressContent(compressedContent);

    size_t nullPos = fullObject.find('\0');
    if (nullPos == string::npos) {
        throw runtime_error("Corrupt object: missing header terminator");
    }

    string header = fullObject.substr(0, nullPos);
    string content = fullObject.substr(nullPos + 1);

    size_t spacePos = header.find(' ');
    if (spacePos == string::npos) {
        throw runtime_error("Corrupt object header");
    }

    string type = header.substr(0, spacePos);
    return {type, content};
}
