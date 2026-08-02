/**
 * @file    test_cds.c
 * @brief   Unit tests for the CCSDS Day Segmented Time Code (CDS) library
 *
 * Exercises the P-field / T-field codecs against the encodings defined in
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.3.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "../include/cds.h"
#include "cunit.h"
#include "test_runners.h"

#include <stdint.h>

/* {CCSDS epoch, 16-bit day, microsecond sub-ms}:
 * P-field = ext(0) id(100) epoch(0) day(0) subms(01) = 0100 0001 = 0x41. */
static int test_pfield_microsecond(void)
{
    cds_format_t fmt = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, CDS_SUBMS_US};
    uint8_t buf[1] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CDS_OK, cds_pfield_encode(&fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(1, (int)written);
    ASSERT_EQ_INT(0x41, buf[0]);

    cds_format_t out = {0, 0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CDS_OK, cds_pfield_decode(buf, written, &out, &consumed));
    ASSERT_EQ_INT(1, (int)consumed);
    ASSERT_EQ_INT(CDS_EPOCH_CCSDS, out.epoch);
    ASSERT_EQ_INT(CDS_DAY_16BIT, out.day_length);
    ASSERT_EQ_INT(CDS_SUBMS_US, out.submillisecond);
    return 0;
}

/* {Agency epoch, 24-bit day, picosecond sub-ms}:
 * P-field = ext(0) id(100) epoch(1) day(1) subms(10) = 0100 1110 = 0x4E. */
