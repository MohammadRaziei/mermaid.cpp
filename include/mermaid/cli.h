// cli.h - the `mermaid_cli` command line, modelled on mermaid-js/mermaid-cli (SVG output only).
//
// The logic lives here so it can be tested without spawning a process; cli/main.cpp only forwards
// `argv`. No command line library is used.
#pragma once

#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

namespace mermaid::cli {

struct options {
    std::string input;                // file, or `-` for stdin
    std::string output;               // file, or `-` for stdout; empty = `<input>.svg` (`out.svg` for stdin)
    std::string theme = "default";    // default | dark | forest | neutral
    std::string background = "white"; // CSS colour or `transparent`
    std::string svg_id = "my-svg";
    std::string css_file;             // CSS appended to the diagram's <style>
    std::string output_format = "svg";
    bool quiet = false;
    bool help = false;
    bool version = false;
};

// A wrong command line (unknown option, missing value, unsupported format, ...).
class usage_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// `args` excludes the program name. Throws usage_error.
options parse_arguments(const std::vector<std::string>& args);

std::string usage();

// Runs the tool; returns the process exit code (0 ok, 1 error). Diagnostics go to `err`.
int run(const std::vector<std::string>& args, std::istream& in, std::ostream& out, std::ostream& err);

}  // namespace mermaid::cli
