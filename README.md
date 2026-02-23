mitu is a high-performance, offline C++ 20 CLI utility designed for instant (sub-millisecond) phone number metadata lookups. By providing a full E.164 phone number, the tool returns the associated geographic location, IANA time zone, and the local time.

Current Version: 0.1.0 (initial release)

- Built upon the google/libphonenumber dataset, supporting custom data loading during build.
- Massive performance achieved through a flattened static trie, memory-mapped database, and precached timezone lookups.
- Rigid memory safety by ulitizing RAII, bound-safe string views, and static_assert for binary layout verification.
- Strict database validation using CRC32 checksums, magic number verification, and schema versioning.

**What's next?** My top priorities are multithreading for batch processing and a fully functional user config file. Eventually, I'll look into building an extended default dataset.

See TODO.md for in-progress and implemented features.