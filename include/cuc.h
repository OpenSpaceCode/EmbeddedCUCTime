/**
 * @file    cuc.h
 * @brief   CCSDS Unsegmented Time Code (CUC) encoder/decoder
 *
 * Implements the CCSDS Unsegmented Time Code (CUC) as per
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.2.
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

/** @brief Minimum number of basic-time (seconds) octets (CCSDS 301.0-B-4 §3.2.2). */
#define CUC_BASIC_OCTETS_MIN 1

/** @brief Maximum number of basic-time octets: 4 (P-field octet 1) + 3 (P-field octet 2). */
#define CUC_BASIC_OCTETS_MAX 7

/** @brief Minimum number of fractional-time octets (CCSDS 301.0-B-4 §3.2.2). */
#define CUC_FRACTION_OCTETS_MIN 0

/** @brief Maximum number of fractional-time octets: 3 (P-field octet 1) + 7 (P-field octet 2). */
#define CUC_FRACTION_OCTETS_MAX 10

/** @brief Maximum number of P-field (preamble) octets. */
#define CUC_PFIELD_OCTETS_MAX 2

/** @brief Maximum number of T-field octets: basic + fractional (17). */
#define CUC_TFIELD_OCTETS_MAX (CUC_BASIC_OCTETS_MAX + CUC_FRACTION_OCTETS_MAX)

/** @brief Maximum octets of a self-identified code: P-field + T-field (19). */
#define CUC_OCTETS_MAX (CUC_PFIELD_OCTETS_MAX + CUC_TFIELD_OCTETS_MAX)

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief Result codes returned by the CUC functions.
 */
typedef enum
{
    CUC_OK = 0,          /**< Success. */
    CUC_ERR_NULL,        /**< A required pointer argument was NULL. */
    CUC_ERR_BUFFER,      /**< The supplied buffer was too small. */
    CUC_ERR_FORMAT,      /**< The format has out-of-range octet counts. */
    CUC_ERR_PFIELD_ID,   /**< The P-field time code identification is not a CUC id. */
    CUC_ERR_UNSUPPORTED, /**< The P-field requests more octets than this library supports. */
} cuc_status_t;

/**
 * @brief Epoch / time code identification of a CUC code (CCSDS 301.0-B-4 §3.2.2).
 *
 * The enumerator values equal the P-field time code identification field
 * (bits 1-3) so they map directly onto the encoded octet.
 */
typedef enum
{
    CUC_EPOCH_CCSDS = 1,  /**< 1958 January 1 (TAI), Level 1 time code (id = 001). */
    CUC_EPOCH_AGENCY = 2, /**< Agency-defined epoch, Level 2 time code (id = 010). */
} cuc_epoch_t;

/**
 * @brief Structure of a CUC time code as described by its P-field (CCSDS 301.0-B-4 §3.2.2).
 *
 * Captures how many octets encode the integer seconds and how many encode the
 * binary fraction, plus which epoch is in use.
 */
typedef struct
{
    cuc_epoch_t epoch;       /**< Epoch / time code identification. */
    uint8_t basic_octets;    /**< Octets of basic time unit (seconds), 1..7. */
    uint8_t fraction_octets; /**< Octets of fractional time unit, 0..10. */
} cuc_format_t;

/**
 * @brief A decoded CUC time value, independent of the encoded format.
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
    uint64_t seconds;  /**< Integer basic time units (seconds) since the epoch. */
    uint64_t fraction; /**< Binary fraction of a second, Q0.64. */
} cuc_time_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Validate that a format has octet counts within the ranges the standard allows.
 *
 * @param[in] fmt Format to validate.
 *
 * @return #CUC_OK when valid; #CUC_ERR_NULL if @p fmt is NULL; #CUC_ERR_FORMAT if the
 *         epoch or octet counts are out of range.
 */
cuc_status_t cuc_format_validate(const cuc_format_t *fmt);

/**
 * @brief Number of P-field octets a format needs (1 or 2).
 *
 * @param[in] fmt Format to size.
 *
 * @return 1 or 2 on success, or 0 if @p fmt is NULL or invalid.
 */
size_t cuc_pfield_size(const cuc_format_t *fmt);

/**
 * @brief Number of T-field octets a format needs (basic + fractional).
 *
 * @param[in] fmt Format to size.
 *
 * @return T-field length in octets, or 0 if @p fmt is NULL or invalid.
 */
size_t cuc_tfield_size(const cuc_format_t *fmt);

/**
 * @brief Total octets of a self-identified code (P-field + T-field).
 *
 * @param[in] fmt Format to size.
 *
 * @return Total length in octets, or 0 if @p fmt is NULL or invalid.
 */
size_t cuc_size(const cuc_format_t *fmt);

/**
 * @brief Encode the P-field (preamble) describing a format.
 *
 * @param[in]  fmt     Format to describe.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @param[out] written Receives the number of octets produced (1 or 2).
 *
 * @return #CUC_OK on success; #CUC_ERR_NULL if @p buf or @p written is NULL;
 *         #CUC_ERR_FORMAT for an invalid format; #CUC_ERR_BUFFER if @p buf_len is too small.
 */
