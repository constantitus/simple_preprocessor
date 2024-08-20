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

#include "arithmetic_parser.hpp"

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <deque>
#include <queue>
#include <stack>


#ifndef PARSER_NAME
#   define PARSER_NAME "ArithmeticParser"
#endif

#define PARSER_PRINTF(msg, ...) printf(msg, ##__VA_ARGS__)
#define PARSER_LOG(msg, ...) PARSER_PRINTF(PARSER_NAME": " msg "\n", ##__VA_ARGS__)
#define PARSER_ASSERT(condition) assert(condition);

using operand_t = int;

enum Precedence : char {
    // https://en.cppreference.com/w/c/language/operator_precedence
    // https://en.cppreference.com/w/cpp/language/operator_precedence
    PRECEDENCE_NONE = 0,

    // PARENTHESIS are handled sepparately
    PRECEDENCE_MULT_DIV,
    PRECEDENCE_ADD_SUBT,
    PRECEDENCE_BITSHIFT,
    PRECEDENCE_RELATIONAL,
    PRECEDENCE_EQUALITY,
    PRECEDENCE_BIT_AND,
    PRECEDENCE_BIT_XOR,
    PRECEDENCE_BIT_OR,
    PRECEDENCE_LOGICAL_AND,
    PRECEDENCE_LOGICAL_OR,
};

enum Operator: short {
    // parenthesis
    OPER_PAREN_LEFT    = '(',               // (
    OPER_PAREN_RIGHT   = ')',               // )
    // 1: multiplication, division, remainder
    OPER_MULTIPLY      = '*',               // *
    OPER_DIVIDE        = '/',               // /
    OPER_REMINDER      = '%',               // %
    // 2: addition, subtraction
    OPER_ADD           = '+',               // +
    OPER_SUBTRACT      = '-',               // -
    // 3: bitwise left and right
    OPER_BITWISE_LEFT  = '<' | ('<' << 8),  // <<
    OPER_BITWISE_RIGHT = '>' | ('>' << 8),  // >>
    // 4: relational
    OPER_LESSER        = '<',               // <
    OPER_GREATER       = '>',               // >
    OPER_LESSER_EQ     = '<' | ('=' << 8),  // <=
    OPER_GREATER_EQ    = '>' | ('=' << 8),  // >=
    // 5: equality
    OPER_EQ_EQ         = '=' | ('=' << 8),  // ==
    OPER_NOT_EQ        = '!' | ('=' << 8),  // !=
    // 6, 7, 8: bitwise
    OPER_BIT_OR        = '|',               // |
    OPER_BIT_XOR       = '^',               // ^
    OPER_BIT_AND       = '&',               // &
    // 9, 10: logical operators
    OPER_LOGICAL_AND   = '&' | ('&' << 8),  // &&
    OPER_LOGICAL_OR    = '|' | ('|' << 8),  // ||
    // (not valid by themselves, only used when tokenizing)
    OPER_EQ            = '=',               // =
    OPER_NOT           = '!',               // !
};

static unsigned char GetOperatorPrecedence(short oper) {
    switch (oper) {
    case OPER_MULTIPLY:         return PRECEDENCE_MULT_DIV;
    case OPER_DIVIDE:           return PRECEDENCE_MULT_DIV;
    case OPER_REMINDER:         return PRECEDENCE_MULT_DIV;
    case OPER_ADD:              return PRECEDENCE_ADD_SUBT;
    case OPER_SUBTRACT:         return PRECEDENCE_ADD_SUBT;
    case OPER_BITWISE_LEFT:     return PRECEDENCE_BITSHIFT;
    case OPER_BITWISE_RIGHT:    return PRECEDENCE_BITSHIFT;
    case OPER_LESSER:           return PRECEDENCE_RELATIONAL;
    case OPER_GREATER:          return PRECEDENCE_RELATIONAL;
    case OPER_LESSER_EQ:        return PRECEDENCE_RELATIONAL;
    case OPER_GREATER_EQ:       return PRECEDENCE_RELATIONAL;
    case OPER_BIT_OR:           return PRECEDENCE_BIT_OR;
    case OPER_BIT_XOR:          return PRECEDENCE_BIT_XOR;
    case OPER_BIT_AND:          return PRECEDENCE_BIT_AND;
    case OPER_LOGICAL_OR:       return PRECEDENCE_LOGICAL_OR;
    case OPER_LOGICAL_AND:      return PRECEDENCE_LOGICAL_AND;
    case OPER_EQ_EQ:            return PRECEDENCE_EQUALITY;
    case OPER_NOT_EQ:           return PRECEDENCE_EQUALITY;
    default:                    return PRECEDENCE_NONE;
    }
}

struct Token {
    enum Type : unsigned char {
        NONE = 0,

        OPERATOR,
        OPERAND,
    };

    union {
        struct {
            short oper;
            unsigned char precedence;
        };
        operand_t operand;
    };
    Type type;
};

struct ArithmeticTokenizer {
    std::deque<Token> tokens;
    bool failed = false;

