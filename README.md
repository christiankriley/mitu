mitu is a high-performance, offline C++ 20 CLI utility designed for instant (sub-millisecond) international phone number metadata lookups. By providing a full E.164 phone number, the tool returns the associated geographic location, IANA time zone, and the local time.

Current Version: 0.3.0

- Built upon the [google/libphonenumber](https://github.com/google/libphonenumber/) dataset, supporting custom data loading during build.
- Massive performance achieved through a flattened static trie, memory-mapped database, and precached timezone lookups.
- Rigid memory safety by utilizing RAII, bound-safe string views, and static_assert for binary layout verification.
- Strict database validation using CRC32 checksums, magic number verification, and schema versioning.
- Support for both 32-bit and 64-bit processors with x86_64 and ARM architecture on Linux, macOS, and Windows systems. (Note: Big Endian is not yet supported.) 

**What's new?** 0.3.0 brings full support for modern Microsoft Windows. Easily compile with MSVC using nmake.

**What's next?** My top priorities are multithreading for batch processing and a fully functional user config file. Eventually, I'll look into building an extended default dataset.

**Compilation:**

Linux:

mitu is developed on the latest version of [Bazzite Developer eXperience for Nvidia GPUs](https://dev.bazzite.gg/).
```
git clone https://github.com/christiankriley/mitu.git
cd mitu && make
```

macOS 14+:

You will need to [use Homebrew](https://github.com/Homebrew/brew/releases/latest) to install GCC because Clang does not fully support the C++ 20 chrono library. Fortunately, the Makefile handles finding GCC automatically once it's installed.
```
brew install gcc
git clone https://github.com/christiankriley/mitu.git
cd mitu && make
```

Windows 10+:

This program compiles perfectly with MSVC and can be built using the native nmake within the VS Developer Command Prompt. Windows 10 version 1903 (May 2019) or newer is required to ensure all OS calls and libraries are supported.
```
git clone https://github.com/christiankriley/mitu.git
cd mitu && nmake
```

**Usage:**

Linux & macOS:
``./mitu +15555556488``
``./mitu --version``

Windows:
``mitu.exe +15555556488``
``mitu.exe --version``

See TODO.md for in-progress and implemented features.