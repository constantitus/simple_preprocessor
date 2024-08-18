/******************************************************************************
 *  Simple C-like preprocessor/parser
 *
 *  Supports macro replacement and conditionals with arithmetic expression
 *  evaluation.
 *
 *  By default, when an unknown # directive is encountered, it throws an error
 *  and stops parsing. To bypass this and actually append the directives, use
 *  #define PARSER_IGNORE_UNKNOWN_DIRECTIVE
 *
 *  Unsupported:
 *  - #ifdef or #if defined() statements. All macros need to have a value, so
 *    just plain #if is enough if the macro value is non-zero. (might implement
 *    this in the future)
 *  - String to string macros
 *  - Per-file macros and the #define directive (planned)
 *
 *  TODO: Support splitting the output into multiple output strings via a #
 *  directive. This is pretty much the reason I built this parser, along with
 *  the conditionals.
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

class SimplePreprocessor {
public:
    SimplePreprocessor(std::initializer_list<std::pair<std::string, int>> defines) :
        global_defines(defines) {}
    ~SimplePreprocessor() {}

    void Define(std::string key, int value = 1) {
        global_defines.push_back({key, value});
    }

    std::string Parse(std::string const& input_buffer);
    std::string Parse(const char *input_buffer, size_t buflen);

private:
    std::vector<std::pair<std::string, int>> global_defines;
};

