/**
 * @file    test_ccs.c
 * @brief   Unit tests for the CCSDS Calendar Segmented Time Code (CCS) library
 *
 * Exercises the P-field / T-field codecs against the encodings defined in
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.4.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "../include/ccs.h"
#include "cunit.h"
#include "test_runners.h"

#include <stdint.h>
#include <string.h>

/* 2024-02-29T12:34:56.78, month/day variation with one sub-second segment. Every
 * segment is BCD, so the octets read back as the decimal digits themselves. */
static const ccs_time_t leap_day = {
    .year = 2024u,
    .month = 2u,
    .day = 29u,
    .day_of_year = 0u,
    .hour = 12u,
    .minute = 34u,
    .second = 56u,
    .subseconds = {78u},
};

/* {month/day variation, one sub-second segment}:
 * P-field = ext(0) id(101) variation(0) resolution(001) = 0101 0001 = 0x51. */
static int test_pfield_month_day(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};
    uint8_t buf[CCS_PFIELD_OCTETS] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CCS_OK, ccs_pfield_encode(&fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(1, (int)written);
    ASSERT_EQ_INT(0x51, buf[0]);

    ccs_format_t out = {0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CCS_OK, ccs_pfield_decode(buf, written, &out, &consumed));
    ASSERT_EQ_INT(1, (int)consumed);
    ASSERT_EQ_INT(CCS_VARIATION_MONTH_DAY, out.variation);
    ASSERT_EQ_INT(1, out.subsecond_segments);
    return 0;
}

/* {day-of-year variation, no sub-second segments}:
 * P-field = ext(0) id(101) variation(1) resolution(000) = 0101 1000 = 0x58. */
