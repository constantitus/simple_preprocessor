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
#include <alloca.h>

#include "arithmetic_parser.hpp"
#include "simple_preprocessor.hpp"

#ifndef PARSER_NAME
#   define PARSER_NAME "Preprocessor"
#endif

// Prefix. Can be changed in case you want to use this with another preprocessor
#define _PFX '#'

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
    bool FindAndReplaceMacro(std::string& tmp_buffer, std::string_view line);
    bool ParseDirective(std::string_view expr);
    void DirectOutput(std::string_view expr);

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

    std::unordered_map<std::string_view, std::variant<std::string_view, int>> defines;
    unsigned int current_output_idx = 0;
    // unsigned int expected_outputs;

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
        if (prev_cond == COND_ELSE) { INTERNAL_FAIL("elif after else"); break; }
        if (!in_nested_loop)        { INTERNAL_FAIL("elif without if"); break; }

        curr_result = TokenizeAndEvaluate(expr);
        condition.top().result = (!consumed && curr_result) && in_true_loop;
        condition.top().consumed = (consumed || curr_result);
        condition.top().cond = COND_ELIF;

        break;

    case COND_ELSE:
        if (prev_cond == COND_ELSE) { INTERNAL_FAIL("else after else"); break; }
        if (!in_nested_loop)        { INTERNAL_FAIL("else without if"); break; }

        condition.top().result = !consumed && in_true_loop;
        condition.top().consumed = true;
        condition.top().cond = COND_ELSE;

        break;

    case COND_ENDIF:
        if (!in_nested_loop)        { INTERNAL_FAIL("endif without if"); break; }
        PARSER_ASSERT(prev_cond != COND_ENDIF); // endif always pops

        condition.pop();
        break;

    default:
        INTERNAL_FAIL("Conditional logic error (should not happen)");
        break;
    }
}

void ParserInternal::DirectOutput(std::string_view expr) {
    // TODO: this will fail if there are spaces after the index.
    while (*expr.data() == ' ' || *expr.data() == '\t')
        expr.remove_prefix(1);

    char *verify_length;
    int number = std::strtol(expr.data(), &verify_length, 10);
    if (verify_length != expr.data() + expr.length()) {
        INTERNAL_FAIL("expected index in output directive");
        return;
    }

    // TODO: Limit max number of outputs to one specified by the user
    this->current_output_idx = number;
}

bool ParserInternal::ParseDirective(std::string_view expr) {
    expr.remove_prefix(1); // '#'

    // get rid of spaces inbetween the prefix and the expression
    while (*expr.data() == ' ' || *expr.data() == '\t')
        expr.remove_prefix(1);

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

    // TODO: ensure there are no extra tokens after the directive
    if (expr.compare(0, 6, "output") == 0) {
        expr.remove_prefix(6);
        if (*expr.data() != ' ')
            goto no_value;
        DirectOutput(expr);
        return false;
    }
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

bool ParserInternal::FindAndReplaceMacro(std::string& tmp_buf, std::string_view line_view) {
    tmp_buf.clear();
    bool found = false;

    std::string_view current_view = line_view;
    unsigned int word_len = 0;

    while (word_len < current_view.length()) {
        if (!MaybePartOfWord(*(current_view.data() + word_len))) {
            if (word_len > 0) {
                size_t before_len = current_view.data() - line_view.data();

                auto kv_pair = this->defines.find({current_view.data(), word_len});
                if (kv_pair != this->defines.end()) {
                    found = true;
                    // append whatever is before the macro
                    size_t before_len = current_view.data() - line_view.data();
                    tmp_buf.append(line_view.data(), before_len);
                    line_view.remove_prefix(before_len + word_len);

                    auto& value_var = kv_pair->second;
                    if (std::holds_alternative<int>(value_var)) {
                        int *pvalue = std::get_if<int>(&value_var);

                        int value_len = std::snprintf(nullptr, 0, "%i", *pvalue) + 1;
                        char *value_buf = (char *)alloca(value_len * sizeof(char));
                        std::snprintf(value_buf, value_len, "%i", *pvalue);
                        value_len -= 1; // - the null terminator, we don't want that int the output.

                        tmp_buf.append(value_buf, value_len);
                    } else if (std::holds_alternative<std::string_view>(value_var)) {
                        std::string_view *pvalue = std::get_if<std::string_view>(&value_var);

                        tmp_buf.append(pvalue->data(), pvalue->length());
                    } else {
                        PARSER_ASSERT(false); // something went very wrong if this triggers
                    }
                } else if (found) {
                    tmp_buf.append(line_view.data(), before_len + word_len);
                    line_view.remove_prefix(before_len + word_len);
                }

                current_view.remove_prefix(word_len);
                word_len = 0;
            } else {
                current_view.remove_prefix(1);
            }
        } else {
            word_len++;
        }
    }

    // append the rest of the line
    if (found) {
        tmp_buf.append(line_view.data(), current_view.data() - line_view.data());
    }

    return found;
}

std::vector<std::string> SimplePreprocessor::Parse(const char *input_buffer, size_t buflen) {
    if (buflen == 0) {
        PARSER_LOG(PARSER_NAME": you passed a empty buffer.");
        return {};
    }

    ParserInternal internal;
    
    // I don't like this
    for (auto &def : this->global_defines) {
        auto& value_variant = def.second;
        if (std::holds_alternative<int>(value_variant)) {
            int *pvalue = std::get_if<int>(&value_variant);
            internal.defines[def.first] = *pvalue;
            continue;
        }
        if (std::holds_alternative<std::string>(value_variant)) {
            std::string *pvalue = std::get_if<std::string>(&value_variant);
            internal.defines[def.first] = *pvalue;
            continue;
        }
        PARSER_ASSERT(false);
    }

    std::vector<std::string> result;

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
            row_final = {tmp_buf.data(), tmp_buf.length() - 1};
        }

        // Parse thee directive (we sometimes want to append it to the output)
        bool append = true;
        if (*row_final.data() == _PFX) {
            append = internal.ParseDirective(row_final);
        }

        // NOTE: This is dirty. If (hypothetically) the indices we're getting from
        // the file are 0 and 14, we're going to have 15 strings, out of which 13
        // are unused.
        // TODO: Allow the user to specify the amount of outputs expected and handle
        // cases where the file declares more than that
        if (internal.current_output_idx >= result.size())
            result.resize(internal.current_output_idx + 1);
        std::string& output = result[internal.current_output_idx];

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

    return result;
}

std::vector<std::string> SimplePreprocessor::Parse(std::string const& input_buffer) {
    return this->Parse(input_buffer.data(), input_buffer.size());
}

