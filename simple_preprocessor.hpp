/******************************************************************************
 *  Simple C-like preprocessor/parser
 *
 *  Supports macro replacement and conditionals with arithmetic expression
 *  evaluation.
 *
 *  Features:
 *  - String to string or string to int macros
 *  - Simple if, elif, else, endif conditional directives. Can be nested.
 *  - Arithmetic parser for conditionals. Evaluated after macro replacement.
 *  - Will output a vector of strings. by default, everything gets appended into
 *    the first string (index 0). the #output directive along with a number can
 *    be used to change the index.
 *
 *  By default, when an unknown # directive is encountered, it throws an error
 *  and stops parsing. To bypass this and actually append the directives, use
 *  #define PARSER_IGNORE_UNKNOWN_DIRECTIVE
 *
 *  Unsupported:
 *  - #ifdef or #if defined() statements. All macros need to have a value, so
 *    just plain #if is enough if the macro value is non-zero.
 *  - #define directive and file-scope macros (planned)
 *  - Recursive macro replacement (planned)
 *  - Per-file macros and the #define directive (planned)
 *
 ******************************************************************************
 *  This software is available as a choice of the following licenses. Choose
 *  whichever one you prefer.
 *
 *  Alternative 1 - Public Domain
 *  This is free and unencumbered software released into the public domain.
 *  For a copy, see <www.unlicense.org>
 * 
 *  Alternative 2 - MIT license.  
 *  Copyright (c) 2024 Constantitus
 *  For a copy, see <https://opensource.org/licenses/MIT>.
 ******************************************************************************/

#pragma once

#define PARSER_IGNORE_UNKNOWN_DIRECTIVE

#include <initializer_list>
#include <string>
#include <vector>
#include <variant>


class SimplePreprocessor {
public:
    SimplePreprocessor() {}
    SimplePreprocessor(std::initializer_list<std::pair<std::string, std::variant<std::string, int>>> defines) :
        global_defines(defines) {}
    ~SimplePreprocessor() {}

    void Define(std::string key, std::string value) {
        global_defines.push_back({key, value});
    }
    void Define(std::string key, int value = 1) {
        global_defines.push_back({key, value});
    }

    std::vector<std::string> Parse(std::string const& input_buffer);
    std::vector<std::string> Parse(const char *input_buffer, size_t buflen);

private:
    std::vector<std::pair<std::string, std::variant<std::string, int>>> global_defines;
};