static int test_pfield_day_of_year(void)
{
    ccs_format_t fmt = {CCS_VARIATION_DAY_OF_YEAR, 0u};
    uint8_t buf[CCS_PFIELD_OCTETS] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CCS_OK, ccs_pfield_encode(&fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(0x58, buf[0]);

    ccs_format_t out = {0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CCS_OK, ccs_pfield_decode(buf, written, &out, &consumed));
    ASSERT_EQ_INT(CCS_VARIATION_DAY_OF_YEAR, out.variation);
    ASSERT_EQ_INT(0, out.subsecond_segments);
    ASSERT_EQ_INT(7, (int)ccs_tfield_size(&out)); /* 4 date + 3 time of day */
    return 0;
}

static int test_tfield_roundtrip(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};
    uint8_t buf[CCS_TFIELD_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CCS_OK, ccs_tfield_encode(&leap_day, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(8, (int)written); /* 4 + 3 + 1 */

    uint8_t expected[8] = {0x20, 0x24, 0x02, 0x29, 0x12, 0x34, 0x56, 0x78};
    ASSERT_EQ_MEM(expected, buf, 8);

    ccs_time_t out = {0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CCS_OK, ccs_tfield_decode(buf, written, &fmt, &out, &consumed));
    ASSERT_EQ_INT(8, (int)consumed);
    ASSERT_TRUE(out.year == leap_day.year);
    ASSERT_TRUE(out.month == leap_day.month);
    ASSERT_TRUE(out.day == leap_day.day);
    ASSERT_TRUE(out.hour == leap_day.hour);
    ASSERT_TRUE(out.minute == leap_day.minute);
    ASSERT_TRUE(out.second == leap_day.second);
    ASSERT_TRUE(out.subseconds[0] == leap_day.subseconds[0]);
    /* The variation carries no day of year, so decoding clears it. */
    ASSERT_TRUE(out.day_of_year == 0u);
    return 0;
}

static int test_full_roundtrip(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};
    uint8_t buf[CCS_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CCS_OK, ccs_encode(&leap_day, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(9, (int)written); /* 1 P-field + 8 T-field */
    ASSERT_EQ_INT(0x51, buf[0]);

    ccs_format_t out_fmt = {0, 0};
    ccs_time_t out = {0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CCS_OK, ccs_decode(buf, written, &out_fmt, &out, &consumed));
    ASSERT_EQ_INT(9, (int)consumed);
    ASSERT_EQ_INT(CCS_VARIATION_MONTH_DAY, out_fmt.variation);
    ASSERT_TRUE(out.year == leap_day.year);
    ASSERT_TRUE(out.day == leap_day.day);
    ASSERT_TRUE(out.subseconds[0] == leap_day.subseconds[0]);
    return 0;
}

/* Day 366 of a leap year: the day-of-year segment spans two octets whose four
 * most significant bits are unused and set to zero (CCSDS 301.0-B-4, 3.4.1.2). */
static int test_day_of_year_roundtrip(void)
{
    ccs_format_t fmt = {CCS_VARIATION_DAY_OF_YEAR, 0u};
    ccs_time_t in = {2024u, 0u, 0u, 366u, 23u, 59u, 60u, {0}}; /* with a leap second */
    uint8_t buf[CCS_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CCS_OK, ccs_encode(&in, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(8, (int)written); /* 1 P-field + 7 T-field */

    uint8_t expected[8] = {0x58, 0x20, 0x24, 0x03, 0x66, 0x23, 0x59, 0x60};
    ASSERT_EQ_MEM(expected, buf, 8);

    ccs_format_t out_fmt = {0, 0};
    ccs_time_t out = {0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CCS_OK, ccs_decode(buf, written, &out_fmt, &out, &consumed));
    ASSERT_EQ_INT(CCS_VARIATION_DAY_OF_YEAR, out_fmt.variation);
    ASSERT_TRUE(out.day_of_year == in.day_of_year);
    ASSERT_TRUE(out.second == in.second);
    /* The variation carries no month or day of month, so decoding clears them. */
    ASSERT_TRUE(out.month == 0u);
    ASSERT_TRUE(out.day == 0u);
    return 0;
}

/* Finest resolution: six sub-second segments down to 10^-12 s fill CCS_OCTETS_MAX. */
static int test_max_resolution(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, CCS_SUBSECOND_SEGMENTS_MAX};
    ccs_time_t in = {1u, 1u, 1u, 0u, 0u, 0u, 0u, {10u, 20u, 30u, 40u, 50u, 99u}};
    uint8_t buf[CCS_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CCS_TFIELD_OCTETS_MAX, (int)ccs_tfield_size(&fmt));
    ASSERT_EQ_INT(CCS_OK, ccs_encode(&in, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CCS_OCTETS_MAX, (int)written);
    ASSERT_EQ_INT(0x56, buf[0]); /* id(101) variation(0) resolution(110) */

    uint8_t expected_subseconds[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x99};
    ASSERT_EQ_MEM(expected_subseconds, &buf[CCS_PFIELD_OCTETS + CCS_TFIELD_OCTETS_MIN], 6);

    ccs_format_t out_fmt = {0, 0};
    ccs_time_t out = {0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CCS_OK, ccs_decode(buf, written, &out_fmt, &out, &consumed));
    ASSERT_EQ_INT(CCS_SUBSECOND_SEGMENTS_MAX, out_fmt.subsecond_segments);
    ASSERT_EQ_MEM(in.subseconds, out.subseconds, CCS_SUBSECOND_SEGMENTS_MAX);
    return 0;
}

/* Leap years are those divisible by 4, except centuries not divisible by 400. */
static int test_leap_year(void)
{
    ASSERT_TRUE(ccs_is_leap_year(2024u));
    ASSERT_TRUE(ccs_is_leap_year(2000u));
    ASSERT_TRUE(!ccs_is_leap_year(2023u));
    ASSERT_TRUE(!ccs_is_leap_year(1900u));
    return 0;
}

/* Every rejection path of ccs_format_validate, plus the accepted variations. */
static int test_format_validate_errors(void)
{
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_format_validate(NULL));

    ccs_format_t bad_variation = {(ccs_variation_t)2, 0u};
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_format_validate(&bad_variation));

    ccs_format_t too_many = {CCS_VARIATION_MONTH_DAY, CCS_SUBSECOND_SEGMENTS_MAX + 1u};
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_format_validate(&too_many));

    ccs_format_t month_day = {CCS_VARIATION_MONTH_DAY, 0u};
    ccs_format_t day_of_year = {CCS_VARIATION_DAY_OF_YEAR, CCS_SUBSECOND_SEGMENTS_MAX};
    ASSERT_EQ_INT(CCS_OK, ccs_format_validate(&month_day));
    ASSERT_EQ_INT(CCS_OK, ccs_format_validate(&day_of_year));
    return 0;
}

/* Segment ranges of CCSDS 301.0-B-4 annex A, including the calendar rules. */
static int test_time_validate_ranges(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};
    ccs_time_t t = leap_day;

    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_time_validate(NULL, &fmt));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_time_validate(&t, NULL));
    ASSERT_EQ_INT(CCS_OK, ccs_time_validate(&t, &fmt));

    t.year = 0u; /* the BCD year segment starts at 1 */
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));
    t.year = 10000u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));

    t = leap_day;
    t.month = 13u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));
    t.month = 0u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));

    /* 29 February exists in 2024 but not in 2023, and April is 30 days long. */
    t = leap_day;
    t.year = 2023u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));
    t = leap_day;
    t.month = 4u;
    t.day = 31u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));
    t.day = 0u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));

    t = leap_day;
    t.hour = 24u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));
    t = leap_day;
    t.minute = 60u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));

    /* Second 60 is the positive leap second; 61 is out of range. */
    t = leap_day;
    t.second = 60u;
    ASSERT_EQ_INT(CCS_OK, ccs_time_validate(&t, &fmt));
    t.second = 61u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));

    /* A sub-second segment holds two decimal digits, so 100 does not fit. */
    t = leap_day;
    t.subseconds[0] = 100u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));
    return 0;
}

