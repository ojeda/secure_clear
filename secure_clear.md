[[attr][title][secure_clear (update to N2505)]]
[[attr][description][Sensitive data, like passwords or keying data, should be cleared from memory as soon as they are not needed. This requires ensuring the compiler will not optimize the memory overwrite away. This proposal adds a `secure_clear`-like function (C) and a `secure_clear`-like function template (C++) that guarantee users that a memory area is cleared.]]
[[attr][url][http://www.open-std.org/jtc1/sc22/wg14/www/docs/n2599.htm]]
[[attr][paper][N2599]]
[[attr][c++-url][http://wg21.link/P1315]]
[[attr][c++-paper][P1315]]
[[attr][c++-revision][R5]]

# `secure_clear` (update to N2505)

C Document number: [[print][{paper}]] \
C++ Document number: [[print][{c++-paper}]] [[latest]]([[print][{c++-url}]])\
Date: 2020-10-20\
Author: Miguel Ojeda \<[m@ojeda.dev](mailto:m@ojeda.dev)\>\
Project: ISO JTC1/SC22/WG14: Programming Language C\
Project: ISO JTC1/SC22/WG21: Programming Language C++


## Abstract

[[print][{description}]]


## Changelog

**N2599** -- Update to N2505:
  - Added 3 wording alternatives.
  - Removed "Naming" section and integrated the rationale into each proposed alternative.
  - Minor modifications in other sections, including style changes.


## The problem

When manipulating sensitive data, like passwords in memory or keying data, there is a need for library and application developers to clear the memory after the data is not needed anymore [[ref][MSC06]][[ref][CWE-14]][[ref][V597]][[ref][memset_s-paper]], in order to minimize the time window where it is possible to capture it (e.g. ending in a core dump or probed by a malicious actor). Recent vulnerabilities (e.g. Meltdown, Spectre-class, Rowhammer-class...) have made this requirement even more prominent.

In order to ensure that the memory is cleared, the developer needs to inform the compiler that it must not optimize away the memory write, even if it can prove it has no observable behavior. For C++, extra care is needed to consider all exceptional return paths.

For instance, the following C++ function may be vulnerable, since the compiler may optimize the `memset` call away because the `password` buffer is not read from before it goes out of scope:

    void f()
    {
        constexpr std::size_t size = 100;
        char password[size];

        // Acquire some sensitive data
        get_password_from_user(password, size);

        // Use it for some operations
        use_password(password, size);

        // Attempt to clear the sensitive data
        std::memset(password, 0, size);
    }

On top of that, `use_password` could throw an exception so `std::memset` is never called (i.e. assume the stack is not overwritten and/or that the memory is held in the free store).

There are many ways that developers may use to try to ensure the memory is cleared as expected (i.e. avoiding the optimizer):
  * Calling a function defined in another translation unit, assuming LTO/WPO is not enabled.
  * Writing memory through a `volatile` pointer (e.g. `decaf_bzero` [[ref][openssl-volatilewrite]] from OpenSSL).
  * Calling `memset` through a `volatile` function pointer (e.g. `OPENSSL_cleanse` C implementation [[ref][OPENSSL_cleanse-implementation]]).
  * Creating a dependency by writing an `extern` variable into it (e.g. `CRYPTO_malloc` implementation [[ref][openssl-c-dependency]] from OpenSSL).
  * Coding the memory write in assembly (e.g. `OPENSSL_cleanse` SPARC implementation [[ref][openssl-asm]]).
  * Introducing a memory barrier (e.g. `memzero_explicit` implementation [[ref][memzero_explicit-implementation]] from the Linux Kernel).
  * Disabling specific compiler options/optimizations (e.g. `-fno-builtin-memset` [[ref][gcc-no-builtin]] in GCC).

Or they may use a pre-existing solution whenever available:
  * Using an operating system-provided API (e.g. `explicit_bzero` [[ref][explicit_bzero]] from OpenBSD & FreeBSD, `SecureZeroMemory` [[ref][SecureZeroMemory]] from Windows).
  * Using a library function (e.g. `memzero_explicit` [[ref][memzero_explicit]][[ref][memzero_explicit-implementation]] from the Linux Kernel, `OPENSSL_cleanse` [[ref][OPENSSL_cleanse]][[ref][OPENSSL_cleanse-implementation]]).

Regardless of how it is done, none of these ways is --- at the same time --- portable, easy to recognize the intent (and/or `grep` for it), readily available and avoiding compiler implementation details. The topic may generate discussions in major projects on which is the best way to proceed and whether an specific approach ensures that the memory is actually cleansed (e.g. [[ref][discussions-1]][[ref][discussions-2]][[ref][discussions-3]][[ref][discussions-4]][[ref][discussions-5]]). Sometimes, such a way is not effective for a particular compiler (e.g. [[ref][difference-in-compilers]]). In the worst case, bugs happen (e.g. [[ref][bug-1]][[ref][bug-2]]).

C11 (and C++17 as it was based on it) added `memset_s` (K.3.7.4.1) to give a standard solution to this problem [[ref][memset_s-paper]][[ref][C11-draft]][[ref][memset_s-cppreference]]. However, it is an optional extension (Annex K) and, at the time of writing, several major compiler vendors do not define `__STDC_LIB_EXT1__` (GCC [[ref][memset_s-test-gcc]], Clang [[ref][memset_s-test-clang]], MSVC [[ref][memset_s-test-msvc]], icc [[ref][memset_s-test-icc]]). Therefore, in practical terms, there is no standard solution yet for C nor C++. A 2015 paper on this topic, WG14's N1967 "Field Experience With Annex K — Bounds Checking Interfaces" [[ref][WG14-N1967-Annex-K]], concludes that "Annex K should be removed from the C standard".

Other languages offer similar facilities in their standard libraries or as external projects:
  * The `SecureString` class in .NET Core, .NET Framework and Xamarin [[ref][.NET-SecureString]].
  * The `securemem` package in Haskell [[ref][Haskell-securemem]].
  * The `secstr` library in Rust [[ref][Rust-secstr]].
  * Limited private types in Ada and SPARK [[ref][AdaSPARK-Chapman-1]][[ref][AdaSPARK-Chapman-2]].


## The basic solution

We can standarize current practise by providing a C `secure_clear`-like function that takes a memory range (a pointer and a size) to be erased, guaranteeing that it won't be optimized away:

    secure_clear(password, size);

As well as a `secure_clear`-like function template for C++ that takes a reference to a non-pointer `trivially_copyable` object to scrub it entirely:

    std::secure_clear(password);

The precise names and interfaces depend on the semantics we want. Three main alternatives are proposed in a later section.

Note that the intention here is not to discuss Annex K in its entirety (e.g. to make it mandatory). Instead, we want to focus on a specific need that projects have right now (clearing sensitive data from memory), as explained in the previous sections.

An alternative solution would be to just make `memset_s` mandatory. However:
  * This implies specifying a particular value instead of leaving memory with indeterminate values, i.e. it looks like it is intended to "***set new values***" rather than "***disposing of old data***". This point was polled at WG21 Kona 2019 [[ref][P1315Kona2019]] and the result was to go for the latter (if the function were to be added to C++).
  * A different name for the function may also be key to emphasize the intended semantics.
  * `memset_s`' interface/signature follows Annex K patterns, which may be objected to.


## Further issues and improvements

While it is a good practise to ensure that the memory is cleared as soon as possible, there are other potential improvements when handling sensitive data in memory:
  * Reducing the number of copies to a minimum.
  * Clearing registers, caches, the entire stack frame, etc.
  * Locking/pinning memory to avoid the data being swapped out to external storage (e.g. disk).
  * Encrypting the memory while it is not being accessed to avoid plain-text leaks (e.g. to a log or in a core dump).
  * Turning off speculative execution (or mitigating it). At the time of writing, Spectre-class vulnerabilities are still being fixed (either in software or in hardware), and new ones are coming up [[ref][CppCon-2018-Spectre-talk]].
  * Techniques related to control flow, e.g. control flow balancing (making all branches spend the same amount of time).

Most of these extra improvements require either non-portable code, operating-system-specific calls, or compiler support as well, which also makes them a good target for standardization. A subset (e.g. encryption-at-rest and locking/pinning) may be relatively easy to tackle in the near future since libraries/projects and other languages handle them already. Other improvements, however, are well in the research stage on compilers, e.g.:
  * The SECURE project and GCC (stack erasure implemented as a function attribute) [[ref][SECURE-GCC-1]][[ref][SECURE-GCC-2]][[ref][SECURE-GCC-3]].
  * Research on controlling side-effects in mainstream C compilers [[ref][whatyouc]].

Furthermore, discussing the addition of security-related features to the C and C++ languages is rare. Therefore, this paper attempts to only spearhead the work on this area, providing access to users to the well-known "guaranteed memory clearing" function already used in the industry as explained in the previous sections.

Some other related C++ improvements based on the basic building blocks can be also thought of, too:
  * For instance, earlier revisions of the C++ paper [[ref][P1315R1]] proposed, in addition, an uncopyable class template meant to wrap a memory area on which `secure_clear` is called on destruction/move, plus some other features (explicit read/modify/write access, locking/pinning, encryption-at-rest, etc.). This wrapper is a good approach to ensure memory is cleared even when dealing with exceptions. During the WG21 LEWGI review, it was acknowledged that similar types are in use by third-parties. Some libraries and languages feature similar types. It was decided at WG21 Kona 2019 [[ref][P1315Kona2019]] to reduce the scope of the paper and only provide the basic building block, `secure_clear`.
  * A type-modifier with similar semantics as the wrapper class template above was suggested during the WG21 LEWGI review.
  * Dynamically-sized string-like types (e.g. `secure_string`) may be considered useful (e.g. for passwords).

There are, as well, other related library features:
  * A way to read non-echoed standard input (e.g. see the `getpass` module in Python [[ref][Python-getpass]]).

Note: at WG21 Kona 2019 [[ref][P1315Kona2019]] it was polled whether the compiler should be free to implement further guarantees (e.g. clearing cache/registers containing it) and/or whether we should encourage them to do so. The result was neutral, so further input from experts from both WG14 and WG21 was seeked to decide this point.

Note: given this function imposes unusual restrictions/behavior, this paper was forwarded to WG21 EWG at Kona 2019 [[ref][P1315Kona2019]], and then to WG21 SG1 at Cologne 2019 [[ref][P1315Cologne2019]].


## Proposal (C)

This proposal suggests three main alternatives for the addition of a `secure_clear`-like function, sorted from least to most similar to the existing Annex K `memset_s` interface.

The proposed wordings are with respect to the N2573 draft.

The suggested names for each alternative have been selected to try to reflect the semantics closely as well as to differentiate between them to facilitate discussion. They have also been picked to be within the space of reserved identifiers within `string.h`.

### Alternative 1: `memerase`

Compared to the others, this alternative:
  - Writes an indeterminate value.
  - Does *not* return the pointer.
  - Emphasizes the idea of erasing data, rather than overwriting with a particular value.

Writing an indeterminate value gives freedom to the implementation to choose the best value (or values, or patterns, or randomized values, etc.) for the purposes of erasing the sensitive data, which may depend on the particular hardware or the placement of the memory (e.g. global memory, stack memory, etc.). It also gives freedom to choose a bit pattern that might be a trap representation, which might be useful under certain environments to detect errors.

Furthermore, in principle it allows implementations to write a trap representation that triggers the program to stop.

For the use case of clearing a buffer that will be freed/destroyed, the advantage is that the written value is not important:

    void f(void) {
        char s[n];
        // ...
        memerase(s, n);  // No value needed
    }

However, if users *are* going to reuse the buffer, then they need to set a value explicitly, making such reuse more clear:

    memerase(s, n);
    memset(s, 42, n);  // Explicitly request a value

Making this case explicit is useful for implementations that may be able to print a diagnostic message if the buffer is read from without having being written again (e.g. similar to uninitialized variable warnings):

    memerase(s, n);
    printf("%s", s);  // Mistake

Furthermore, the function does not return the pointer as an attempt to prevent reuses of such pointer without careful consideration. It also prevents "hiding" the erasure in a single line like:

    memset(memerase(s, n), 42, n);  // Error

which makes it harder to spot (e.g. during a refactor).

Finally, this alternative is the one where avoiding `_explicit` in the name may be reasonable, since `erase` feels like performs something different than just a clear/zero/set of memory.

#### Proposed Wording

After "7.24.6.1 The `memset` function", add:

> #### 7.24.6.2 The `memerase` function
>
> ##### Synopsis
>
>     #include <string.h>
>     void memerase(void *s, size_t n);
>
> ##### Description
>
> The `memerase` function copies indeterminate values (converted to `unsigned char`) into each of the first `n` characters of the object pointed to by `s` as a needed side effect (5.1.2.3) (Footnote 1). The purpose of this function is to make sensitive information stored in the object inaccessible.
>
> (Footnote 1) The intention is that the memory store is always performed (i.e. never elided), regardless of optimizations. This is in constract to calls to the `memset` function.
>
> The implementation may not clear other copies of the data (e.g. intermediate values, cache lines, spilled registers, etc.).
>
> ##### Recommended Practice
>
> A call to the `memerase` function should have similar performance to a call to the `memset` function.
>
> The implementation should produce a diagnostic message if the written characters of the object pointed to by `s` are read from before they has been written again (i.e. they can be regarded as uninitialized after the call).

In "B.23 String handling `<string.h>`", after the `memset` line, add:

    void memerase(void *s, size_t n);

In "J.6.1 Rule based identifiers", after the `memset` line, add:

    memerase

### Alternative 2: `memclear_explicit`

Compared to the others, this alternative:
  - Writes a `0` value.
  - Returns the pointer.
  - Emphasizes that a zero is being written and that it is always performed.

For the use case of clearing a buffer that will be freed/destroyed, the advantage of unconditionally writing `0` is that no value needs to be passed:

    void f(void) {
        char s[n];
        // ...
        memclear_explicit(s, n);  // No value needed
    }

However, at the same time, if users *are* going to reuse the buffer, it is common that they need it to be zero-filled, which means they don't need to do anything else in that case:

    memclear_explicit(s, n);
    printf("%s", s);          // OK, initialized

Furthermore, since the memory is initialized, returning the pointer makes more sense to be used right away and also mimics the `memset` interface.

Finally, we add `_explicit` as a suffix because otherwise `memclear(s, n)` might be interpreted as just a shortcut for `memset(s, 0, n)` without the side effect guarantees.

#### Proposed Wording

After "7.24.6.1 The `memset` function", add:

> #### 7.24.6.2 The `memclear_explicit` function
>
> ##### Synopsis
>
>     #include <string.h>
>     void *memclear_explicit(void *s, size_t n);
>
> ##### Description
>
> The `memclear_explicit` function copies the value of `0` (converted to `unsigned char`) into each of the first `n` characters of the object pointed to by `s` as a needed side effect (5.1.2.3) (Footnote 1). The purpose of this function is to make sensitive information stored in the object inaccessible.
>
> (Footnote 1) The intention is that the memory store is always performed (i.e. never elided), regardless of optimizations. This is in constract to calls to the `memset` function.
>
> The implementation may not clear other copies of the data (e.g. intermediate values, cache lines, spilled registers, etc.).
>
> ##### Recommended Practice
>
> A call to the `memclear_explicit` function should have similar performance to a call to the `memset` function.
>
> ##### Returns
>
> The `memclear_explicit` function returns the value of `s`.

In "B.23 String handling `<string.h>`", after the `memset` line, add:

    void *memclear_explicit(void *s, size_t n);

In "J.6.1 Rule based identifiers", after the `memset` line, add:

    memclear_explicit

### Alternative 3: `memset_explicit`

Compared to the others, this alternative:
  - Writes a given `c` value.
  - Returns the pointer.
  - Emphasizes that a given value is being written and that it is always performed.

The main advantage of this alternative is that it follows `memset`'s interface, semantics and naming. Users are accustomed to it, and the only difference is that optimizations are not permitted.

Another advantage is that any need for a specific value for the buffer reuse use case is covered.

#### Proposed Wording

After "7.24.6.1 The `memset` function", add:

> #### 7.24.6.2 The `memset_explicit` function
>
> ##### Synopsis
>
>     #include <string.h>
>     void *memset_explicit(void *s, int c, size_t n);
>
> ##### Description
>
> The `memset_explicit` function copies the value of `c` (converted to `unsigned char`) into each of the first `n` characters of the object pointed to by `s` as a needed side effect (5.1.2.3) (Footnote 1). The purpose of this function is to make sensitive information stored in the object inaccessible.
>
> (Footnote 1) The intention is that the memory store is always performed (i.e. never elided), regardless of optimizations. This is in constract to calls to the `memset` function.
>
> The implementation may not clear other copies of the data (e.g. intermediate values, cache lines, spilled registers, etc.).
>
> ##### Recommended Practice
>
> A call to the `memset_explicit` function should have similar performance to a call to a `memset` function call.
>
> ##### Returns
>
> The `memset_explicit` function returns the value of `s`.

In "B.23 String handling `<string.h>`", after the `memset` line, add:

    void *memset_explicit(void *s, int c, size_t n);

In "J.6.1 Rule based identifiers", after the `memset` line, add:

    memset_explicit

## Proposal (C++)

This proposal adds a `secure_clear`-like function template to C++. The name for the function template will be decided based on the one chosen for C.

    namespace std {
        template <class T>
            requires is_trivially_copyable_v<T>
                and (not is_pointer_v<T>)
        void secure_clear(T & object) noexcept;
    }

The `secure_clear` function template is equivalent to a call to `secure_clear(addressof(object), sizeof(object))`.

The `not is_pointer_v<T>` constraint is intended to prevent unintended calls that would have cleared a pointer rather than the object it points to:

    char buf[100];
    char * buf2 = buf;

    std::secure_clear(buf);                 // OK, clears the array
    std::secure_clear(buf2);                // Error
    std::secure_clear(buf2, sizeof(buf2));  // OK, explicit

The return value is `void`, regardless of the alternative that is chosen on the C side, given that we are taking a reference to an object rather than a pointer and a size.


## Example implementation

A trivial example implementation (i.e. relying on OS-specific functions) can be found at [[ref][example-implementation]].


## Acknowledgements

Thanks to Aaron Ballman, JF Bastien, David Keaton and Billy O'Neal for providing guidance about the WG14 and WG21 standardization processes. Thanks to Aaron Ballman, Peter Sewell, David Svoboda, Martin Uecker and others for their input on wording. Thanks to Ryan McDougall for presenting the paper at WG21 Kona 2019. Thanks to Graham Markall for his input regarding the SECURE project and the current state on compiler support for related features. Thanks to Martin Sebor for pointing out the SECURE project. Thanks to BSI for suggesting constraining the template to non-pointers. Thanks to Philipp Klaus Krause for raising the discussion in the OpenBSD list. Thanks to everyone else that joined all the different discussions.


## References

[[refs][  {number}. {title} --- [*{url}*][{id}]\n]]

[MSC06]: https://wiki.sei.cmu.edu/confluence/display/c/MSC06-C.+Beware+of+compiler+optimizations "MSC06-C. Beware of compiler optimizations"
[CWE-14]: https://cwe.mitre.org/data/definitions/14.html "CWE-14: Compiler Removal of Code to Clear Buffers"
[V597]: https://www.viva64.com/en/w/v597/ "V597. The compiler could delete the `memset` function call (...)"
[C11-draft]: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1548.pdf "N1548 --- C11 draft"
[memset_s-paper]: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1381.pdf "N1381 --- #5 `memset_s()` to clear memory, without fear of removal"
[memset_s-cppreference]: https://en.cppreference.com/w/c/string/byte/memset "`memset`, `memset_s`"
[P1315R1]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1315r1.html "P1315R1 `secure_val`: a secure-clear-on-move type"
[P1315Kona2019]: http://wiki.edg.com/bin/view/Wg21kona2019/P1315 "P1315R1 minutes at Kona 2019"
[P1315Cologne2019]: http://wiki.edg.com/bin/view/Wg21cologne2019/P1315 "P1315R2 minutes at Cologne 2019"
[example-implementation]: https://github.com/ojeda/secure_clear/tree/master/example-implementation "`secure_clear` Example implementation"
[SecureZeroMemory]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa366877(v=vs.85).aspx "`SecureZeroMemory` function"
[explicit_bzero]: http://man7.org/linux/man-pages/man3/bzero.3.html "`bzero`, `explicit_bzero` - zero a byte string"
[OPENSSL_cleanse]: https://www.openssl.org/docs/man1.1.1/man3/OPENSSL_cleanse.html "`OPENSSL_cleanse`"
[OPENSSL_cleanse-implementation]: https://github.com/openssl/openssl/blob/master/crypto/mem_clr.c "`OPENSSL_cleanse` (implementation)"
[discussions-1]: https://github.com/openssl/openssl/pull/455 "Reimplement non-asm `OPENSSL_cleanse()` #455"
[discussions-2]: http://www.daemonology.net/blog/2014-09-04-how-to-zero-a-buffer.html "How to zero a buffer"
[discussions-3]: https://news.ycombinator.com/item?id=8270136 "Hacker News: How to zero a buffer (daemonology.net)"
[discussions-4]: https://stackoverflow.com/questions/13299420/ "Mac OS X equivalent of `SecureZeroMemory` / `RtlSecureZeroMemory`?"
[discussions-5]: https://gcc.gnu.org/ml/gcc-help/2014-10/msg00047.html "Optimising away `memset()` calls?"
[difference-in-compilers]: https://bugs.llvm.org/show_bug.cgi?id=15495 "Bug 15495 - dead store pass ignores memory clobbering asm statement"
[bug-1]: https://bugzilla.kernel.org/show_bug.cgi?id=82041 "Bug 82041 - `memset` optimized out in `random.c`"
[bug-2]: https://lkml.org/lkml/2014/8/25/497 "[PATCH] random: add and use memzero_explicit() for clearing data"
[memset_s-test-icc]: https://godbolt.org/g/vHZNrW "Test for `memset_s` in icc 18.0.0 at Godbolt"
[memset_s-test-gcc]: https://godbolt.org/g/M7MyRg "Test for `memset_s` in gcc 8.2 at Godbolt"
[memset_s-test-clang]: https://godbolt.org/g/ZwbkgY "Test for `memset_s` in clang 6.0.0 at Godbolt"
[memset_s-test-msvc]: https://godbolt.org/g/FtrVJ8 "Test for `memset_s` in MSVC 19 2017 at Godbolt"
[Python-getpass]: https://docs.python.org/3.7/library/getpass.html "Portable password input"
[.NET-SecureString]: https://docs.microsoft.com/en-us/dotnet/api/system.security.securestring "`SecureString` Class"
[Rust-secstr]: https://github.com/myfreeweb/secstr "`secstr`: Secure string library for Rust"
[Haskell-securemem]: https://hackage.haskell.org/package/securemem "`securemem`: abstraction to an auto scrubbing and const time eq, memory chunk."
[AdaSPARK-Chapman-1]: https://proteancode.com/wp-content/uploads/2017/06/ae2017_final3_for_web.pdf "Sanitizing Sensitive Data: How to get it Right (or at least Less Wrong…)"
[AdaSPARK-Chapman-2]: https://link.springer.com/chapter/10.1007%2F978-3-319-60588-3_3 "Sanitizing Sensitive Data: How to get it Right (or at least Less Wrong…)"
[whatyouc]: https://www.cl.cam.ac.uk/~rja14/Papers/whatyouc.pdf "What you get is what you C: Controlling side effects in mainstream C compilers"
[SECURE-GCC-1]: https://www.youtube.com/watch?v=Lg_jJLtH7R0 "The SECURE Project and GCC - GNU Tools Cauldron 2018"
[SECURE-GCC-2]: https://gcc.gnu.org/wiki/cauldron2018#secure "The SECURE project and GCC"
[SECURE-GCC-3]: https://www.embecosm.com/research/secure/ "The Security Enhancing Compilation for Use in Real Environments (SECURE) project"
[openssl-volatilewrite]: https://github.com/openssl/openssl/blob/f8385b0fc0215b378b61891582b0579659d0b9f4/crypto/ec/curve448/utils.c "`openssl/crypto/ec/curve448/utils.c` (old code)"
[openssl-c-dependency]: https://github.com/openssl/openssl/blob/104ce8a9f02d250dd43c255eb7b8747e81b29422/crypto/mem.c#L143 "`openssl/crypto/mem.c` (old code)"
[openssl-asm]: https://github.com/openssl/openssl/blob/master/crypto/sparccpuid.S#L363 "`openssl/crypto/sparccpuid.S` (example of assembly implementation)"
[gcc-no-builtin]: https://gcc.gnu.org/onlinedocs/gcc/C-Dialect-Options.html "Options Controlling C Dialect"
[memzero_explicit-implementation]: https://elixir.bootlin.com/linux/v4.18.5/source/lib/string.c#L706 "`memzero_explicit` (implementation)"
[memzero_explicit]: https://www.kernel.org/doc/htmldocs/kernel-api/API-memzero-explicit.html "`memzero_explicit`"
[WG14-N1967-Annex-K]: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1967.htm "N1967 (WG14) --- Field Experience With Annex K - Bounds Checking Interfaces"
[CppCon-2018-Spectre-talk]: https://www.youtube.com/watch?v=_f7O3IfIR2k "CppCon 2018: Chandler Carruth “Spectre: Secrets, Side-Channels, Sandboxes, and Security”"
