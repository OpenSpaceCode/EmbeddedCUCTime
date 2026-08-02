# EmbeddedCUCTime

A minimal, embedded-friendly C library implementing the **CCSDS Unsegmented
Time Code (CUC)** — a pure binary count of seconds and binary fractions of a
second from a defined epoch.

The library is deliberately small and dependency-free (only `<stdint.h>`,
`<stddef.h>`, `<stdbool.h>`), uses caller-supplied buffers (no heap), and keeps
its core encode/decode path integer-only so it fits comfortably on
microcontrollers.

## Standards Compliance

- **CCSDS 301.0-B-4** — *Time Code Formats*, Section 3.2 (CCSDS Unsegmented
  Time Code). See [`docs/ccsds_cuc.md`](docs/ccsds_cuc.md) for a field-by-field
  summary.

## Features

- Encode/decode the **P-field** (1 or 2 octets) and **T-field** separately, or
  as a combined self-identified code.
- Full format range: 1–7 basic (second) octets, 0–10 fractional octets.
- Both CCSDS 1958 TAI epoch (Level 1) and agency-defined epoch (Level 2).
- Format-independent time value (`cuc_time_t`) using a Q0.64 binary fraction.
- Optional `double` conversion helpers, removable with `-DCUC_NO_FLOAT`.
- Explicit, checked error codes; no dynamic allocation.

## Project Structure

```
EmbeddedCUCTime/
├── include/
│   └── cuc.h                # Public API
├── src/
│   └── cuc.c                # Implementation
├── examples/
│   └── cuc_example.c        # Encode/decode demo
├── tests/
│   ├── cunit.h              # Minimal test framework
│   └── unit_tests.c         # Unit tests
├── docs/
│   └── ccsds_cuc.md         # CUC format reference notes
├── tools/
│   └── coverage-html.sh     # Coverage report helper
├── Makefile
└── README.md
```

## Building

### Build everything

```bash
make            # static library, example and test binaries in build/
```

### Build the library only

```bash
make lib        # produces build/libcuc.a
```

### Build and run the example

```bash
make example
./build/examples/cuc_example
```

### Run the tests

```bash
make run        # or: make ctest && ./build/tests/ctest
```

### Coverage (HTML)

Requires `gcovr` (`pip install gcovr`):

```bash
make coverage-html   # writes build/coverage/index.html
```

### Clean

```bash
make clean
```

## Quick Start

See the examples.

## Memory Usage

- **No heap usage**: all buffers are caller-supplied.
- **Largest CUC code**: `CUC_OCTETS_MAX` = 19 octets (2 P-field + 17 T-field).
- **State**: `cuc_time_t` and `cuc_format_t` are small plain structs.

## Notes and Limitations

- CUC is **not UTC-based**; leap-second corrections do not apply (use CDS/CCS
  for UTC-based time — not yet implemented here).
- The fractional value is stored to 64 bits (Q0.64). Encodings that request
  more than 8 fractional octets carry those extra low octets as zero.
- Integer seconds fit in `uint64_t`; a field configured for fewer octets rolls
  over modulo 256 per octet, exactly as the standard specifies.
- This library implements the CUC format only. CDS, CCS and ASCII time codes
  from CCSDS 301.0-B-4 are out of scope for this initial version.

## References

- CCSDS 301.0-B-4, *Time Code Formats*, Blue Book, Issue 4, November 2010.

## License

See the [LICENSE](LICENSE) file.