static int test_pfield_picosecond(void)
{
    cds_format_t fmt = {CDS_EPOCH_AGENCY, CDS_DAY_24BIT, CDS_SUBMS_PS};
    uint8_t buf[1] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CDS_OK, cds_pfield_encode(&fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(0x4E, buf[0]);

    cds_format_t out = {0, 0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CDS_OK, cds_pfield_decode(buf, written, &out, &consumed));
    ASSERT_EQ_INT(CDS_EPOCH_AGENCY, out.epoch);
    ASSERT_EQ_INT(CDS_DAY_24BIT, out.day_length);
    ASSERT_EQ_INT(CDS_SUBMS_PS, out.submillisecond);
    ASSERT_EQ_INT(11, (int)cds_tfield_size(&out)); /* 3 + 4 + 4 */
    return 0;
}

static int test_tfield_roundtrip(void)
{
    cds_format_t fmt = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, CDS_SUBMS_US};
    cds_time_t in = {1000u, 3661000u, 500u}; /* day 1000, 01:01:01.000, 500 us */
    uint8_t buf[CDS_TFIELD_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CDS_OK, cds_tfield_encode(&in, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(8, (int)written); /* 2 + 4 + 2 */

    uint8_t expected[8] = {0x03, 0xE8, 0x00, 0x37, 0xDC, 0xC8, 0x01, 0xF4};
    ASSERT_EQ_MEM(expected, buf, 8);

    cds_time_t out = {0, 0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CDS_OK, cds_tfield_decode(buf, written, &fmt, &out, &consumed));
    ASSERT_EQ_INT(8, (int)consumed);
    ASSERT_TRUE(out.days == in.days);
    ASSERT_TRUE(out.ms_of_day == in.ms_of_day);
    ASSERT_TRUE(out.submilliseconds == in.submilliseconds);
    return 0;
}

static int test_full_roundtrip(void)
{
    cds_format_t fmt = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, CDS_SUBMS_US};
    cds_time_t in = {1000u, 3661000u, 500u};
    uint8_t buf[CDS_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CDS_OK, cds_encode(&in, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(9, (int)written); /* 1 P-field + 8 T-field */
    ASSERT_EQ_INT(0x41, buf[0]);

    cds_format_t out_fmt = {0, 0, 0};
    cds_time_t out = {0, 0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CDS_OK, cds_decode(buf, written, &out_fmt, &out, &consumed));
    ASSERT_EQ_INT(9, (int)consumed);
    ASSERT_TRUE(out.days == in.days);
    ASSERT_TRUE(out.ms_of_day == in.ms_of_day);
    ASSERT_TRUE(out.submilliseconds == in.submilliseconds);
    return 0;
}

/* Millisecond resolution: no sub-ms segment, 24-bit day exercises the wide day path. */
static int test_millisecond_only(void)
{
    cds_format_t fmt = {CDS_EPOCH_CCSDS, CDS_DAY_24BIT, CDS_SUBMS_NONE};
    ASSERT_EQ_INT(0, (int)cds_subms_size(&fmt));
    ASSERT_EQ_INT(7, (int)cds_tfield_size(&fmt)); /* 3 + 4 + 0 */

    cds_time_t in = {0x0ABCDEu, 1234u, 0u};
    uint8_t buf[CDS_OCTETS_MAX] = {0};
    size_t written = 0;
    ASSERT_EQ_INT(CDS_OK, cds_encode(&in, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(8, (int)written);
    ASSERT_EQ_INT(0x44, buf[0]); /* id(100) day(1) subms(00) */

    cds_time_t out = {0, 0, 0};
    cds_format_t out_fmt = {0, 0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CDS_OK, cds_decode(buf, written, &out_fmt, &out, &consumed));
    ASSERT_TRUE(out.days == in.days);
    ASSERT_TRUE(out.submilliseconds == 0u);
    return 0;
}

/* Every rejection path of cds_format_validate, plus the accepted agency epoch. */
static int test_format_validate_errors(void)
{
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_format_validate(NULL));

    cds_format_t bad_epoch = {(cds_epoch_t)2, CDS_DAY_16BIT, CDS_SUBMS_NONE};
    ASSERT_EQ_INT(CDS_ERR_FORMAT, cds_format_validate(&bad_epoch));

    cds_format_t bad_day = {CDS_EPOCH_CCSDS, (cds_day_length_t)2, CDS_SUBMS_NONE};
    ASSERT_EQ_INT(CDS_ERR_FORMAT, cds_format_validate(&bad_day));

    cds_format_t bad_subms = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, (cds_subms_t)3};
    ASSERT_EQ_INT(CDS_ERR_FORMAT, cds_format_validate(&bad_subms));

    cds_format_t agency = {CDS_EPOCH_AGENCY, CDS_DAY_24BIT, CDS_SUBMS_PS};
    ASSERT_EQ_INT(CDS_OK, cds_format_validate(&agency));
    return 0;
}

/* Size helpers, including the NULL guards and each segment width. */
static int test_size_helpers(void)
{
    cds_format_t us = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, CDS_SUBMS_US};
    cds_format_t ps = {CDS_EPOCH_CCSDS, CDS_DAY_24BIT, CDS_SUBMS_PS};

    ASSERT_EQ_INT(0, (int)cds_day_size(NULL));
    ASSERT_EQ_INT(2, (int)cds_day_size(&us));
    ASSERT_EQ_INT(3, (int)cds_day_size(&ps));

    ASSERT_EQ_INT(0, (int)cds_subms_size(NULL));
    ASSERT_EQ_INT(2, (int)cds_subms_size(&us));
    ASSERT_EQ_INT(4, (int)cds_subms_size(&ps));

    ASSERT_EQ_INT(0, (int)cds_tfield_size(NULL));
    ASSERT_EQ_INT(8, (int)cds_tfield_size(&us)); /* 2 + 4 + 2 */

    ASSERT_EQ_INT(0, (int)cds_size(NULL));
    ASSERT_EQ_INT(9, (int)cds_size(&us));
    return 0;
}

static int test_pfield_encode_errors(void)
{
    cds_format_t fmt = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, CDS_SUBMS_US};
    cds_format_t invalid = {(cds_epoch_t)2, CDS_DAY_16BIT, CDS_SUBMS_US};
    uint8_t buf[CDS_PFIELD_OCTETS] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CDS_ERR_FORMAT, cds_pfield_encode(&invalid, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_pfield_encode(&fmt, NULL, sizeof(buf), &written));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_pfield_encode(&fmt, buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(CDS_ERR_BUFFER, cds_pfield_encode(&fmt, buf, 0, &written));
    return 0;
}

static int test_pfield_decode_errors(void)
{
    cds_format_t out = {0, 0, 0};
    size_t consumed = 0;
    uint8_t good[1] = {0x41};

    ASSERT_EQ_INT(CDS_ERR_NULL, cds_pfield_decode(NULL, 1, &out, &consumed));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_pfield_decode(good, 1, NULL, &consumed));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_pfield_decode(good, 1, &out, NULL));
    ASSERT_EQ_INT(CDS_ERR_BUFFER, cds_pfield_decode(good, 0, &out, &consumed));

    uint8_t bad_id[1] = {0x00}; /* id 000 is not CDS */
    ASSERT_EQ_INT(CDS_ERR_PFIELD_ID, cds_pfield_decode(bad_id, 1, &out, &consumed));

    uint8_t reserved[1] = {0x43}; /* id 100, sub-ms bits '11' = reserved */
    ASSERT_EQ_INT(CDS_ERR_FORMAT, cds_pfield_decode(reserved, 1, &out, &consumed));
    return 0;
}

