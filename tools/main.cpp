#include "aiws_send.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <unistd.h>

//gets the default socket path
std::string get_default_socket_path()
{
    const char* user = std::getenv("USER");
    std::string user_str;
    
    if (user != nullptr)
    {
        user_str = user;
    }
    else
    {
        user_str = std::to_string(getuid());
    }

    return "/tmp/aiws-" + user_str + ".sock";
}

//prints usage
void print_usage()
{
    std::cerr << "Usage: aiws_send [--socket PATH] add FILE\n";
}

int main(int argc, char* argv[])
{
    std::string socket_path = get_default_socket_path();
    std::string command;
    std::string file_path;
    bool has_add = false;

    //parse arguments manually
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        
        if (arg == "--socket")
        {
            if (i + 1 < argc)
            {
                socket_path = argv[++i];
            }
            else
            {
                std::cerr << "Missing path for --socket\n";
                return 2;
            }
        }
        else if (arg == "add")
        {
            has_add = true;
            if (i + 1 < argc)
            {
                file_path = argv[++i];
            }
            else
            {
                std::cerr << "Missing FILE for add\n";
                return 1;
            }
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    if (!has_add || file_path.empty())
    {
        print_usage();
        return 2;
    }

    //we construct the command string as "add <file_path>"
    command = "add " + file_path;
    return aiws::tools::send_command(socket_path, command);
}
