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

```c
#include "cuc.h"

/* 4 octets of seconds + 2 octets of fraction, CCSDS 1958 TAI epoch. */
cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
cuc_time_t   t   = cuc_time_from_seconds(1234567.25);

uint8_t buf[CUC_OCTETS_MAX];
size_t  n = 0;

if (cuc_encode(&t, &fmt, buf, sizeof(buf), &n) == CUC_OK) {
    /* buf[0..n-1] holds P-field + T-field: 1E 00 12 D6 87 40 00 */
}

cuc_format_t out_fmt;
cuc_time_t   out;
size_t       consumed = 0;
cuc_decode(buf, n, &out_fmt, &out, &consumed);   /* out ≈ 1234567.25 s */
```

To work with an implicitly-conveyed P-field (structure known from external
metadata), use `cuc_tfield_encode` / `cuc_tfield_decode` with a caller-supplied
`cuc_format_t` and omit the P-field entirely.

## API Reference

### Types

```c
typedef enum { CUC_EPOCH_CCSDS = 1, CUC_EPOCH_AGENCY = 2 } cuc_epoch_t;

typedef struct {
    cuc_epoch_t epoch;
    uint8_t     basic_octets;      /* 1..7  */
    uint8_t     fraction_octets;   /* 0..10 */
} cuc_format_t;

typedef struct {
    uint64_t seconds;    /* integer seconds since epoch          */
    uint64_t fraction;   /* binary fraction of a second, Q0.64   */
} cuc_time_t;

typedef enum {
    CUC_OK = 0, CUC_ERR_NULL, CUC_ERR_BUFFER,
    CUC_ERR_FORMAT, CUC_ERR_PFIELD_ID, CUC_ERR_UNSUPPORTED
} cuc_status_t;
```

### Functions

```c
/* Sizing / validation */
cuc_status_t cuc_format_validate(const cuc_format_t *fmt);
size_t       cuc_pfield_size(const cuc_format_t *fmt);   /* 1 or 2       */
size_t       cuc_tfield_size(const cuc_format_t *fmt);   /* basic + frac */
size_t       cuc_size(const cuc_format_t *fmt);          /* P + T        */

/* P-field only */
cuc_status_t cuc_pfield_encode(const cuc_format_t *fmt, uint8_t *buf,
                               size_t buf_len, size_t *written);
cuc_status_t cuc_pfield_decode(const uint8_t *buf, size_t buf_len,
                               cuc_format_t *fmt, size_t *consumed);

/* T-field only (implicit P-field) */
cuc_status_t cuc_tfield_encode(const cuc_time_t *time, const cuc_format_t *fmt,
                               uint8_t *buf, size_t buf_len, size_t *written);
cuc_status_t cuc_tfield_decode(const uint8_t *buf, size_t buf_len,
                               const cuc_format_t *fmt, cuc_time_t *time,
                               size_t *consumed);

/* Combined self-identified code (P-field + T-field) */
cuc_status_t cuc_encode(const cuc_time_t *time, const cuc_format_t *fmt,
                        uint8_t *buf, size_t buf_len, size_t *written);
cuc_status_t cuc_decode(const uint8_t *buf, size_t buf_len, cuc_format_t *fmt,
                        cuc_time_t *time, size_t *consumed);

/* Convenience (omitted with -DCUC_NO_FLOAT) */
double     cuc_time_to_seconds(const cuc_time_t *time);
cuc_time_t cuc_time_from_seconds(double seconds);
```

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
