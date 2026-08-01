/**
 * @file    cds.h
 * @brief   CCSDS Day Segmented Time Code (CDS) encoder/decoder
 *
 * Implements the CCSDS Day Segmented Time Code (CDS) as per
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.3.
 *
 * A CDS time code is a segmented binary time code: a day counter from a
 * defined epoch, a millisecond-of-day counter, and an optional sub-millisecond
 * segment. It is UTC-based, so leap-second corrections apply. The value is
 * carried in a TIME SPECIFICATION FIELD (T-field) preceded by a one-octet
 * TIME CODE PREAMBLE FIELD (P-field) describing its structure.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CDS_H
#define CDS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

#define CDS_PFIELD_OCTETS 1      /* the CDS P-field is a single octet */
#define CDS_MS_OF_DAY_OCTETS 4   /* the millisecond-of-day segment is 32 bits */
#define CDS_TFIELD_OCTETS_MAX 11 /* 3 (day) + 4 (ms) + 4 (sub-ms) */
#define CDS_OCTETS_MAX (CDS_PFIELD_OCTETS + CDS_TFIELD_OCTETS_MAX) /* 12 */

/* Nominal range of the millisecond-of-day counter is 0..86_399_999; a day with
 * a positive leap second reaches 86_400_999 (CCSDS 301.0-B-4, annex A). */
#define CDS_MS_OF_DAY_MAX 86400999u

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * Result codes returned by the CDS functions.
 */
typedef enum
{
    CDS_OK = 0,        /* success */
    CDS_ERR_NULL,      /* a required pointer argument was NULL */
    CDS_ERR_BUFFER,    /* the supplied buffer was too small */
    CDS_ERR_FORMAT,    /* invalid format, or a value does not fit its segment */
    CDS_ERR_PFIELD_ID, /* the P-field time code identification is not CDS */
} cds_status_t;

/**
 * Epoch / level of a CDS code (P-field bit 4).
 */
typedef enum
{
    CDS_EPOCH_CCSDS = 0,  /* 1958 January 1, Level 1 time code */
    CDS_EPOCH_AGENCY = 1, /* Agency-defined epoch, Level 2 time code */
} cds_epoch_t;

/**
 * Length of the day segment (P-field bit 5).
 */
typedef enum
{
    CDS_DAY_16BIT = 0, /* 16-bit day segment (2 octets) */
    CDS_DAY_24BIT = 1, /* 24-bit day segment (3 octets) */
} cds_day_length_t;

/**
 * Resolution / length of the sub-millisecond segment (P-field bits 6-7).
 */
typedef enum
{
    CDS_SUBMS_NONE = 0, /* absent: millisecond resolution */
    CDS_SUBMS_US = 1,   /* 16-bit microsecond-of-millisecond (0..999) */
    CDS_SUBMS_PS = 2,   /* 32-bit picosecond-of-millisecond (0..999_999_999) */
} cds_subms_t;

/**
 * Structure of a CDS time code, mirroring the P-field selections.
 */
typedef struct
{
    cds_epoch_t epoch;           /* epoch identification */
    cds_day_length_t day_length; /* 16- or 24-bit day segment */
    cds_subms_t submillisecond;  /* sub-millisecond segment resolution */
} cds_format_t;

/**
 * A decoded CDS time value.
 *
 * @c submilliseconds holds microseconds-of-millisecond (0..999) when the format
 * is CDS_SUBMS_US, or picoseconds-of-millisecond (0..999_999_999) when
 * CDS_SUBMS_PS. It is unused (0) for CDS_SUBMS_NONE.
 */
typedef struct
{
    uint32_t days;            /* whole days since the epoch */
    uint32_t ms_of_day;       /* milliseconds of day, 0..CDS_MS_OF_DAY_MAX */
    uint32_t submilliseconds; /* sub-millisecond remainder (see above) */
} cds_time_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * Validate that a format selects only defined field lengths and resolutions.
 * Returns CDS_OK when valid, CDS_ERR_FORMAT otherwise.
 */
cds_status_t cds_format_validate(const cds_format_t *fmt);

/** Number of octets in the day segment (2 or 3). */
size_t cds_day_size(const cds_format_t *fmt);

/** Number of octets in the sub-millisecond segment (0, 2 or 4). */
size_t cds_subms_size(const cds_format_t *fmt);

/** Number of T-field octets a format needs (day + ms-of-day + sub-ms). */
size_t cds_tfield_size(const cds_format_t *fmt);

/** Total octets of a self-identified code (P-field + T-field). */
size_t cds_size(const cds_format_t *fmt);

/**
 * Encode the one-octet P-field describing @p fmt into @p buf.
 * @p written receives the number of octets produced (always 1).
 */
cds_status_t cds_pfield_encode(const cds_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written);

/**
 * Decode a P-field from @p buf into @p fmt.
 * @p consumed receives the number of octets read (always 1).
 */
cds_status_t cds_pfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               cds_format_t *fmt,
                               size_t *consumed);

/**
 * Encode only the T-field (time data) for @p time using @p fmt into @p buf.
 * Use this when the P-field is conveyed implicitly (external metadata).
 * @p written receives the number of octets produced.
 */
cds_status_t cds_tfield_encode(const cds_time_t *time,
                               const cds_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written);

/**
 * Decode only the T-field from @p buf using the caller-supplied @p fmt.
 * @p consumed receives the number of octets read.
 */
cds_status_t cds_tfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               const cds_format_t *fmt,
                               cds_time_t *time,
                               size_t *consumed);

/**
 * Encode a self-identified CDS code: P-field followed by T-field.
 * @p written receives the total number of octets produced.
 */
cds_status_t cds_encode(const cds_time_t *time,
                        const cds_format_t *fmt,
                        uint8_t *buf,
                        size_t buf_len,
                        size_t *written);

/**
 * Decode a self-identified CDS code: parse the P-field, then the T-field.
 * The recovered structure is written to @p fmt and the value to @p time.
 * @p consumed receives the total number of octets read.
 */
cds_status_t cds_decode(const uint8_t *buf,
                        size_t buf_len,
                        cds_format_t *fmt,
                        cds_time_t *time,
                        size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif /* CDS_H */
