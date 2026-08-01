/**
 * @file    test_cuc.c
 * @brief   Unit tests for the CCSDS Unsegmented Time Code (CUC) library
 *
 * Exercises the P-field / T-field codecs against the encodings defined in
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.2.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "../include/cuc.h"
#include "cunit.h"
#include "test_runners.h"

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

/* Every rejection path of cuc_format_validate: NULL, each out-of-range field, and both
 * accepted epochs (CCSDS and Agency). */
static int test_format_validate_errors(void)
{
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_format_validate(NULL));

    cuc_format_t bad_epoch = {(cuc_epoch_t)0, 4, 2};
    ASSERT_EQ_INT(CUC_ERR_FORMAT, cuc_format_validate(&bad_epoch));

    cuc_format_t agency = {CUC_EPOCH_AGENCY, 4, 2};
    ASSERT_EQ_INT(CUC_OK, cuc_format_validate(&agency));

    cuc_format_t basic_low = {CUC_EPOCH_CCSDS, 0, 2};
    ASSERT_EQ_INT(CUC_ERR_FORMAT, cuc_format_validate(&basic_low));

    cuc_format_t basic_high = {CUC_EPOCH_CCSDS, CUC_BASIC_OCTETS_MAX + 1, 2};
    ASSERT_EQ_INT(CUC_ERR_FORMAT, cuc_format_validate(&basic_high));

    cuc_format_t frac_high = {CUC_EPOCH_CCSDS, 4, CUC_FRACTION_OCTETS_MAX + 1};
    ASSERT_EQ_INT(CUC_ERR_FORMAT, cuc_format_validate(&frac_high));

    cuc_format_t good = {CUC_EPOCH_CCSDS, 4, 2};
    ASSERT_EQ_INT(CUC_OK, cuc_format_validate(&good));
    return 0;
}

/* The stand-alone size helpers, including the NULL guards and both P-field widths.
 * A format extended only by its fraction count exercises the second half of the
 * cuc_format_is_extended predicate. */
static int test_size_helpers(void)
{
    cuc_format_t single = {CUC_EPOCH_CCSDS, 4, 2};
    cuc_format_t ext_basic = {CUC_EPOCH_CCSDS, 5, 4};
    cuc_format_t ext_frac = {CUC_EPOCH_CCSDS, 4, 5};

    ASSERT_EQ_INT(0, (int)cuc_pfield_size(NULL));
    ASSERT_EQ_INT(1, (int)cuc_pfield_size(&single));
    ASSERT_EQ_INT(2, (int)cuc_pfield_size(&ext_basic));
    ASSERT_EQ_INT(2, (int)cuc_pfield_size(&ext_frac));

    ASSERT_EQ_INT(0, (int)cuc_tfield_size(NULL));
    ASSERT_EQ_INT(6, (int)cuc_tfield_size(&single));

    ASSERT_EQ_INT(0, (int)cuc_size(NULL));
    ASSERT_EQ_INT(7, (int)cuc_size(&single));
    return 0;
}

static int test_pfield_encode_errors(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
    cuc_format_t invalid = {CUC_EPOCH_CCSDS, 0, 0};
    uint8_t buf[CUC_PFIELD_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CUC_ERR_FORMAT, cuc_pfield_encode(&invalid, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_pfield_encode(&fmt, NULL, sizeof(buf), &written));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_pfield_encode(&fmt, buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(CUC_ERR_BUFFER, cuc_pfield_encode(&fmt, buf, 0, &written));
    return 0;
}

static int test_pfield_decode_errors(void)
{
    cuc_format_t out = {0, 0, 0};
    size_t consumed = 0;
    uint8_t single[1] = {0x1E};

    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_pfield_decode(NULL, 1, &out, &consumed));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_pfield_decode(single, 1, NULL, &consumed));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_pfield_decode(single, 1, &out, NULL));
    ASSERT_EQ_INT(CUC_ERR_BUFFER, cuc_pfield_decode(single, 0, &out, &consumed));

    /* The Agency epoch id (010) is a valid CUC identification and must round-trip. */
    cuc_format_t agency = {CUC_EPOCH_AGENCY, 4, 2};
    uint8_t agency_pf[CUC_PFIELD_OCTETS_MAX] = {0};
    size_t written = 0;
    ASSERT_EQ_INT(CUC_OK, cuc_pfield_encode(&agency, agency_pf, sizeof(agency_pf), &written));
    ASSERT_EQ_INT(CUC_OK, cuc_pfield_decode(agency_pf, written, &out, &consumed));
    ASSERT_EQ_INT(CUC_EPOCH_AGENCY, out.epoch);

    /* Extension bit set in octet 1, but only one octet is supplied. */
    uint8_t truncated_ext[1] = {0x90};
    ASSERT_EQ_INT(CUC_ERR_BUFFER, cuc_pfield_decode(truncated_ext, 1, &out, &consumed));

    /* Extension bit set in octet 2 would request a third octet, which is unsupported. */
    uint8_t third_octet[2] = {0x90, 0x80};
    ASSERT_EQ_INT(CUC_ERR_UNSUPPORTED, cuc_pfield_decode(third_octet, 2, &out, &consumed));
    return 0;
}

