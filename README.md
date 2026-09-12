# EmbeddedCUCTime

A minimal, embedded-friendly C library implementing the binary time code
formats of **CCSDS 301.0-B-4**: the **Unsegmented Time Code (CUC)**, the **Day
Segmented Time Code (CDS)** and the **Calendar Segmented Time Code (CCS)**.

Each code is an independent module with its own header, source file, static
library, example and test suite, so a project can take only the one it needs.

The library is deliberately small and dependency-free (only `<stdint.h>`,
`<stddef.h>`, `<stdbool.h>`), uses caller-supplied buffers (no heap), and keeps
its encode/decode paths integer-only so it fits comfortably on
microcontrollers.

## Standards Compliance

- **CCSDS 301.0-B-4** — *Time Code Formats*, Section 3.2 (CCSDS Unsegmented
  Time Code).
- **CCSDS 301.0-B-4** — *Time Code Formats*, Section 3.3 (CCSDS Day Segmented
  Time Code).
- **CCSDS 301.0-B-4** — *Time Code Formats*, Section 3.4 (CCSDS Calendar
  Segmented Time Code), with the segment ranges of annex A.

The standard is published by the CCSDS and is not redistributed here; see the
[References](#references) section.

## Features

Common to every module:

- Encode/decode the **P-field** and **T-field** separately, or as a combined
  self-identified code.
- Caller-supplied buffers, explicit checked error codes, no dynamic allocation.
- Format-independent time value, so the same value can be written at any
  resolution the code offers.

### Unsegmented Time Code (CUC)

- P-field of 1 or 2 octets; 1–7 basic (second) octets and 0–10 fractional
  octets.
- Both the CCSDS 1958 TAI epoch (Level 1) and an agency-defined epoch (Level 2).
- Time value held as integer seconds plus a Q0.64 binary fraction.
- Optional `double` conversion helpers, removable with `-DCUC_NO_FLOAT`.

### Day Segmented Time Code (CDS)

- One-octet P-field; 16- or 24-bit day segment and a 32-bit millisecond-of-day
  counter.
- Optional sub-millisecond segment at microsecond or picosecond resolution.
- Both the CCSDS 1958 epoch (Level 1) and an agency-defined epoch (Level 2).
- UTC-based: the millisecond-of-day range allows for a leap second.

### Calendar Segmented Time Code (CCS)

- One-octet P-field; every segment is binary coded decimal, so the encoded
  octets read back as the decimal digits themselves.
- Month-of-year/day-of-month or day-of-year calendar variation.
- 0–6 optional sub-second segments, giving 10^-2 s down to 10^-12 s.
- UTC-based, and encoding validates the value against the segment ranges of
  annex A: month lengths, leap years and the 60th second of a leap minute.

## Project Structure

```
EmbeddedCUCTime/
├── include/
│   ├── cuc.h                # CUC public API
│   ├── cds.h                # CDS public API
│   └── ccs.h                # CCS public API
├── src/
│   ├── cuc.c                # CUC implementation
│   ├── cds.c                # CDS implementation
│   └── ccs.c                # CCS implementation
├── examples/
│   ├── cuc_example.c        # Encode/decode demos
│   ├── cds_example.c
│   └── ccs_example.c
├── tests/
│   ├── cunit.h              # Minimal test framework
│   ├── test_runners.h       # Per-module runner declarations
│   ├── unit_tests.c         # Test entry point
│   ├── test_cuc.c           # Unit tests
│   ├── test_cds.c
│   └── test_ccs.c
├── tools/
│   └── coverage-html.sh     # Coverage report helper
├── Makefile
└── README.md
```

## Building

### Build everything

```bash
make            # static libraries, examples and test binary in build/
```

### Build the libraries only

```bash
make lib        # produces build/libcuc.a, build/libcds.a and build/libccs.a
```

### Build and run the examples

```bash
make example
./build/examples/cuc_example
./build/examples/cds_example
./build/examples/ccs_example
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
- **Largest code**: `CUC_OCTETS_MAX` = 19 octets (2 P-field + 17 T-field),
  `CDS_OCTETS_MAX` = 12 octets (1 + 11), `CCS_OCTETS_MAX` = 14 octets (1 + 13).
- **State**: the time and format structs of each module are small plain structs.

## Notes and Limitations

- CUC is **not UTC-based**; leap-second corrections do not apply. Use CDS or CCS
  for UTC-based time.
- The CUC fractional value is stored to 64 bits (Q0.64). Encodings that request
  more than 8 fractional octets carry those extra low octets as zero.
- CUC integer seconds fit in `uint64_t`; a field configured for fewer octets
  rolls over modulo 256 per octet, exactly as the standard specifies.
- Encoding a CDS or CCS value checks that it fits the segments the format
  provides. Decoding does not apply the same range checks, so a well-formed
  code may still carry an implausible value; `ccs_time_validate` is exposed for
  callers that need to check untrusted input. CCS decoding does reject segments
  that are not valid BCD.
- A CCS P-field with the extension flag set is rejected, since the second
  octet it announces is not defined by the standard.
- The ASCII time code (section 3.5) and agency-defined codes (section 3.6) of
  CCSDS 301.0-B-4 are not implemented.

## References

- CCSDS 301.0-B-4, *Time Code Formats*, Blue Book, Issue 4, November 2010.

## License

See the [LICENSE](LICENSE) file.
