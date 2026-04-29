#include "utils.h"
#include <openssl/sha.h>
#include <zlib.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cstring>

using namespace std;

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