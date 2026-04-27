#include <bits/stdc++.h>
#include <openssl/sha.h>
#include <zlib.h>

using namespace std;
using namespace filesystem;

string createBlobFromFile(const string &filename)
{
    ifstream file(filename, ios::binary);
    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();

    string sha1_hash = calculateSHA1(content);

    string dirName = sha1_hash.substr(0, 2);
    string fileName = sha1_hash.substr(2);
    path objectDir = path(".neogit") / "objects" / dirName;
    create_directories(objectDir);

    string compressed = compressContent(content);
    ofstream outFile(objectDir / fileName, ios::binary);
    outFile << compressed;
    outFile.close();

    return sha1_hash;
}

string decompressContent(const string &compressedData)
{
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK)
    {
        throw(runtime_error("inflateInit failed while decompressing."));
    }

    zs.next_in = (Bytef *)compressedData.data();
    zs.avail_in = compressedData.size();

    int ret;
    char outbuffer[32768];
    string outstring;

    do
    {
        zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, Z_NO_FLUSH);

        if (outstring.size() < zs.total_out)
        {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END)
    {
        throw(runtime_error("Exception during zlib decompression."));
    }

    return outstring;
}

string compressContent(const string &data)
{
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK)
    {
        throw(runtime_error("deflateInit failed while compressing."));
    }

    zs.next_in = (Bytef *)data.data();
    zs.avail_in = data.size();

    int ret;
    char outbuffer[32768];
    string outstring;

    do
    {
        zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = deflate(&zs, Z_FINISH);

        if (outstring.size() < zs.total_out)
        {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END)
    {
        throw(runtime_error("Exception during zlib compression."));
    }

    return outstring;
}

void handleInit()
{
    path repo_path = ".neogit";

    if (exists(repo_path))
    {
        cout << "Repository already initialized." << endl;
        return;
    }

    try
    {
        create_directory(repo_path);
        create_directory(repo_path / "objects");
        create_directory(repo_path / "refs");

        cout << "Initialized empty neogit repository in " << absolute(repo_path) << endl;
    }
    catch (const filesystem_error &e)
    {
        cerr << "Error: " << e.what() << endl;
    }
}

string calculateSHA1(const string &content)
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(content.c_str()), content.size(), hash);
    stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

void handleCatFile(const string &hash)
{
    if (hash.length() != 40)
    {
        cerr << "Fatal: Not a valid object name " << hash << '\n';
        return;
    }

    string dirName = hash.substr(0, 2);
    string fileName = hash.substr(2);

    path objectPath = path(".neogit") / "objects" / dirName / fileName;

    if (!exists(objectPath))
    {
        cerr << "Fatal: Not a valid object name " << hash << endl;
        return;
    }

    ifstream file(objectPath, ios::binary);
    stringstream buffer;
    buffer << file.rdbuf();
    string compressedContent = buffer.str();

    try
    {
        string originalContent = decompressContent(compressedContent);
        cout << originalContent;
    }
    catch (const exception &e)
    {
        cerr << "Error reading object: " << e.what() << '\n';
    }
}

void handleHashObject(const string &filename, bool writeToStore = false)
{
    if (!exists(filename))
    {
        cout << "Fatal: Cannot open '" << filename << "': No such file" << '\n';
        return;
    }

    ifstream file(filename, ios::binary);

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();

    string sha1_hash = calculateSHA1(content);
    cout << sha1_hash << '\n';

    if (writeToStore)
    {
        string dirName = sha1_hash.substr(0, 2);
        string fileName = sha1_hash.substr(2);

        path objectDir = path(".neogit") / "objects" / dirName;
        create_directories(objectDir);

        string compressed = compressContent(content);

        ofstream outFile(objectDir / fileName, ios::binary);
        outFile << compressed;
        outFile.close();

        cout << "Object stored successfully." << '\n';
    }
}

void handleAdd(const string &filename)
{
    if (!exists(filename))
    {
        cerr << "Fatal: pathspec '" << filename << "' did not match any files" << endl;
        return;
    }
    string hash = createBlobFromFile(filename);
    path indexPath = path(".neogit") / "index";

    ofstream indexFile(indexPath, ios::app);
    indexFile << "100644 " << hash << " " << filename << '\n';
    indexFile.close();

    cout << "Added '" << filename << "' to index." << '\n';
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "Usage: neogit <command>" << endl;
        return 1;
    }

    string command = argv[1];

    if (command == "init")
        handleInit();
    else if (command == "hash-object")
    {
        if (argc < 3)
        {
            cout << "Usage: neogit hash-object <file>" << endl;
            return 1;
        }
        handleHashObject(argv[2], true);
    }
    else if (command == "cat-file")
    {
        if (argc < 3)
        {
            cout << "Usage: neogit cat-file <hash>" << endl;
            return 1;
        }
        handleCatFile(argv[2]);
    }
    else if (command == "add")
    {
        if (argc < 3)
        {
            cout << "Usage: neogit add <file>" << endl;
            return 1;
        }
        handleAdd(argv[2]);
    }
    else
    {
        cout << "Unknown command: " << command << endl;
    }

    return 0;
}