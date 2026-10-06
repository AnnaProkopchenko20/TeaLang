#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "lexer.h"
#include "parser.h"

namespace
{
    using TokenLines = std::vector<std::vector<Token>>;

    struct Options
    {
        bool print_ast = false;
        bool print_tokens = false;
        std::string input_path;
    };

    // ---- command line ----

    bool parse_args(int argc, char* argv[], Options& opts)
    {
        std::vector<std::string> files;

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "-ast" || arg == "--ast") {
                opts.print_ast = true;
            }
            else if (arg == "-tokens" || arg == "--tokens") {
                opts.print_tokens = true;
            }
            else {
                files.push_back(arg);
            }
        }

        if (files.empty())
        {
            std::cerr << "Expected at least an input file\n";
            return false;
        }

        opts.input_path = files[0];
        return true;
    }

    // ---- input ----

    bool read_source(const std::string& path, std::string& source)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cerr << "Failed to open input file.\n";
            return false;
        }

        source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return true;
    }

    // ---- compilation stages ----

    void print_tokens(const TokenLines& lines)
    {
        for (const auto& line : lines)
        {
            for (const auto& t : line)
            {
                std::cout << t.kind << "('" << t.text << "') ";
            }
            std::cout << "\n";
        }
    }

    std::unique_ptr<ProgramNode> parse(const TokenLines& lines)
    {
        Parser parser(lines);
        return parser.parse_program();
    }

    // ---- driver ----

    int run(const Options& opts, const std::string& source)
    {
        TokenLines lines = lex(source);

        if (opts.print_tokens)
        {
            print_tokens(lines);
            if (!opts.print_ast) { return 0; }
        }

        auto program = parse(lines);

        // Stage 1 output requirement
        if (opts.print_ast)
        {
            program->dump(0);
        }

        return 0;
    }
}

int main(int argc, char* argv[])
{
    Options opts;
    if (!parse_args(argc, argv, opts))
    {
        return 1;
    }

    std::string source;
    if (!read_source(opts.input_path, source))
    {
        return 1;
    }

    try
    {
        return run(opts, source);
    }
    catch (const std::exception& e)
    {
        std::cerr << "compilation error: " << e.what() << "\n";
        return 1;
    }
}