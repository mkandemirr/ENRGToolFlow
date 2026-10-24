#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <sys/wait.h>

const std::string RED    = "\033[1;31m";
const std::string GREEN  = "\033[1;32m";
const std::string YELLOW = "\033[1;33m";
const std::string RESET  = "\033[0m";

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Usage: ./runPipeline ./toolChains/SimpleReco.chain"
            << std::endl;

        return 1;
    }

    std::string chainFile = argv[1];

    std::ifstream infile(chainFile);

    if (!infile.is_open())
    {
        std::cerr << "Error: cannot open chain file: " << chainFile << std::endl;

        return 1;
    }

    std::string command;

    while (std::getline(infile, command))
    {
        if (command.empty()) continue;
        if (command[0] == '#') continue;

        std::cout
    << "\n======================================" << std::endl
    << RED << "Running:" << RESET << std::endl
    << YELLOW << command << RESET << std::endl
    << "======================================" << std::endl;

        // std::system() returns a wait status, not an exit code (exit 1 -> 256).
        // Returning it from main() unchanged would be truncated to 0 by the shell.
        int status = std::system(command.c_str());
        int ret = 1;

        if (status != -1 && WIFEXITED(status))
        {
            ret = WEXITSTATUS(status);
        }
        else if (status != -1 && WIFSIGNALED(status))
        {
            ret = 128 + WTERMSIG(status);
        }

        if (ret != 0)
        {
            std::cerr
                << "\n======================================"
                << "\n" << RED << "PIPELINE FAILED" << RESET
                << "\nFailed command:"
                << "\n" << command
                << "\nReturn code: " << ret
                << "\n======================================"
                << std::endl;

            return ret;
        }
    }

    std::cout
        << "\n======================================"
        << "\n" << GREEN << "PIPELINE FINISHED SUCCESSFULLY" << RESET
        << "\n======================================"
        << std::endl;

    return 0;
}
