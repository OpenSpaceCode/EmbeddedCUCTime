/**
 * @file    ccs_example.c
 * @brief   Minimal usage example for the CCS time code library
 *
 * Encodes a calendar date and time of day into a self-identified CCS code and
 * decodes it back, printing the octets. Demonstrates CCSDS 301.0-B-4, 3.4.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "ccs.h"

#include <stdio.h>

int main(void)
{
    /* Month-of-year/day-of-month variation with one sub-second segment, so the
     * code resolves to 10^-2 s. The P-field records both choices. */
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};

    /* 2024-02-29T12:34:56.78 UTC — a leap day, which the encoder accepts only
     * because 2024 is a leap year. */
    ccs_time_t time = {
        .year = 2024u,
        .month = 2u,
        .day = 29u,
        .hour = 12u,
        .minute = 34u,
        .second = 56u,
        .subseconds = {78u}, /* hundredths of a second */
    };

    uint8_t buf[CCS_OCTETS_MAX];
    size_t written = 0;
    /* Encoding validates the value against the segment ranges of annex A first,
     * so an impossible date such as 2023-02-29 is rejected here. */
    if (ccs_encode(&time, &fmt, buf, sizeof(buf), &written) != CCS_OK)
    {
        fprintf(stderr, "encode failed\n");
        return 1;
    }

    /* Every segment is BCD, so the octets read back as the decimal digits
     * themselves: 51 20 24 02 29 12 34 56 78. */
    printf("encoded %zu octets:", written);
    for (size_t i = 0; i < written; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\n");

    ccs_format_t decoded_fmt;
    ccs_time_t decoded;
    size_t consumed = 0;
    /* The P-field is self-identifying, so the decoder recovers the calendar
     * variation and the resolution before reading the T-field. */
    if (ccs_decode(buf, written, &decoded_fmt, &decoded, &consumed) != CCS_OK)
    {
        fprintf(stderr, "decode failed\n");
        return 1;
    }

    printf("decoded: %04u-%02u-%02uT%02u:%02u:%02u.%02u (%zu octets)\n",
           decoded.year,
           decoded.month,
           decoded.day,
           decoded.hour,
           decoded.minute,
           decoded.second,
           decoded.subseconds[0],
           consumed);

    /* The day-of-year variation replaces the month and day-of-month segments
     * with a single day-of-year segment; the rest of the code is unchanged. */
    ccs_format_t doy_fmt = {CCS_VARIATION_DAY_OF_YEAR, 0u};
    ccs_time_t doy_time = {
        .year = 2024u,
        .day_of_year = 60u, /* 29 February is day 60 of a leap year */
        .hour = 12u,
        .minute = 34u,
        .second = 56u,
    };

    written = 0;
    if (ccs_encode(&doy_time, &doy_fmt, buf, sizeof(buf), &written) != CCS_OK)
    {
        fprintf(stderr, "day-of-year encode failed\n");
        return 1;
    }

    printf("day-of-year variation, %zu octets:", written);
    for (size_t i = 0; i < written; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\n");
    return 0;
}
