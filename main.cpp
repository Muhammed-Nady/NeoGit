#include <iostream>
#include <string>
#include "repository.h"

using namespace std;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "Usage: neogit <command> [<args>]" << endl;
        return 1;
    }

    string command = argv[1];

    if (command == "init")
    {
        handleInit();
    }
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
    else if (command == "commit")
    {
        if (argc < 3)
        {
            cout << "Usage: neogit commit <message>" << endl;
            return 1;
        }
        handleCommit(argv[2]);
    }
    else
    {
        cout << "Unknown command: " << command << endl;
    }

    return 0;
}