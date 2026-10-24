#include <mermaid/cli.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

#include <mermaid/error.h>
#include <mermaid/mermaid.h>
#include <mermaid/render.h>

namespace mermaid::cli {

namespace {

// One command line option: its spellings and whether it takes a value.
struct option_spec {
    const char* short_name;  // "-i"
    const char* long_name;   // "--input"
    bool takes_value;
    const char* value_name;
    const char* help;
};

const option_spec k_options[] = {
    {"-i", "--input", true, "<file>", "Input mermaid file, `-` reads stdin"},
    {"-o", "--output", true, "<file>", "Output file, `-` writes stdout. Default: <input>.svg (out.svg for stdin)"},
    {"-t", "--theme", true, "<theme>", "Theme of the chart: default, dark, forest, neutral, base"},
    {"-b", "--backgroundColor", true, "<color>", "Background of the SVG: a CSS colour or `transparent`. Default: white"},
    {"-I", "--svgId", true, "<id>", "The id attribute of the SVG element. Default: my-svg"},
    {"-C", "--cssFile", true, "<file>", "CSS file appended to the diagram's styles"},
    {"-e", "--outputFormat", true, "<format>", "Output format; only svg is supported"},
    {"-q", "--quiet", false, "", "Suppress the status message"},
    {"-V", "--version", false, "", "Print the version"},
    {"-h", "--help", false, "", "Print this help"},
};

const option_spec* find_option(const std::string& name) {
    for (const option_spec& spec : k_options)
        if (name == spec.short_name || name == spec.long_name) return &spec;
    return nullptr;
}

bool known_theme(const std::string& theme) {
    return theme == "default" || theme == "dark" || theme == "forest" || theme == "neutral";
}

void apply(options& o, const option_spec& spec, const std::string& value) {
    const std::string name = spec.long_name;
    if (name == "--input") o.input = value;
    else if (name == "--output") o.output = value;
    else if (name == "--theme") o.theme = value;
    else if (name == "--backgroundColor") o.background = value;
    else if (name == "--svgId") o.svg_id = value;
    else if (name == "--cssFile") o.css_file = value;
    else if (name == "--outputFormat") o.output_format = value;
    else if (name == "--quiet") o.quiet = true;
    else if (name == "--version") o.version = true;
    else if (name == "--help") o.help = true;
}

std::string read_stream(std::istream& s) {
    return std::string(std::istreambuf_iterator<char>(s), std::istreambuf_iterator<char>());
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read '" + path + "'");
    return read_stream(f);
}

}  // namespace

options parse_arguments(const std::vector<std::string>& args) {
    options o;
    for (size_t i = 0; i < args.size(); ++i) {
        std::string name = args[i];
        std::string value;
        bool has_inline_value = false;
        if (name.rfind("--", 0) == 0) {
            const size_t eq = name.find('=');
            if (eq != std::string::npos) {
                value = name.substr(eq + 1);
                name = name.substr(0, eq);
                has_inline_value = true;
            }
        }
        if (name.size() < 2 || name[0] != '-') throw usage_error("unexpected argument '" + args[i] + "'");
        const option_spec* spec = find_option(name);
        if (!spec) throw usage_error("unknown option '" + name + "'");

        if (spec->takes_value && !has_inline_value) {
            if (i + 1 >= args.size()) throw usage_error("option '" + name + "' requires a value");
            value = args[++i];
        } else if (!spec->takes_value && has_inline_value) {
            throw usage_error("option '" + name + "' does not take a value");
        }
        apply(o, *spec, value);
    }

    if (o.help || o.version) return o;
    if (o.input.empty()) throw usage_error("an input is required: -i <file>, or -i - for stdin");
    if (!known_theme(o.theme)) throw usage_error("unknown theme '" + o.theme + "' (default, dark, forest, neutral)");
    if (o.output_format != "svg") throw usage_error("unsupported output format '" + o.output_format + "': only svg is supported");
    return o;
}

std::string usage() {
    std::ostringstream s;
    s << "Usage: mermaid_cli -i <file> [options]\n\nOptions:\n";
    for (const option_spec& spec : k_options) {
        std::string left = std::string(spec.short_name) + ", " + spec.long_name;
        if (spec.takes_value) left += std::string(" ") + spec.value_name;
        s << "  " << left << std::string(left.size() < 36 ? 36 - left.size() : 1, ' ') << spec.help << "\n";
    }
    return s.str();
}

int run(const std::vector<std::string>& args, std::istream& in, std::ostream& out, std::ostream& err) {
    options o;
    try {
        o = parse_arguments(args);
    } catch (const usage_error& e) {
        err << "error: " << e.what() << "\n\n" << usage();
        return 1;
    }
    if (o.help) {
        out << usage();
        return 0;
    }
    if (o.version) {
        out << MERMAID_VERSION_STRING << "\n";
        return 0;
    }

    try {
        const std::string source = o.input == "-" ? read_stream(in) : read_file(o.input);

        RenderOptions render;
        render.theme = o.theme;
        render.background = o.background;
        render.svg_id = o.svg_id;
        if (!o.css_file.empty()) render.css = read_file(o.css_file);
        const std::string svg = render_svg(source, render);

        if (o.output == "-") {
            out << svg;
            return 0;
        }
        const std::string path = !o.output.empty() ? o.output : (o.input == "-" ? "out.svg" : o.input + ".svg");
        std::ofstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("cannot write '" + path + "'");
        f << svg;
        f.close();
        if (!f) throw std::runtime_error("cannot write '" + path + "'");
        if (!o.quiet) err << "Generated " << path << "\n";
        return 0;
    } catch (const ParseError& e) {
        err << "error: " << e.what();
        if (e.line() > 0) err << " (line " << e.line() << ")";
        err << "\n";
    } catch (const std::exception& e) {
        err << "error: " << e.what() << "\n";
    }
    return 1;
}

}  // namespace mermaid::cli
