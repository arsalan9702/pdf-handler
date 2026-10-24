#include "cli/Cli.hpp"

int main(int argc, char** argv) {
    return runCli(utf8Args(argc, argv));
}
