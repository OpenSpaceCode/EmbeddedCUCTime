# CCSDS Unsegmented Time Code (CUC)

Reference notes for the CUC implementation in this repository. Based on
**CCSDS 301.0-B-4, *Time Code Formats*, Section 3.2** (Blue Book, November 2010).

## Overview

A CUC time code is a *pure binary count* of a basic time unit (the second)
and an optional binary fraction of that unit, measured from a defined epoch.
Because it is unsegmented and continuous (no discontinuities), arithmetic on
time differences can be performed directly on the encoded value — which is
what makes it well suited to spacecraft-clock and time-difference computations.

The code is **not UTC-based**; leap-second corrections do not apply.

A time code is a **P-field** (preamble, optional) followed by a **T-field**
(the time data). Both are an integral number of octets. Bit 0 is the most
significant bit and the first transmitted bit (CCSDS 301.0-B-4, 1.5).

## T-field

| Part          | Octets | Meaning                                          |
|---------------|--------|--------------------------------------------------|
| Basic time    | 1..7   | Integer count of seconds since the epoch (MSB first) |
| Fractional time | 0..10 | Binary fraction of a second (MSB = weight 2⁻⁸ first) |

Each octet is a counter that rolls over modulo 256, cascaded with its
neighbours. The value increases monotonically without reversion.

The CCSDS-recommended epoch is **1958 January 1 (TAI)** with the second as the
basic unit, giving a Level 1 time code. An agency-defined epoch is also allowed
(Level 2).

## P-field

### Octet 1 (mandatory when the P-field is explicit)

| Bit(s) | Field                        | Values |
|--------|------------------------------|--------|
| 0      | Extension flag               | 0 = last octet, 1 = octet 2 follows |
| 1–3    | Time code identification     | 001 = 1958 epoch (Level 1), 010 = agency epoch (Level 2) |
| 4–5    | Number of basic octets − 1   | 0..3 → 1..4 basic octets |
| 6–7    | Number of fractional octets  | 0..3 |

### Octet 2 (present only when the octet 1 extension flag is 1)

| Bit(s) | Field                                   | Values |
|--------|-----------------------------------------|--------|
| 0      | Extension flag                          | 0 = last octet |
| 1–2    | Additional basic octets (added to oct 1) | 0..3 |
| 3–5    | Additional fractional octets (added to oct 1) | 0..7 |
| 6–7    | Reserved for mission definition         | 0 |

The library emits octet 2 automatically whenever the basic octet count exceeds
4 or the fractional octet count exceeds 3.

## Implementation notes

- The decoded value (`cuc_time_t`) stores the fraction as a format-independent
  **Q0.64** binary fraction (`fraction / 2^64` seconds), so one value can be
  encoded into any fractional resolution. Resolution below 2⁻⁶⁴ s (i.e. beyond
  8 fractional octets) is not representable and is treated as zero.
- The core encode/decode path is integer-only. The `double` helpers
  (`cuc_time_to_seconds`, `cuc_time_from_seconds`) can be excluded with
  `-DCUC_NO_FLOAT`.
- The P-field is only two octets in this standard; a set extension flag on
  octet 2 (a hypothetical third octet) is rejected with `CUC_ERR_UNSUPPORTED`.

## Reference

Consultative Committee for Space Data Systems, *Time Code Formats*,
CCSDS 301.0-B-4, Blue Book, Issue 4, November 2010.