/* Day-of-year bounds follow the length of the year. */
static int test_time_validate_day_of_year(void)
{
    ccs_format_t fmt = {CCS_VARIATION_DAY_OF_YEAR, 0u};
    ccs_time_t t = {2023u, 0u, 0u, 365u, 0u, 0u, 0u, {0}};

    ASSERT_EQ_INT(CCS_OK, ccs_time_validate(&t, &fmt));
    t.day_of_year = 366u; /* 2023 is not a leap year */
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));

    t.year = 2024u;
    ASSERT_EQ_INT(CCS_OK, ccs_time_validate(&t, &fmt));
    t.day_of_year = 367u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));
    t.day_of_year = 0u;
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_time_validate(&t, &fmt));
    return 0;
}

/* Size helpers, including the NULL and invalid-format guards. */
static int test_size_helpers(void)
{
    ccs_format_t none = {CCS_VARIATION_MONTH_DAY, 0u};
    ccs_format_t full = {CCS_VARIATION_DAY_OF_YEAR, CCS_SUBSECOND_SEGMENTS_MAX};
    ccs_format_t invalid = {CCS_VARIATION_MONTH_DAY, CCS_SUBSECOND_SEGMENTS_MAX + 1u};

    ASSERT_EQ_INT(0, (int)ccs_tfield_size(NULL));
    ASSERT_EQ_INT(0, (int)ccs_tfield_size(&invalid));
    ASSERT_EQ_INT(CCS_TFIELD_OCTETS_MIN, (int)ccs_tfield_size(&none));
    ASSERT_EQ_INT(CCS_TFIELD_OCTETS_MAX, (int)ccs_tfield_size(&full));

    ASSERT_EQ_INT(0, (int)ccs_size(NULL));
    ASSERT_EQ_INT(0, (int)ccs_size(&invalid));
    ASSERT_EQ_INT(CCS_TFIELD_OCTETS_MIN + 1, (int)ccs_size(&none));
    ASSERT_EQ_INT(CCS_OCTETS_MAX, (int)ccs_size(&full));
    return 0;
}

static int test_pfield_encode_errors(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};
    ccs_format_t invalid = {CCS_VARIATION_MONTH_DAY, CCS_SUBSECOND_SEGMENTS_MAX + 1u};
    uint8_t buf[CCS_PFIELD_OCTETS] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_pfield_encode(&invalid, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_pfield_encode(&fmt, NULL, sizeof(buf), &written));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_pfield_encode(&fmt, buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(CCS_ERR_BUFFER, ccs_pfield_encode(&fmt, buf, 0, &written));
    return 0;
}

