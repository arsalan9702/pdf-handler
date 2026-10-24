#pragma once

#include <string>
#include <vector>

// Arguments after the program name, as UTF-8 on every platform (wide command line on Windows).
std::vector<std::string> utf8Args(int argc, char** argv);

// Runs one pdfh command (args exclude the program name). Returns the process exit code.
int runCli(const std::vector<std::string>& args);
