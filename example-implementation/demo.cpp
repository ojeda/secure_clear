// SPDX-License-Identifier: MIT
//
// Trivial example implementation of N2505/P1315 `secure_clear`
//
// This is not a test suite. It is just a simple demonstration of
// the difference `secure_clear()` makes with respect to something
// like `memset()` when optimizations are enabled.
//
// To try it out, compare the output code generated for `test_secure_clear()`
// and `test_memset()`. Take also a look at `test_template()`.
//
// An easy option to visualize it is to use a tool like Compiler Explorer
// (https://godbolt.org/).
//
// Copyright (c) 2019-2020 Miguel Ojeda <miguel@ojeda.io>

#include "secure_clear" // secure_clear

#include <cstring> // memset

void getPasswordFromUser(char *, std::size_t);
void usePassword(char *, std::size_t);

namespace {

    // Similar to the example in the paper
    template <bool useSecureClear>
    void f()
    {
        constexpr std::size_t size = 100;
        char password[size];

        // Acquire some sensitive data
        getPasswordFromUser(password, size);

        // Use it for some operations
        usePassword(password, size);

        // Attempt to clear the sensitive data
        if constexpr (useSecureClear)
            std::secure_clear(password, size);
        else
            std::memset(password, 0, size);
    }

} // namespace

void test_secure_clear()
{
    f<true>();
}

void test_memset()
{
    f<false>();
}

void test_template()
{
    struct MyType {
        int a;
        char b;
        long c;
    } myObject;

    std::secure_clear(myObject);

    struct MyComplexType {
        ~MyComplexType() {}
    } myComplexObject;

    // std::secure_clear(myComplexObject); // Error

    char buf[100];
    char * buf2 = buf;

    std::secure_clear(buf);                // OK, clears the array
    // std::secure_clear(buf2);            // Error
    std::secure_clear(buf2, sizeof(buf2)); // OK, explicit
}

