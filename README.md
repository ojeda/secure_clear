# secure_clear

Sensitive data, like passwords or keying data, should be cleared from memory as soon as they are not needed. This requires ensuring the compiler will not optimize the memory overwrite away. This proposal adds a new header, `<secure>`, containing a `secure_clear` function and a `secure_clear` function template that guarantee users that a memory area is cleared.

