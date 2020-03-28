// SPDX-License-Identifier: MIT
//
// Trivial example implementation of N2505/P1315 `secure_clear`
//
// It just forwards the call to OS-specific functions. See the paper for
// references to actual implementations used in production in several
// major projects.
//
// Copyright (c) 2019-2020 Miguel Ojeda <miguel@ojeda.io>

#pragma once

#include <stddef.h> // size_t

#if defined(_WIN32)
#    include <Windows.h> // SecureZeroMemory
#elif defined(__linux__) || defined(__unix__)
#    include <string.h> // explicit_bzero
#else
#    error Unsupported platform
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline void secure_clear(void * data, size_t size)
{
#if defined(_WIN32)
    SecureZeroMemory(data, size);
#elif defined(__linux__) || defined(__unix__)
    explicit_bzero(data, size);
#else
#    error Unsupported platform
#endif
}

#ifdef __cplusplus
}
#endif

