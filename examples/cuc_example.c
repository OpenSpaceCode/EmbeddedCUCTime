/**
 * @file    cuc_example.c
 * @brief   Minimal usage example for the CUC time code library
 *
 * Encodes a time value into a self-identified CUC code and decodes it back,
 * printing the intermediate octets. Demonstrates CCSDS 301.0-B-4, Section 3.2.
 *
 * The example stays integer-only, like the codec itself, so it builds and runs
 * unchanged on targets without an FPU and under -DCUC_NO_FLOAT. The optional
 * cuc_time_to_seconds() / cuc_time_from_seconds() helpers are deliberately
 * unused here.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cuc.h"

#include <stdio.h>

int main(void)
{
    /* 4 octets of seconds, 2 octets of fraction, CCSDS 1958 TAI epoch. */
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};

    /* 1234567.25 s since the epoch. The fraction is an unsigned Q0.64 binary
     * fraction of a second, so a quarter second is 2^64 / 4, i.e. 1 << 62. */
    cuc_time_t time = {UINT64_C(1234567), UINT64_C(1) << 62};

    uint8_t buf[CUC_OCTETS_MAX];
    size_t written = 0;
    if (cuc_encode(&time, &fmt, buf, sizeof(buf), &written) != CUC_OK)
    {
        fprintf(stderr, "encode failed\n");
        return 1;
    }

    printf("encoded %zu octets:", written);
    for (size_t i = 0; i < written; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\n");

    cuc_format_t decoded_fmt;
    cuc_time_t decoded;
    size_t consumed = 0;
    if (cuc_decode(buf, written, &decoded_fmt, &decoded, &consumed) != CUC_OK)
    {
        fprintf(stderr, "decode failed\n");
        return 1;
    }

    /* Render the Q0.64 fraction as milliseconds without floating point. Scaling
     * only the top 32 bits keeps the product inside uint64_t, and the discarded
     * low bits are worth less than 2^-32 s. */
    uint32_t fraction_hi = (uint32_t)(decoded.fraction >> 32);
    uint32_t milliseconds = (uint32_t)(((uint64_t)fraction_hi * 1000u) >> 32);

    printf("decoded: %llu.%03u s (%u basic + %u fractional octets)\n",
           (unsigned long long)decoded.seconds,
           milliseconds,
           decoded_fmt.basic_octets,
           decoded_fmt.fraction_octets);
    return 0;
}