cuc_status_t cuc_pfield_encode(const cuc_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written);

/**
 * @brief Decode a P-field into a format.
 *
 * @param[in]  buf      Input buffer positioned at the P-field.
 * @param[in]  buf_len  Number of octets available in @p buf.
 * @param[out] fmt      Receives the decoded format.
 * @param[out] consumed Receives the number of octets read (1 or 2).
 *
 * @return #CUC_OK on success; #CUC_ERR_NULL if any pointer is NULL; #CUC_ERR_BUFFER if
 *         @p buf_len is too small; #CUC_ERR_PFIELD_ID if the identification is not a CUC id;
 *         #CUC_ERR_UNSUPPORTED if the P-field requests a third octet.
 */
cuc_status_t cuc_pfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               cuc_format_t *fmt,
                               size_t *consumed);

/**
 * @brief Encode only the T-field (time data) for a value using a given format.
 *
 * Use this when the P-field is conveyed implicitly (external metadata).
 *
 * @param[in]  time    Time value to encode.
 * @param[in]  fmt     Format describing the octet layout.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @param[out] written Receives the number of octets produced.
 *
 * @return #CUC_OK on success; #CUC_ERR_NULL if @p time, @p buf or @p written is NULL;
 *         #CUC_ERR_FORMAT for an invalid format; #CUC_ERR_BUFFER if @p buf_len is too small.
 */
cuc_status_t cuc_tfield_encode(const cuc_time_t *time,
                               const cuc_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written);

/**
 * @brief Decode only the T-field using a caller-supplied format.
 *
 * @param[in]  buf      Input buffer positioned at the T-field.
 * @param[in]  buf_len  Number of octets available in @p buf.
 * @param[in]  fmt      Format describing the octet layout.
 * @param[out] time     Receives the decoded time value.
 * @param[out] consumed Receives the number of octets read.
 *
 * @return #CUC_OK on success; #CUC_ERR_NULL if @p buf, @p time or @p consumed is NULL;
 *         #CUC_ERR_FORMAT for an invalid format; #CUC_ERR_BUFFER if @p buf_len is too small.
 */
cuc_status_t cuc_tfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               const cuc_format_t *fmt,
                               cuc_time_t *time,
                               size_t *consumed);

/**
 * @brief Encode a self-identified CUC code: P-field followed by T-field.
 *
 * @note On failure the contents of @p buf are unspecified, since the P-field may
 *       already be written when the T-field stage rejects the value. Only a
 *       #CUC_OK return sets @p written, so a caller that checks the status never
 *       transmits a partial code.
 *
 * @param[in]  time    Time value to encode.
 * @param[in]  fmt     Format to encode.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @param[out] written Receives the total number of octets produced.
 *
 * @return #CUC_OK on success; #CUC_ERR_NULL if @p written or another required pointer is NULL;
 *         #CUC_ERR_FORMAT for an invalid format; #CUC_ERR_BUFFER if @p buf_len is too small.
 */
cuc_status_t cuc_encode(const cuc_time_t *time,
                        const cuc_format_t *fmt,
                        uint8_t *buf,
                        size_t buf_len,
                        size_t *written);

/**
 * @brief Decode a self-identified CUC code: parse the P-field, then the T-field.
 *
 * @param[in]  buf      Input buffer positioned at the P-field.
 * @param[in]  buf_len  Number of octets available in @p buf.
 * @param[out] fmt      Receives the recovered format.
 * @param[out] time     Receives the decoded time value.
 * @param[out] consumed Receives the total number of octets read.
 *
 * @return #CUC_OK on success; #CUC_ERR_NULL if @p consumed or another required pointer is NULL;
 *         #CUC_ERR_BUFFER if @p buf_len is too small; #CUC_ERR_PFIELD_ID or
 *         #CUC_ERR_UNSUPPORTED on an invalid P-field.
 */
cuc_status_t cuc_decode(const uint8_t *buf,
                        size_t buf_len,
                        cuc_format_t *fmt,
                        cuc_time_t *time,
                        size_t *consumed);

#ifndef CUC_NO_FLOAT
/**
 * @brief Convert a CUC time to seconds as a double.
 *
 * @note Compile with -DCUC_NO_FLOAT to omit this on targets without an FPU;
 *       the core codec does not use floating point.
 *
 * @param[in] time Time value to convert.
 *
 * @return Seconds since the epoch as a double, or 0.0 if @p time is NULL.
 */
double cuc_time_to_seconds(const cuc_time_t *time);

/**
 * @brief Convert seconds as a double to a CUC time.
 *
 * @note Compile with -DCUC_NO_FLOAT to omit this on targets without an FPU;
 *       the core codec does not use floating point.
 *
 * @param[in] seconds Seconds since the epoch; negative values yield a zero time.
 *
 * @return The equivalent CUC time value.
 */
cuc_time_t cuc_time_from_seconds(double seconds);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CUC_H */