static int test_pfield_decode_errors(void)
{
    ccs_format_t out = {0, 0};
    size_t consumed = 0;
    uint8_t good[1] = {0x51};

    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_pfield_decode(NULL, 1, &out, &consumed));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_pfield_decode(good, 1, NULL, &consumed));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_pfield_decode(good, 1, &out, NULL));
    ASSERT_EQ_INT(CCS_ERR_BUFFER, ccs_pfield_decode(good, 0, &out, &consumed));

    uint8_t bad_id[1] = {0x41}; /* id 100 is CDS, not CCS */
    ASSERT_EQ_INT(CCS_ERR_PFIELD_ID, ccs_pfield_decode(bad_id, 1, &out, &consumed));

    uint8_t unused_resolution[1] = {0x57}; /* id 101, resolution bits '111' */
    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_pfield_decode(unused_resolution, 1, &out, &consumed));

    uint8_t extended[1] = {0xD1}; /* extension flag set: a second P-field octet */
    ASSERT_EQ_INT(CCS_ERR_UNSUPPORTED, ccs_pfield_decode(extended, 1, &out, &consumed));
    return 0;
}

static int test_tfield_encode_errors(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};
    ccs_format_t invalid = {(ccs_variation_t)2, 1u};
    uint8_t buf[CCS_TFIELD_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CCS_ERR_FORMAT,
                  ccs_tfield_encode(&leap_day, &invalid, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_tfield_encode(NULL, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_tfield_encode(&leap_day, &fmt, NULL, sizeof(buf), &written));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_tfield_encode(&leap_day, &fmt, buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(CCS_ERR_BUFFER, ccs_tfield_encode(&leap_day, &fmt, buf, 7, &written));

    /* Range checks run before any octet is written. */
    ccs_time_t out_of_range = leap_day;
    out_of_range.day = 30u; /* February never has 30 days */
    ASSERT_EQ_INT(CCS_ERR_FORMAT,
                  ccs_tfield_encode(&out_of_range, &fmt, buf, sizeof(buf), &written));
    return 0;
}

static int test_tfield_decode_errors(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};
    ccs_format_t invalid = {(ccs_variation_t)2, 1u};
    ccs_time_t out = {0};
    size_t consumed = 0;
    uint8_t buf[8] = {0x20, 0x24, 0x02, 0x29, 0x12, 0x34, 0x56, 0x78};

    ASSERT_EQ_INT(CCS_ERR_FORMAT, ccs_tfield_decode(buf, sizeof(buf), &invalid, &out, &consumed));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_tfield_decode(NULL, sizeof(buf), &fmt, &out, &consumed));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_tfield_decode(buf, sizeof(buf), &fmt, NULL, &consumed));
    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_tfield_decode(buf, sizeof(buf), &fmt, &out, NULL));
    ASSERT_EQ_INT(CCS_ERR_BUFFER, ccs_tfield_decode(buf, 7, &fmt, &out, &consumed));
    return 0;
}

