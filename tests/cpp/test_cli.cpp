#include <filesystem>
#include <fstream>
#include <sstream>

#include <mermaid/cli.h>
#include <mermaid/mermaid.h>
#include "utest.h"

using namespace mermaid;
namespace fs = std::filesystem;

namespace {

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

struct run_result {
    int code = 0;
    std::string out, err;
};

run_result run_cli(const std::vector<std::string>& args, const std::string& stdin_text = "") {
    std::istringstream in(stdin_text);
    std::ostringstream out, err;
    run_result r;
    r.code = cli::run(args, in, out, err);
    r.out = out.str();
    r.err = err.str();
    return r;
}

std::string read_all(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

// A scratch directory that goes away with the test.
struct scratch {
    fs::path dir;
    scratch() {
        dir = fs::temp_directory_path() / ("mermaid_cli_test_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        fs::create_directories(dir);
    }
    ~scratch() {
        std::error_code ignored;
        fs::remove_all(dir, ignored);
    }
    std::string write(const std::string& name, const std::string& text) const {
        std::ofstream(dir / name, std::ios::binary) << text;
        return (dir / name).string();
    }
    std::string path(const std::string& name) const { return (dir / name).string(); }
};

const char* kFlow = "flowchart TD\nA --> B\n";

}  // namespace

UTEST(cli_arguments, defaults) {
    const cli::options o = cli::parse_arguments({"-i", "a.mmd"});
    ASSERT_STREQ("a.mmd", o.input.c_str());
    ASSERT_TRUE(o.output.empty());
    ASSERT_STREQ("default", o.theme.c_str());
    ASSERT_STREQ("white", o.background.c_str());
    ASSERT_STREQ("my-svg", o.svg_id.c_str());
    ASSERT_FALSE(o.quiet);
}

UTEST(cli_arguments, short_long_and_equals_forms) {
    const cli::options a = cli::parse_arguments(
        {"-i", "a.mmd", "-o", "b.svg", "-t", "dark", "-b", "transparent", "-I", "d1", "-C", "x.css", "-q"});
    ASSERT_STREQ("b.svg", a.output.c_str());
    ASSERT_STREQ("dark", a.theme.c_str());
    ASSERT_STREQ("transparent", a.background.c_str());
    ASSERT_STREQ("d1", a.svg_id.c_str());
    ASSERT_STREQ("x.css", a.css_file.c_str());
    ASSERT_TRUE(a.quiet);

    const cli::options b = cli::parse_arguments(
        {"--input", "a.mmd", "--output=b.svg", "--theme", "forest", "--backgroundColor=red", "--svgId", "d2", "--cssFile", "y.css", "--quiet"});
    ASSERT_STREQ("a.mmd", b.input.c_str());
    ASSERT_STREQ("b.svg", b.output.c_str());
    ASSERT_STREQ("forest", b.theme.c_str());
    ASSERT_STREQ("red", b.background.c_str());
    ASSERT_STREQ("d2", b.svg_id.c_str());
    ASSERT_STREQ("y.css", b.css_file.c_str());
    ASSERT_TRUE(b.quiet);
}

UTEST(cli_arguments, help_and_version_need_no_input) {
    ASSERT_TRUE(cli::parse_arguments({"-h"}).help);
    ASSERT_TRUE(cli::parse_arguments({"--help"}).help);
    ASSERT_TRUE(cli::parse_arguments({"-V"}).version);
    ASSERT_TRUE(cli::parse_arguments({"--version"}).version);
}

UTEST(cli_arguments, only_svg_output_is_accepted) {
    ASSERT_STREQ("svg", cli::parse_arguments({"-i", "a.mmd", "-e", "svg"}).output_format.c_str());
    bool threw = false;
    try {
        cli::parse_arguments({"-i", "a.mmd", "-e", "png"});
    } catch (const cli::usage_error& e) {
        threw = has(e.what(), "svg");
    }
    ASSERT_TRUE(threw);
}

UTEST(cli_arguments, bad_usage_is_reported) {
    auto fails = [](std::vector<std::string> args, const char* mentions) {
        try {
            cli::parse_arguments(args);
        } catch (const cli::usage_error& e) {
            return has(e.what(), mentions);
        }
        return false;
    };
    ASSERT_TRUE(fails({"-i", "a.mmd", "--bogus"}, "--bogus"));
    ASSERT_TRUE(fails({"-i"}, "-i"));                                // missing value
    ASSERT_TRUE(fails({"-i", "a.mmd", "-t", "neon"}, "neon"));       // unknown theme
    ASSERT_TRUE(fails({"-t", "dark"}, "input"));                     // no input at all
    ASSERT_TRUE(fails({"-i", "a.mmd", "extra"}, "extra"));           // stray argument
}

UTEST(cli_run, stdin_to_stdout) {
    const run_result r = run_cli({"-i", "-", "-o", "-"}, kFlow);
    ASSERT_EQ(0, r.code);
    ASSERT_TRUE(r.out.rfind("<svg", 0) == 0);
    ASSERT_TRUE(has(r.out, "id=\"my-svg\""));
    ASSERT_TRUE(r.err.empty());
}

UTEST(cli_run, file_to_file_with_options) {
    const scratch s;
    const std::string in = s.write("d.mmd", kFlow);
    const std::string css = s.write("x.css", ".extra{fill:red}");
    const run_result r = run_cli({"-i", in, "-o", s.path("o.svg"), "-I", "chart", "-b", "transparent", "-C", css});
    ASSERT_EQ(0, r.code);
    const std::string svg = read_all(s.path("o.svg"));
    ASSERT_TRUE(has(svg, "id=\"chart\""));
    ASSERT_TRUE(has(svg, "background-color: transparent"));
    ASSERT_TRUE(has(svg, ".extra{fill:red;}"));
}

UTEST(cli_run, default_output_is_the_input_name_plus_svg) {
    const scratch s;
    const std::string in = s.write("d.mmd", kFlow);
    ASSERT_EQ(0, run_cli({"-i", in}).code);
    ASSERT_TRUE(fs::exists(in + ".svg"));
}

UTEST(cli_run, theme_changes_the_output) {
    const run_result light = run_cli({"-i", "-", "-o", "-"}, kFlow);
    const run_result dark = run_cli({"-i", "-", "-o", "-", "-t", "dark"}, kFlow);
    ASSERT_EQ(0, dark.code);
    ASSERT_TRUE(light.out != dark.out);
}

UTEST(cli_run, errors_give_exit_code_1_and_a_message) {
    const run_result syntax = run_cli({"-i", "-", "-o", "-"}, "flowchart TD\nA --> \n");
    ASSERT_EQ(1, syntax.code);
    ASSERT_TRUE(syntax.out.empty());
    ASSERT_TRUE(has(syntax.err, "line"));

    const run_result missing = run_cli({"-i", "/definitely/not/here.mmd", "-o", "-"});
    ASSERT_EQ(1, missing.code);
    ASSERT_TRUE(has(missing.err, "here.mmd"));

    const run_result unsupported = run_cli({"-i", "-", "-o", "-"}, "sequenceDiagram\nA->>B: hi\n");
    ASSERT_EQ(1, unsupported.code);
    ASSERT_FALSE(unsupported.err.empty());

    const run_result usage = run_cli({"--bogus"});
    ASSERT_EQ(1, usage.code);
    ASSERT_TRUE(has(usage.err, "--bogus"));
    ASSERT_TRUE(has(usage.err, "Usage"));  // the usage text follows the complaint
}

UTEST(cli_run, help_and_version_print_to_stdout) {
    const run_result help = run_cli({"--help"});
    ASSERT_EQ(0, help.code);
    ASSERT_TRUE(has(help.out, "Usage"));
    ASSERT_TRUE(has(help.out, "--theme"));
    const run_result version = run_cli({"-V"});
    ASSERT_EQ(0, version.code);
    ASSERT_TRUE(has(version.out, MERMAID_VERSION_STRING));
}