static int test_tfield_encode_errors(void)
{
    cds_format_t fmt = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, CDS_SUBMS_US};
    cds_format_t invalid = {(cds_epoch_t)2, CDS_DAY_16BIT, CDS_SUBMS_US};
    cds_time_t t = {1u, 0u, 0u};
    uint8_t buf[CDS_TFIELD_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CDS_ERR_FORMAT, cds_tfield_encode(&t, &invalid, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_tfield_encode(NULL, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_tfield_encode(&t, &fmt, NULL, sizeof(buf), &written));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_tfield_encode(&t, &fmt, buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(CDS_ERR_BUFFER, cds_tfield_encode(&t, &fmt, buf, 2, &written));

    /* Range checks: day beyond the 16-bit segment, and a microsecond beyond 16 bits. */
    cds_time_t big_day = {0x10000u, 0u, 0u};
    ASSERT_EQ_INT(CDS_ERR_FORMAT, cds_tfield_encode(&big_day, &fmt, buf, sizeof(buf), &written));

    cds_time_t big_us = {0u, 0u, 0x10000u};
    ASSERT_EQ_INT(CDS_ERR_FORMAT, cds_tfield_encode(&big_us, &fmt, buf, sizeof(buf), &written));

    cds_format_t day24 = {CDS_EPOCH_CCSDS, CDS_DAY_24BIT, CDS_SUBMS_NONE};
    cds_time_t big_day24 = {0x1000000u, 0u, 0u};
    ASSERT_EQ_INT(CDS_ERR_FORMAT,
                  cds_tfield_encode(&big_day24, &day24, buf, sizeof(buf), &written));
    return 0;
}

static int test_tfield_decode_errors(void)
{
    cds_format_t fmt = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, CDS_SUBMS_US};
    cds_format_t invalid = {(cds_epoch_t)2, CDS_DAY_16BIT, CDS_SUBMS_US};
    cds_time_t out = {0, 0, 0};
    size_t consumed = 0;
    uint8_t buf[CDS_TFIELD_OCTETS_MAX] = {0};

    ASSERT_EQ_INT(CDS_ERR_FORMAT, cds_tfield_decode(buf, sizeof(buf), &invalid, &out, &consumed));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_tfield_decode(NULL, sizeof(buf), &fmt, &out, &consumed));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_tfield_decode(buf, sizeof(buf), &fmt, NULL, &consumed));
    ASSERT_EQ_INT(CDS_ERR_NULL, cds_tfield_decode(buf, sizeof(buf), &fmt, &out, NULL));
    ASSERT_EQ_INT(CDS_ERR_BUFFER, cds_tfield_decode(buf, 2, &fmt, &out, &consumed));
    return 0;
}

static int test_encode_decode_errors(void)
{
    cds_format_t fmt = {CDS_EPOCH_CCSDS, CDS_DAY_16BIT, CDS_SUBMS_US};
    cds_time_t t = {1u, 0u, 0u};
    uint8_t buf[CDS_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CDS_ERR_NULL, cds_encode(&t, &fmt, buf, sizeof(buf), NULL));
    /* No room even for the P-field preamble: the failure surfaces from the P-field stage. */
    ASSERT_EQ_INT(CDS_ERR_BUFFER, cds_encode(&t, &fmt, buf, 0, &written));
    /* Room for the P-field but not the T-field. */
    ASSERT_EQ_INT(CDS_ERR_BUFFER, cds_encode(&t, &fmt, buf, 2, &written));

    cds_format_t out_fmt = {0, 0, 0};
    cds_time_t out = {0, 0, 0};
    size_t consumed = 0;

    ASSERT_EQ_INT(CDS_ERR_NULL, cds_decode(buf, sizeof(buf), &out_fmt, &out, NULL));
    ASSERT_EQ_INT(CDS_ERR_BUFFER, cds_decode(buf, 0, &out_fmt, &out, &consumed));

    /* A valid one-octet P-field for an 8-octet T-field, but no T-field bytes follow. */
    uint8_t pfield_only[1] = {0x41};
    ASSERT_EQ_INT(CDS_ERR_BUFFER, cds_decode(pfield_only, 1, &out_fmt, &out, &consumed));
    return 0;
}

test_result_t test_cds_run_all(void)
{
    RUN_TEST(test_pfield_microsecond);
    RUN_TEST(test_pfield_picosecond);
    RUN_TEST(test_tfield_roundtrip);
    RUN_TEST(test_full_roundtrip);
    RUN_TEST(test_millisecond_only);
    RUN_TEST(test_format_validate_errors);
    RUN_TEST(test_size_helpers);
    RUN_TEST(test_pfield_encode_errors);
    RUN_TEST(test_pfield_decode_errors);
    RUN_TEST(test_tfield_encode_errors);
    RUN_TEST(test_tfield_decode_errors);
    RUN_TEST(test_encode_decode_errors);

    test_result_t r;
    r.total = cunit_total_tests;
    r.passed = cunit_total_tests - cunit_overall_failures;
    return r;
}
