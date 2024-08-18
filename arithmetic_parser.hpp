/******************************************************************************
 *  Arithmetic expression tokenizer and parser for a C-like preprocessor.
 *  
 *  Respects the order of precedence like most C-like languages.
 *  For now, it only supports multiplication, division, remainder, addition,
 *  subtraction, bit shifting, relational comparision, bitwise and logical
 *  operators.
 *
 *  Unsupported:
 *  - Only supports integers for now, but implementing floating point arithmetic
 *    shouldn't be too hard (not planned).
 *  - Doesn't support treating parenthesis without an operator as multiplication 
 *    like mathematical expressions do (e.x. " a (b + c) "), since this is not
 *    usually supported by programming languages or preprocessors. (not planned)
 *  - Doesn't support expressions like -a, !a, !(a + b) for now (planned).
 *    -a will have to be written as 0 - a
 *    TODO: the latter case (-a) could be solved by appending a 0 before the
 *          operand as needed when tokenizing.
 *  - Does not support consecutive operators (aside from parenthesis)
 *    (e.x. "a + - b")
 *
 ******************************************************************************
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

#pragma once

#include <string_view>

std::pair<int, bool> EvaluateExpression(std::string_view expr);