    void Tokenize(std::string_view expr);
    void Parse(char c);
    void Parse(std::string_view view);
    std::queue<Token> ShuntingYard();
};

void ArithmeticTokenizer::Parse(char c) {
    // keep track of previous token's type
    auto prev_type = Token::NONE;
    short prev = 0;
    if (!this->tokens.empty()) {
        prev_type = this->tokens.back().type;
        prev = this->tokens.back().oper;
    }

    // oper is basically a char[2] in the form of a short.

    // push_back open-parenthesis as a new operator regardless.
    if (c == OPER_PAREN_LEFT) {
        this->tokens.push_back({ .oper = c, .type = Token::OPERATOR});
        return;
    }

    if (prev_type != Token::OPERATOR) { // oper[0]
        this->tokens.push_back({ .oper = c, .type = Token::OPERATOR});
        return;
    } else { // oper[1]
        // Test against the previous operator and combine if possible
        switch (prev) {
        case OPER_LESSER:  if (c == OPER_EQ)      { this->tokens.back().oper = OPER_LESSER_EQ;   return; }
        case OPER_GREATER: if (c == OPER_EQ)      { this->tokens.back().oper = OPER_GREATER_EQ;  return; }
        case OPER_EQ:      if (c == OPER_EQ)      { this->tokens.back().oper = OPER_EQ_EQ;       return; }
        case OPER_NOT:     if (c == OPER_EQ)      { this->tokens.back().oper = OPER_NOT_EQ;      return; }
        case OPER_BIT_OR:  if (c == OPER_BIT_OR)  { this->tokens.back().oper = OPER_LOGICAL_OR;  return; }
        case OPER_BIT_AND: if (c == OPER_BIT_AND) { this->tokens.back().oper = OPER_LOGICAL_AND; return; }
        case OPER_PAREN_RIGHT:
        if (c != OPER_PAREN_RIGHT) { // "()" is not allowed. (we could pop_back())
            this->tokens.push_back({ .oper = c, .type = Token::OPERATOR});
            return;
        }
        default: break;
        }
        PARSER_LOG("failed to parse operator");
        this->failed = true;
        return;
    }
    PARSER_ASSERT(false);
}

void ArithmeticTokenizer::Parse(std::string_view tok_view) {
    PARSER_ASSERT(tok_view.length() > 0);

    // prevent consecutive operands
    if (!this->tokens.empty()) {
        if (this->tokens.back().type == Token::OPERAND) {
            PARSER_LOG("expected expression");
            this->failed = true;
            return;
        }
    }

    char *verify_length;
    operand_t number = std::strtol(tok_view.data(), &verify_length, 10);
    if (verify_length != tok_view.data() + tok_view.length())
        number = 0;
        // token is not a number (e.x. 123a would not be valid)
        // silently default it to 0
    tokens.push_back({ .operand = number, .type = Token::OPERAND, });

    return;
}

static constexpr bool IsLegalCharacter(char c) {
    // Ascii table - the symbols we don't need
    return c >= ' '  && c <= '|' && c != '{' && c != '\\' &&
           c != '['  && c != ']' && c != '@' && c != '?'  &&
           c != ';'  && c != ':' && c != '.' && c != '`'  &&
           c != '\'' && c != '"' && c != '$' && c != '#';
}

void ArithmeticTokenizer::Tokenize(std::string_view expr) {
    // Sanitize expression
    for (int i = 0; i < expr.size(); i++) {
        if (!IsLegalCharacter(expr[i])) {
            PARSER_LOG("illegal character (%c) in expression", expr[i]);
            return;
        }
    }

    // Tokenize
    unsigned int ptr = 0;
    while (ptr < expr.length()) {
        char c = expr[ptr];
        PARSER_ASSERT(c != '\n');
        switch (c) {
        case ' ': case '!': case '%': case '&':
        case '(': case ')': case '*': case '+':
        case '-': case '/': case '<': case '=':
        case '>': case '|':
            // fallthrough
            if (ptr > 0) {
                this->Parse({expr.data(), (size_t)ptr});
                expr.remove_prefix(ptr);
            }
            // error checking after we parse each token
            if (this->failed)
                return;

            if (c != ' ')
                this->Parse(c);
            ptr = 0;
            expr.remove_prefix(1);

            break;
        default:
            // get rid of spaces
            if (ptr == 0 && c == ' ')
                expr.remove_prefix(1);
            else
                ptr++;
            break;
        }
    }
    if (ptr > 0)
        this->Parse({expr.data(), (size_t)ptr});

    // Debug
    // printf(ANSI_BLUE"Expression "ANSI_RESET"(unmodified input order):  ");
    // for (auto const& tok: this->tokens) {
    //     PARSER_ASSERT(tok.type != Token::NONE);
    //     if (tok.type == Token::OPERATOR) {
    //         short c = tok.oper;
    //         printf("[%c%c]", c, c >> 8);
    //     } else {
    //         operand_t i = tok.operand;
    //         printf("(%i)", i);
    //     }
    // }
    // printf("\n");
}


