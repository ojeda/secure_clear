// SPDX-License-Identifier: MIT
//
// Trivial example implementation of N2505/P1315 `secure_clear`
//
// This is not a test suite. It is just a simple demonstration of
// the difference `secure_clear()` makes with respect to something
// like `memset()` when optimizations are enabled.
//
// To try it out, compare the output code generated for `test_secure_clear()`
// and `test_memset()`.
//
// An easy option to visualize it is to use a tool like Compiler Explorer
// (https://godbolt.org/).
//
// Copyright (c) 2019-2020 Miguel Ojeda <miguel@ojeda.io>

#include "secure_clear.h" // secure_clear

#include <string.h> // memset

void getPasswordFromUser(char *, size_t);
void usePassword(char *, size_t);

// Similar to the example in the paper
static void f(_Bool useSecureClear)
{
#define SIZE 100

    char password[SIZE];

    // Acquire some sensitive data
    getPasswordFromUser(password, SIZE);

    // Use it for some operations
    usePassword(password, SIZE);

    // Attempt to clear the sensitive data
    if (useSecureClear)
        secure_clear(password, SIZE);
    else
        memset(password, 0, SIZE);

#undef SIZE
}

void test_secure_clear()
{
    f(1);
}

void test_memset()
{
    f(0);
}

