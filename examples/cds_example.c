/**
 * @file    cds_example.c
 * @brief   Minimal usage example for the CDS time code library
 *
 * Encodes a day/millisecond time value into a self-identified CDS code and
 * decodes it back, printing the octets. Demonstrates CCSDS 301.0-B-4, 3.3.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cds.h"

#include <stdio.h>

int main(void)
{
    /* 16-bit day segment, microsecond resolution, CCSDS 1958 epoch. */
    cds_format_t fmt = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, CDS_SUBMS_US};

    /* Day 20000 since 1958-01-01, 12:34:56.789_123 into the day. */
    cds_time_t time = {
        .days = 20000u,
        .ms_of_day = (12u * 3600u + 34u * 60u + 56u) * 1000u + 789u,
        .submilliseconds = 123u, /* microseconds of the millisecond */
    };

    uint8_t buf[CDS_OCTETS_MAX];
    size_t written = 0;
    if (cds_encode(&time, &fmt, buf, sizeof(buf), &written) != CDS_OK)
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

    cds_format_t decoded_fmt;
    cds_time_t decoded;
    size_t consumed = 0;
    if (cds_decode(buf, written, &decoded_fmt, &decoded, &consumed) != CDS_OK)
    {
        fprintf(stderr, "decode failed\n");
        return 1;
    }

    printf("decoded: day %u, %u ms of day, %u us (%zu octets)\n",
           decoded.days,
           decoded.ms_of_day,
           decoded.submilliseconds,
           consumed);
    return 0;
}
