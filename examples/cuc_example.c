/**
 * @file    cuc_example.c
 * @brief   Minimal usage example for the CUC time code library
 *
 * Encodes a time value into a self-identified CUC code and decodes it back,
 * printing the intermediate octets. Demonstrates CCSDS 301.0-B-4, Section 3.2.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cuc.h"

#include <stdio.h>

int main(void)
{
    /* 4 octets of seconds, 2 octets of fraction, CCSDS 1958 TAI epoch. */
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
    cuc_time_t time = cuc_time_from_seconds(1234567.25);

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

    printf("decoded: %.3f s (%u basic + %u fractional octets)\n",
           cuc_time_to_seconds(&decoded),
           decoded_fmt.basic_octets,
           decoded_fmt.fraction_octets);
    return 0;
}