static int test_tfield_encode_errors(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
    cuc_format_t invalid = {CUC_EPOCH_CCSDS, 0, 0};
    cuc_time_t t = {1, 0};
    uint8_t buf[CUC_TFIELD_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CUC_ERR_FORMAT, cuc_tfield_encode(&t, &invalid, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_tfield_encode(NULL, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_tfield_encode(&t, &fmt, NULL, sizeof(buf), &written));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_tfield_encode(&t, &fmt, buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(CUC_ERR_BUFFER, cuc_tfield_encode(&t, &fmt, buf, 2, &written));
    return 0;
}

static int test_tfield_decode_errors(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
    cuc_format_t invalid = {CUC_EPOCH_CCSDS, 0, 0};
    cuc_time_t out = {0, 0};
    size_t consumed = 0;
    uint8_t buf[CUC_TFIELD_OCTETS_MAX] = {0};

    ASSERT_EQ_INT(CUC_ERR_FORMAT, cuc_tfield_decode(buf, sizeof(buf), &invalid, &out, &consumed));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_tfield_decode(NULL, sizeof(buf), &fmt, &out, &consumed));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_tfield_decode(buf, sizeof(buf), &fmt, NULL, &consumed));
    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_tfield_decode(buf, sizeof(buf), &fmt, &out, NULL));
    ASSERT_EQ_INT(CUC_ERR_BUFFER, cuc_tfield_decode(buf, 2, &fmt, &out, &consumed));
    return 0;
}

/* Ten fractional octets exceed the 8 held in the Q0.64 representation, so the two low
 * octets are transmitted as zero and dropped on decode while the value round-trips. */
static int test_wide_fraction_roundtrip(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, CUC_FRACTION_OCTETS_MAX};
    cuc_time_t in = {0x11223344u, 0xABCDEF0123456789u};
    uint8_t buf[CUC_TFIELD_OCTETS_MAX] = {0};
    size_t written = 0;

    ASSERT_EQ_INT(CUC_OK, cuc_tfield_encode(&in, &fmt, buf, sizeof(buf), &written));
    ASSERT_EQ_INT(14, (int)written);
    ASSERT_EQ_INT(0, buf[12]);
    ASSERT_EQ_INT(0, buf[13]);

    cuc_time_t out = {0, 0};
    size_t consumed = 0;
    ASSERT_EQ_INT(CUC_OK, cuc_tfield_decode(buf, written, &fmt, &out, &consumed));
    ASSERT_EQ_INT(14, (int)consumed);
    ASSERT_TRUE(out.seconds == in.seconds);
    ASSERT_TRUE(out.fraction == in.fraction);
    return 0;
}

static int test_encode_decode_errors(void)
{
    cuc_format_t fmt = {CUC_EPOCH_CCSDS, 4, 2};
    cuc_time_t t = {1, 0};
    uint8_t buf[CUC_OCTETS_MAX] = {0};
    size_t written = 0;

    /* No room even for the P-field preamble: the failure surfaces from the P-field stage. */
    ASSERT_EQ_INT(CUC_ERR_BUFFER, cuc_encode(&t, &fmt, buf, 0, &written));

    cuc_format_t out_fmt = {0, 0, 0};
    cuc_time_t out = {0, 0};
    size_t consumed = 0;

    ASSERT_EQ_INT(CUC_ERR_NULL, cuc_decode(buf, sizeof(buf), &out_fmt, &out, NULL));
    ASSERT_EQ_INT(CUC_ERR_BUFFER, cuc_decode(buf, 0, &out_fmt, &out, &consumed));

    /* A valid one-octet P-field for a 6-octet T-field, but no T-field bytes follow. */
    uint8_t pfield_only[1] = {0x1E};
    ASSERT_EQ_INT(CUC_ERR_BUFFER, cuc_decode(pfield_only, 1, &out_fmt, &out, &consumed));
    return 0;
}

#ifndef CUC_NO_FLOAT
static int test_seconds_conversion(void)
{
    cuc_time_t t = cuc_time_from_seconds(1.5);
    ASSERT_TRUE(t.seconds == 1u);
    ASSERT_TRUE(t.fraction == 0x8000000000000000u);

    double s = cuc_time_to_seconds(&t);
    ASSERT_TRUE((s > 1.4999) && (s < 1.5001));
    return 0;
}

/* NULL input converts to zero seconds; a negative input yields a zero time value. */
static int test_seconds_conversion_edge(void)
{
    ASSERT_TRUE(cuc_time_to_seconds(NULL) == 0.0);

    cuc_time_t z = cuc_time_from_seconds(-1.0);
    ASSERT_TRUE(z.seconds == 0u);
    ASSERT_TRUE(z.fraction == 0u);
    return 0;
}
#endif

test_result_t test_cuc_run_all(void)
{
    RUN_TEST(test_pfield_single_octet);
    RUN_TEST(test_pfield_extended);
    RUN_TEST(test_tfield_roundtrip);
    RUN_TEST(test_full_roundtrip);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_format_validate_errors);
    RUN_TEST(test_size_helpers);
    RUN_TEST(test_pfield_encode_errors);
    RUN_TEST(test_pfield_decode_errors);
    RUN_TEST(test_tfield_encode_errors);
    RUN_TEST(test_tfield_decode_errors);
    RUN_TEST(test_wide_fraction_roundtrip);
    RUN_TEST(test_encode_decode_errors);
#ifndef CUC_NO_FLOAT
    RUN_TEST(test_seconds_conversion);
    RUN_TEST(test_seconds_conversion_edge);
#endif

    test_result_t r;
    r.total = cunit_total_tests;
    r.passed = cunit_total_tests - cunit_overall_failures;
    return r;
}
