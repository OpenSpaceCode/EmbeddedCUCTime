/**
 * @file    unit_tests.c
 * @brief   Unit tests for the CCSDS Unsegmented Time Code (CUC) library
 *
 * Exercises the P-field / T-field codecs against the encodings defined in
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.2.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cuc.h"
#include "cunit.h"

#include <stdint.h>

/* A common configuration: 4 basic octets + 2 fractional octets, CCSDS epoch.
 * P-field octet 1 = ext(0) id(001) basic-1(011) frac(10) = 0001 1110 = 0x1E. */
static int test_pfield_single_octet(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
    uint8_t buf[2] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CUC_OK, cuc_pfield_encode(&fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(1, (int)written);
    ASSERT_EQ_INT(0x1E, buf[0]);

    cuc_format_t out = {0, 0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CUC_OK, cuc_pfield_decode(buf, written, &out, &consumed));
    ASSERT_EQ_INT(1, (int)consumed);
    ASSERT_EQ_INT(CUC_EPOCH_CCSDS, out.epoch);
    ASSERT_EQ_INT(4, out.basic_octets);
    ASSERT_EQ_INT(2, out.fraction_octets);
    return 0;
}

/* 5 basic + 4 fractional octets forces a second P-field octet.
 * Octet 1 = ext(1) id(001) basic-1(011) frac(11) = 1001 1111 = 0x9F.
 * Octet 2 = ext(0) add_basic(01) add_frac(001) rsvd(00) = 0010 0100 = 0x24. */
static int test_pfield_extended(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 5, 4};
    uint8_t buf[2] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CUC_OK, cuc_pfield_encode(&fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(2, (int)written);
    ASSERT_EQ_INT(0x9F, buf[0]);
    ASSERT_EQ_INT(0x24, buf[1]);

    cuc_format_t out = {0, 0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CUC_OK, cuc_pfield_decode(buf, written, &out, &consumed));
    ASSERT_EQ_INT(2, (int)consumed);
    ASSERT_EQ_INT(5, out.basic_octets);
    ASSERT_EQ_INT(4, out.fraction_octets);
    return 0;
}

static int test_tfield_roundtrip(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
    cuc_time_t in = {0x12345678u, 0x8000000000000000u}; /* 0.5 s fraction */
    uint8_t buf[CUC_TFIELD_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CUC_OK, cuc_tfield_encode(&in, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(6, (int)written);

    uint8_t expected[6] = {0x12, 0x34, 0x56, 0x78, 0x80, 0x00};
    ASSERT_EQ_MEM(expected, buf, 6);

    cuc_time_t out = {0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CUC_OK, cuc_tfield_decode(buf, written, &fmt, &out, &consumed));
    ASSERT_EQ_INT(6, (int)consumed);
    ASSERT_TRUE(out.seconds == in.seconds);
    ASSERT_TRUE(out.fraction == in.fraction);
    return 0;
}

static int test_full_roundtrip(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
    cuc_time_t in = {0x12345678u, 0x8000000000000000u};
    uint8_t buf[CUC_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CUC_OK, cuc_encode(&in, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(7, (int)written); /* 1 P-field + 6 T-field */
    ASSERT_EQ_INT(0x1E, buf[0]);

    cuc_format_t out_fmt = {0, 0, 0};
    cuc_time_t out = {0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CUC_OK, cuc_decode(buf, written, &out_fmt, &out, &consumed));
    ASSERT_EQ_INT(7, (int)consumed);
    ASSERT_EQ_INT(4, out_fmt.basic_octets);
    ASSERT_EQ_INT(2, out_fmt.fraction_octets);
    ASSERT_TRUE(out.seconds == in.seconds);
    ASSERT_TRUE(out.fraction == in.fraction);
    return 0;
}

static int test_error_handling(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
    cuc_time_t t = {1, 0};
    uint8_t buf[3] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_encode(&t, &fmt, buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(CUC_ERR_BUFFER, cuc_encode(&t, &fmt, buf, sizeof(buf), &written));

    cuc_format_t bad = {CUC_EPOCH_CCSDS, 0, 0}; /* basic_octets must be >= 1 */
    ASSERT_EQ_INT(CUC_ERR_FORMAT, cuc_format_validate(&bad));

    uint8_t bad_id[1] = {0x00}; /* id 000 is reserved, not CUC */
    cuc_format_t out = {0, 0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CUC_ERR_PFIELD_ID, cuc_pfield_decode(bad_id, 1, &out, &consumed));
    return 0;
}

#ifndef CUC_NO_FLOAT
static int test_seconds_conversion(void)
{
    cuc_time_t t = cuc_time_from_seconds(1.5);
    ASSERT_TRUE(t.seconds == 1u);
    ASSERT_TRUE(t.fraction == 0x8000000000000000u);

    double s = cuc_time_to_seconds(&t);
    ASSERT_TRUE(s > 1.4999 && s < 1.5001);
    return 0;
}
#endif

int main(void)
{
    RUN_TEST(test_pfield_single_octet);
    RUN_TEST(test_pfield_extended);
    RUN_TEST(test_tfield_roundtrip);
    RUN_TEST(test_full_roundtrip);
    RUN_TEST(test_error_handling);
#ifndef CUC_NO_FLOAT
    RUN_TEST(test_seconds_conversion);
#endif

    if (cunit_overall_failures == 0)
    {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", cunit_overall_failures);
    return 1;
}
