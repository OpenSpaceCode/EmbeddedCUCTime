/**
 * @file    cuc.h
 * @brief   CCSDS Unsegmented Time Code (CUC) encoder/decoder
 *
 * Implements the CCSDS Unsegmented Time Code (CUC) as per
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.2.
 * See also: docs/ccsds_cuc.md
 *
 * A CUC time code is a pure binary count of a basic time unit (the second)
 * and a binary fraction of that unit, measured from a defined epoch. It is
 * carried in a TIME SPECIFICATION FIELD (T-field) that may be preceded by an
 * explicit TIME CODE PREAMBLE FIELD (P-field) describing its structure.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CUC_H
#define CUC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* -------------------------------------------------------------------------
 * Constants
 *
 * The P-field can describe up to 4 octets of basic time in octet 1 plus 3
 * more in octet 2, and up to 3 octets of fractional time in octet 1 plus 7
 * more in octet 2 (CCSDS 301.0-B-4, 3.2.2).
 * ---------------------------------------------------------------------- */

#define CUC_BASIC_OCTETS_MIN 1
#define CUC_BASIC_OCTETS_MAX 7 /* 4 (P-field octet 1) + 3 (P-field octet 2) */
#define CUC_FRACTION_OCTETS_MIN 0
#define CUC_FRACTION_OCTETS_MAX 10 /* 3 (P-field octet 1) + 7 (P-field octet 2) */

#define CUC_PFIELD_OCTETS_MAX 2
#define CUC_TFIELD_OCTETS_MAX (CUC_BASIC_OCTETS_MAX + CUC_FRACTION_OCTETS_MAX) /* 17 */
#define CUC_OCTETS_MAX (CUC_PFIELD_OCTETS_MAX + CUC_TFIELD_OCTETS_MAX)         /* 19 */

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * Result codes returned by the CUC functions.
 */
typedef enum
{
    CUC_OK = 0,          /* success */
    CUC_ERR_NULL,        /* a required pointer argument was NULL */
    CUC_ERR_BUFFER,      /* the supplied buffer was too small */
    CUC_ERR_FORMAT,      /* the format has out-of-range octet counts */
    CUC_ERR_PFIELD_ID,   /* the P-field time code identification is not a CUC id */
    CUC_ERR_UNSUPPORTED, /* the P-field requests more octets than this library supports */
} cuc_status_t;

/**
 * Epoch / time-code-identification of a CUC code.
 *
 * The enumerator values equal the P-field time code identification field
 * (bits 1-3) so they map directly onto the encoded octet.
 */
typedef enum
{
    CUC_EPOCH_CCSDS = 1,  /* 1958 January 1 (TAI), Level 1 time code (id = 001) */
    CUC_EPOCH_AGENCY = 2, /* Agency-defined epoch, Level 2 time code (id = 010) */
} cuc_epoch_t;

/**
 * Structure of a CUC time code: how many octets encode the integer seconds
 * and how many encode the binary fraction, plus which epoch is in use.
 */
typedef struct
{
    cuc_epoch_t epoch;       /* epoch / time code identification */
    uint8_t basic_octets;    /* octets of basic time unit (seconds), 1..7 */
    uint8_t fraction_octets; /* octets of fractional time unit, 0..10 */
} cuc_format_t;

/**
 * A decoded CUC time value, independent of the encoded format.
 *
 * The fraction is stored as an unsigned Q0.64 binary fraction of a second,
 * i.e. the fractional part in seconds equals fraction / 2^64. This keeps the
 * core codec integer-only and lets the same value be encoded into any
 * fractional resolution. Encodings using more than 8 fractional octets carry
 * resolution below 2^-64 s, which is not representable here and is treated as
 * zero in those low octets.
 */
typedef struct
{
    uint64_t seconds;  /* integer basic time units (seconds) since the epoch */
    uint64_t fraction; /* binary fraction of a second, Q0.64 */
} cuc_time_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * Validate that a format has octet counts within the ranges the standard
 * allows. Returns CUC_OK when valid, CUC_ERR_FORMAT otherwise.
 */
cuc_status_t cuc_format_validate(const cuc_format_t *fmt);

/** Number of P-field octets a format needs (1 or 2). */
size_t cuc_pfield_size(const cuc_format_t *fmt);

/** Number of T-field octets a format needs (basic + fractional). */
size_t cuc_tfield_size(const cuc_format_t *fmt);

/** Total octets of a self-identified code (P-field + T-field). */
size_t cuc_size(const cuc_format_t *fmt);

/**
 * Encode the P-field (preamble) describing @p fmt into @p buf.
 * @p written receives the number of octets produced (1 or 2).
 */
cuc_status_t cuc_pfield_encode(const cuc_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written);

/**
 * Decode a P-field from @p buf into @p fmt.
 * @p consumed receives the number of octets read (1 or 2).
 */
cuc_status_t cuc_pfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               cuc_format_t *fmt,
                               size_t *consumed);

/**
 * Encode only the T-field (time data) for @p time using @p fmt into @p buf.
 * Use this when the P-field is conveyed implicitly (external metadata).
 * @p written receives the number of octets produced.
 */
cuc_status_t cuc_tfield_encode(const cuc_time_t *time,
                               const cuc_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written);

/**
 * Decode only the T-field from @p buf using the caller-supplied @p fmt.
 * @p consumed receives the number of octets read.
 */
cuc_status_t cuc_tfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               const cuc_format_t *fmt,
                               cuc_time_t *time,
                               size_t *consumed);

/**
 * Encode a self-identified CUC code: P-field followed by T-field.
 * @p written receives the total number of octets produced.
 */
cuc_status_t cuc_encode(const cuc_time_t *time,
                        const cuc_format_t *fmt,
                        uint8_t *buf,
                        size_t buf_len,
                        size_t *written);

/**
 * Decode a self-identified CUC code: parse the P-field, then the T-field.
 * The recovered structure is written to @p fmt and the value to @p time.
 * @p consumed receives the total number of octets read.
 */
cuc_status_t cuc_decode(const uint8_t *buf,
                        size_t buf_len,
                        cuc_format_t *fmt,
                        cuc_time_t *time,
                        size_t *consumed);

#ifndef CUC_NO_FLOAT
/**
 * Convenience conversions between a CUC time and seconds as a double.
 * Compile with -DCUC_NO_FLOAT to omit these on targets without an FPU;
 * the core codec above does not use floating point.
 */
double cuc_time_to_seconds(const cuc_time_t *time);
cuc_time_t cuc_time_from_seconds(double seconds);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CUC_H */