/* Nibbles above 9 are not decimal digits, so no segment may carry them. */
static int test_tfield_decode_bcd_errors(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};
    ccs_time_t out = {0};
    size_t consumed = 0;
    const uint8_t valid[8] = {0x20, 0x24, 0x02, 0x29, 0x12, 0x34, 0x56, 0x78};

    /* Corrupt one segment at a time: both year octets, month, day of month,
     * hour, minute, second and the sub-second segment. */
    for (size_t i = 0; i < sizeof(valid); i++)
    {
        uint8_t corrupted[8];
        memcpy(corrupted, valid, sizeof(valid));
        corrupted[i] = 0xAAu; /* both nibbles above 9 */
        ASSERT_EQ_INT(CCS_ERR_BCD,
                      ccs_tfield_decode(corrupted, sizeof(corrupted), &fmt, &out, &consumed));
    }

    /* A failed decode leaves the caller's value untouched. */
    ASSERT_TRUE(out.year == 0u);

    /* Day-of-year variation: the four unused bits of the segment must be zero and
     * the three digits that remain must all be decimal. */
    ccs_format_t doy = {CCS_VARIATION_DAY_OF_YEAR, 0u};
    const uint8_t doy_valid[7] = {0x20, 0x24, 0x03, 0x66, 0x23, 0x59, 0x60};
    uint8_t corrupted_doy[7];

    memcpy(corrupted_doy, doy_valid, sizeof(doy_valid));
    corrupted_doy[2] = 0x10u; /* an unused bit is set */
    ASSERT_EQ_INT(CCS_ERR_BCD,
                  ccs_tfield_decode(corrupted_doy, sizeof(corrupted_doy), &doy, &out, &consumed));

    memcpy(corrupted_doy, doy_valid, sizeof(doy_valid));
    corrupted_doy[2] = 0x0Bu; /* hundreds digit above 9 */
    ASSERT_EQ_INT(CCS_ERR_BCD,
                  ccs_tfield_decode(corrupted_doy, sizeof(corrupted_doy), &doy, &out, &consumed));

    memcpy(corrupted_doy, doy_valid, sizeof(doy_valid));
    corrupted_doy[3] = 0x6Au; /* units digit above 9 */
    ASSERT_EQ_INT(CCS_ERR_BCD,
                  ccs_tfield_decode(corrupted_doy, sizeof(corrupted_doy), &doy, &out, &consumed));
    return 0;
}

static int test_encode_decode_errors(void)
{
    ccs_format_t fmt = {CCS_VARIATION_MONTH_DAY, 1u};
    uint8_t buf[CCS_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_encode(&leap_day, &fmt, buf, sizeof(buf), NULL));
    /* No room even for the P-field preamble: the failure surfaces from the P-field stage. */
    ASSERT_EQ_INT(CCS_ERR_BUFFER, ccs_encode(&leap_day, &fmt, buf, 0, &written));
    /* Room for the P-field but not the T-field. */
    ASSERT_EQ_INT(CCS_ERR_BUFFER, ccs_encode(&leap_day, &fmt, buf, 2, &written));

    ccs_format_t out_fmt = {0, 0};
    ccs_time_t out = {0};
    size_t consumed = 0;

    ASSERT_EQ_INT(CCS_ERR_NULL, ccs_decode(buf, sizeof(buf), &out_fmt, &out, NULL));
    ASSERT_EQ_INT(CCS_ERR_BUFFER, ccs_decode(buf, 0, &out_fmt, &out, &consumed));

    /* A valid one-octet P-field for an 8-octet T-field, but no T-field octets follow. */
    uint8_t pfield_only[1] = {0x51};
    ASSERT_EQ_INT(CCS_ERR_BUFFER, ccs_decode(pfield_only, 1, &out_fmt, &out, &consumed));
    return 0;
}

test_result_t test_ccs_run_all(void)
{
    RUN_TEST(test_pfield_month_day);
    RUN_TEST(test_pfield_day_of_year);
    RUN_TEST(test_tfield_roundtrip);
    RUN_TEST(test_full_roundtrip);
    RUN_TEST(test_day_of_year_roundtrip);
    RUN_TEST(test_max_resolution);
    RUN_TEST(test_leap_year);
    RUN_TEST(test_format_validate_errors);
    RUN_TEST(test_time_validate_ranges);
    RUN_TEST(test_time_validate_day_of_year);
    RUN_TEST(test_size_helpers);
    RUN_TEST(test_pfield_encode_errors);
    RUN_TEST(test_pfield_decode_errors);
    RUN_TEST(test_tfield_encode_errors);
    RUN_TEST(test_tfield_decode_errors);
    RUN_TEST(test_tfield_decode_bcd_errors);
    RUN_TEST(test_encode_decode_errors);

    test_result_t r;
    r.total = cunit_total_tests;
    r.passed = cunit_total_tests - cunit_overall_failures;
    return r;
}
