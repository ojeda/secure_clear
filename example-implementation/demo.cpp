// SPDX-License-Identifier: MIT
//
// Trivial example implementation of P1315 `secure_clear` (demo)
//
// This is not a test suite. It is just a simple demonstration of
// the difference std::secure_clear() makes with respect to something
// like std::memset().
//
// Compile using flags like -std=c++17 -O2 -S (gcc, clang, icc), /FA (msvc)
// or use a tool like the Compiler Explorer (https://godbolt.org/).
//
// Then compare the output code generated for test_secure_clear()
// and test_memset(), as well as taking a look at test_template().
//
// Copyright (c) 2019 Miguel Ojeda <miguel@ojeda.io>

#include "secure"

#include <cstring>

void getPasswordFromUser(char *, std::size_t) noexcept;
void usePassword(char *, std::size_t) noexcept;

namespace {

    // Similar to the example in the paper
    template <bool useSecureClear>
    void f() noexcept
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

void test_secure_clear() noexcept
{
    f<true>();
}

void test_memset() noexcept
{
    f<false>();
}

void test_template() noexcept
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

    // std::secure_clear(myComplexObject); // should fail
}