std::queue<Token> ArithmeticTokenizer::ShuntingYard() {
    // https://en.wikipedia.org/wiki/Shunting_yard_algorithm
    Token token;

    std::queue<Token> out_queue;
    std::stack<Token> oper_stack;

    while (!this->tokens.empty()) {
        token = this->tokens.front();
        if (token.type == Token::OPERATOR)
            token.precedence = GetOperatorPrecedence(token.oper);
        this->tokens.pop_front();
        
        if (token.type == Token::OPERAND) {
            out_queue.push(token);
        } else {
            if (token.oper == OPER_PAREN_LEFT) {
                oper_stack.push(token);
            } else if (token.oper == OPER_PAREN_RIGHT) {
                while (!oper_stack.empty()) {
                    if (oper_stack.top().oper == OPER_PAREN_LEFT)
                        break;
                    out_queue.push(oper_stack.top());
                    oper_stack.pop();
                }
                if (oper_stack.empty()) {
                    PARSER_LOG("failure in number of parenthesis");
                    this->failed = true;
                    return {};
                }
                if (oper_stack.top().oper == OPER_PAREN_LEFT)
                    oper_stack.pop();
            } else {
                Token o1 = token;
                while (!oper_stack.empty()) {
                    Token o2 = oper_stack.top();
                    if (o2.oper != OPER_PAREN_LEFT && (o1.precedence >= o2.precedence)) {
                        oper_stack.pop();
                        out_queue.push(o2);
                    } else {
                        break;
                    }
                }
                oper_stack.push(o1);
            }
        }

    }
    while (!oper_stack.empty()) {
        if (oper_stack.top().oper != OPER_PAREN_LEFT) {
            out_queue.push(oper_stack.top());
            oper_stack.pop();
        } else {
            break;
        }
    }

    // Debug
    // printf(ANSI_BLUE"Expression "ANSI_RESET"(reverse polish notation): ");
    // for (int i = 0; i < out_queue.size(); i++) {
    //     auto const& tok = *(&out_queue.front() + i);
    //     if (tok.type == Token::OPERATOR) {
    //         short c = tok.oper;
    //         printf("[%c%c]", c, c >> 8);
    //     } else {
    //         int i = tok.operand;
    //         printf("(%i)", i);
    //     }
    // }
    // printf("\n");

    return out_queue;
}

std::pair<int, bool> EvaluateExpression(std::string_view expr) {
    ArithmeticTokenizer tokenizer;
    tokenizer.Tokenize(expr);
    if (tokenizer.failed || tokenizer.tokens.size() == 0) {
        return {0, false};
    }

    std::queue<Token> queue = tokenizer.ShuntingYard();
    if (tokenizer.failed)
        return {0, false};

    // Calculate
    std::vector<int> operands;

    while (!queue.empty()) {
        Token t = queue.front();
        queue.pop();

        if (t.type == Token::OPERAND) {
            operands.push_back(t.operand);
            continue;
        }

        if (operands.size() < 2) {
            PARSER_LOG("failure parsing arithmetic operation");
            return {0, false};
        }

        int left = operands.back();
        operands.pop_back();
        int right = operands.back();
        operands.pop_back();

        if ((t.oper == OPER_DIVIDE || t.oper == OPER_REMINDER) && left == 0) {
            PARSER_LOG("division by 0");
            return {0, false};
        }

        switch (t.oper) {
        case OPER_MULTIPLY:      operands.push_back(right *  left); break;
        case OPER_DIVIDE:        operands.push_back(right /  left); break;
        case OPER_REMINDER:      operands.push_back(right %  left); break;
        case OPER_ADD:           operands.push_back(right +  left); break;
        case OPER_SUBTRACT:      operands.push_back(right -  left); break;
        case OPER_BITWISE_LEFT:  operands.push_back(right << left); break;
        case OPER_BITWISE_RIGHT: operands.push_back(right >> left); break;
        case OPER_LESSER:        operands.push_back(right <  left); break;
        case OPER_LESSER_EQ:     operands.push_back(right <= left); break;
        case OPER_GREATER:       operands.push_back(right >  left); break;
        case OPER_GREATER_EQ:    operands.push_back(right >= left); break;
        case OPER_BIT_OR:        operands.push_back(right |  left); break;
        case OPER_BIT_XOR:       operands.push_back(right ^  left); break;
        case OPER_BIT_AND:       operands.push_back(right &  left); break;
        case OPER_LOGICAL_OR:    operands.push_back(right || left); break;
        case OPER_LOGICAL_AND:   operands.push_back(right && left); break;
        case OPER_EQ_EQ:         operands.push_back(right == left); break;
        case OPER_NOT_EQ:        operands.push_back(right != left); break;
        }
    }

    if (operands.size() != 1) {
        PARSER_LOG("failure in number of operands");
        return {0, false};
    }
    int result = operands.front();

    return {result, true};
}

