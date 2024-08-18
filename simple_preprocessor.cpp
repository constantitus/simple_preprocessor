/******************************************************************************
 *  License:
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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>
#include <string_view>
#if defined(__has_include)
#   if __has_include(<alloca.h>)
#       include <alloca.h>
#   endif
#endif

#include "arithmetic_parser.hpp"
#include "simple_preprocessor.hpp"

#ifndef PARSER_NAME
#   define PARSER_NAME "Preprocessor"
#endif

#define PARSER_PRINTF(msg, ...) printf(msg, ##__VA_ARGS__)
#define PARSER_LOG(msg, ...) PARSER_PRINTF(PARSER_NAME": " msg "\n", ##__VA_ARGS__)
#define PARSER_ASSERT(condition) assert(condition);

// Prints the line number along with the message
#define INTERNAL_LOG(msg, ...) PARSER_PRINTF(PARSER_NAME" log: " msg " (line %i)\n", \
                                           ##__VA_ARGS__, this->current_line)
// Sets the failed flag so the parser stops
#define INTERNAL_FAIL(msg, ...)             \
    do {                                    \
        INTERNAL_LOG(msg, ##__VA_ARGS__);   \
        this->failed = true;                \
    } while(0)


enum Conditional : unsigned char {
    COND_NONE = 0,
    COND_IF,
    COND_ELIF,
    COND_ELSE,
    COND_ENDIF,
};

struct ParserInternal {
    bool FindAndReplaceMacro(std::string& modified_buffer, std::string_view line);
    bool ParseDirective(std::string_view expr);
    void ParseExpression(std::string_view expr, Conditional directive);
    inline bool TokenizeAndEvaluate(std::string_view expr) {
        while (*expr.data() == ' ' || *expr.data() == '\t')
            expr.remove_prefix(1);

        std::pair<int, bool> result = EvaluateExpression(expr);
        if (result.second == false) {
            INTERNAL_FAIL("failed to evaluate expression %.*s", (int)expr.length(), expr.data());
            return 0;
        }
        return result.first != 0;
    }

    std::unordered_map<std::string_view, int> defines;

    struct ConditionalBranch {
        bool result;
        bool consumed;
        bool in_true_loop;
        Conditional cond;
    };
    std::stack<ConditionalBranch> condition;

    unsigned int current_line {0};
    bool failed  {false};
};

void ParserInternal::ParseExpression(std::string_view expr, Conditional eval) {
    bool curr_result = false;
    bool consumed = false;
    bool prev_result = true;
    bool in_true_loop = true;
    Conditional prev_cond = COND_NONE;

    if (!condition.empty()) {
        prev_result = condition.top().result;
        consumed = condition.top().consumed;
        in_true_loop = condition.top().in_true_loop;
        prev_cond = condition.top().cond;
    }
    bool in_nested_loop = !condition.empty();

    switch (eval) {
    case COND_IF:
        curr_result = TokenizeAndEvaluate(expr);
        condition.push({ curr_result, curr_result, in_true_loop && prev_result, COND_IF });
        break;

    case COND_ELIF:
        if (prev_cond == COND_ELSE) { INTERNAL_FAIL("#elif after #else"); break; }
        if (!in_nested_loop)        { INTERNAL_FAIL("#elif without #if"); break; }

        curr_result = TokenizeAndEvaluate(expr);
        condition.top().result = (!consumed && curr_result) && in_true_loop;
        condition.top().consumed = (consumed || curr_result);
        condition.top().cond = COND_ELIF;

        break;

    case COND_ELSE:
        if (prev_cond == COND_ELSE) { INTERNAL_FAIL("#else after #else"); break; }
        if (!in_nested_loop)        { INTERNAL_FAIL("#else without #if"); break; }

        condition.top().result = !consumed && in_true_loop;
        condition.top().consumed = true;
        condition.top().cond = COND_ELSE;

        break;

    case COND_ENDIF:
        if (!in_nested_loop)        { INTERNAL_FAIL("#endif without #if"); break; }
        PARSER_ASSERT(prev_cond != COND_ENDIF); // endif always pops

        condition.pop();
        break;

    default:
        INTERNAL_FAIL("Conditional logic error (should not happen)");
        break;
    }
}


bool ParserInternal::ParseDirective(std::string_view expr) {
    if (expr.compare(0, 2, "if") == 0) {
        expr.remove_prefix(2);
        if (*expr.data() != ' ')
            goto no_value;
        ParseExpression(expr, COND_IF);
        return false;
    }
    if (expr.compare(0, 4, "elif") == 0) {
        expr.remove_prefix(4);
        if (*expr.data() != ' ')
            goto no_value;
        ParseExpression(expr, COND_ELIF);
        return false;
    }

    // TODO: ensure there are no extra tokens after else and elif
    if (expr.compare(0, 4, "else") == 0) {
        expr.remove_prefix(4);
        ParseExpression(expr, COND_ELSE);
        return false;
    }
    if (expr.compare(0, 5, "endif") == 0) {
        expr.remove_prefix(4);
        ParseExpression(expr, COND_ENDIF);
        return false;
    }


#if defined(PARSER_IGNORE_UNKNOWN_DIRECTIVE)
    return true;
#else
    INTERNAL_LOG("unknown directive in %.*s", (int)expr.length(), expr.data());
    return false;
#endif

    no_value:
    INTERNAL_FAIL("expected value in directive");
    return false;
}

constexpr bool MaybePartOfWord(char c) {
    return ('0' <= c && c <= '9') ||
           ('a' <= c && c <= 'z') ||
           ('A' <= c && c <= 'Z') ||
           c == '_';
}

bool ParserInternal::FindAndReplaceMacro(std::string& modified_buf, std::string_view line_view) {
    modified_buf.clear();
    bool found = false;

    std::string_view current_view = line_view;
    unsigned int word_length = 0;

    // NOTE: this is a bit dirty and it could probably be cleaner
    while (word_length < current_view.length()) {
        if (!MaybePartOfWord(*(current_view.data() + word_length))) {
            if (word_length > 0) {
                auto kv_pair = this->defines.find({current_view.data(), word_length});
                if (kv_pair != this->defines.end()) {
                    found = true;
                    modified_buf.append(line_view.data(), current_view.data() - line_view.data());

                    // convert value to string
                    int value = kv_pair->second;
                    int value_len = std::snprintf(nullptr, 0, "%i", value) + 1;
#if defined(alloca)
                    char *value_str = (char *)alloca(value_len * sizeof(char));
#else
                    char value_str[value_len]; // This is an extension
#endif
                    std::snprintf(value_str, value_len, "%i", value);

                    // append the value
                    modified_buf.append(value_str);
                    line_view.remove_prefix(current_view.data() - line_view.data() + word_length);
                } else if (found) {
                    modified_buf.append(line_view.data(), current_view.data() - line_view.data() + word_length);
                    line_view.remove_prefix(current_view.data() - line_view.data() + word_length);
                }
                current_view.remove_prefix(word_length);
                word_length = 0;
            } else {
                current_view.remove_prefix(1);
            }
        } else {
            word_length++;
        }
    }
    // Append the remainder of non-word characters after the last word
    if (found)
        modified_buf.append(line_view.data(), current_view.data() - line_view.data() - 1);

    return found;
}

std::string SimplePreprocessor::Parse(const char *input_buffer, size_t buflen) {
    if (buflen == 0) {
        PARSER_LOG(PARSER_NAME": you passed a empty buffer.");
        return {};
    }

    ParserInternal internal;
    internal.defines = std::unordered_map<std::string_view, int>(this->global_defines.begin(), this->global_defines.end());

    std::string output;

    // used only when we find something during the macro processing pass
    std::string tmp_buf;
    std::string_view input_view(input_buffer, buflen);

    while (!input_view.empty()) {
        if (internal.failed)
            return {};

        internal.current_line += 1;

        size_t next_pos = input_view.find('\n');
        std::string_view row_final(input_view.data(), next_pos);

        // Macro preprocessor pass
        bool found = internal.FindAndReplaceMacro(tmp_buf, {input_view.data(), next_pos + 1});
        if (found) {
            row_final = tmp_buf;
        }

        // Sometimes we want to append the directive too.
        bool append = true;
        if (*row_final.data() == '#') {
            row_final.remove_prefix(1);

            while (*row_final.data() == ' ' || *row_final.data() == '\t')
                row_final.remove_prefix(1);

            append = internal.ParseDirective(row_final);
        }

        if (append) {
            if (internal.condition.empty() ||
                internal.condition.top().result == true) {
                output.append(row_final.data(), row_final.length());
                output.append("\n");
            }
        }

        if (next_pos == std::string::npos)
            break;

        input_view.remove_prefix(next_pos + 1);
    }

    if(!internal.condition.empty()) {
        PARSER_LOG(PARSER_NAME": unterminated conditional directive");
        return {};
    }

    return output;
}

std::string SimplePreprocessor::Parse(std::string const& input_buffer) {
    return this->Parse(input_buffer.data(), input_buffer.size());
}

