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

/** @brief Number of P-field (preamble) octets; CDS defines exactly one (CCSDS 301.0-B-4 §3.3.2). */
#define CDS_PFIELD_OCTETS 1

/** @brief Number of octets in the millisecond-of-day segment (32 bits). */
#define CDS_MS_OF_DAY_OCTETS 4

/** @brief Maximum number of T-field octets: 3 (day) + 4 (ms-of-day) + 4 (sub-ms). */
#define CDS_TFIELD_OCTETS_MAX 11

/** @brief Maximum octets of a self-identified code: P-field + T-field (12). */
#define CDS_OCTETS_MAX (CDS_PFIELD_OCTETS + CDS_TFIELD_OCTETS_MAX)

/**
 * @brief Maximum value of the millisecond-of-day counter (CCSDS 301.0-B-4 annex A).
 *
 * The nominal range is 0..86_399_999; a day carrying a positive leap second
 * reaches 86_400_999.
 */
#define CDS_MS_OF_DAY_MAX 86400999u

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief Result codes returned by the CDS functions.
 */
typedef enum
{
    CDS_OK = 0,        /**< Success. */
    CDS_ERR_NULL,      /**< A required pointer argument was NULL. */
    CDS_ERR_BUFFER,    /**< The supplied buffer was too small. */
    CDS_ERR_FORMAT,    /**< Invalid format, or a value does not fit its segment. */
    CDS_ERR_PFIELD_ID, /**< The P-field time code identification is not CDS. */
} cds_status_t;

/**
 * @brief Epoch / level of a CDS code (CCSDS 301.0-B-4 §3.3.2, P-field bit 4).
 *
 * The enumerator values equal the encoded bit so they map directly onto the
 * P-field octet.
 */
typedef enum
{
    CDS_EPOCH_CCSDS = 0,  /**< 1958 January 1, Level 1 time code (bit = 0). */
    CDS_EPOCH_AGENCY = 1, /**< Agency-defined epoch, Level 2 time code (bit = 1). */
} cds_epoch_t;

/**
 * @brief Length of the day segment (CCSDS 301.0-B-4 §3.3.2, P-field bit 5).
 *
 * The enumerator values equal the encoded bit so they map directly onto the
 * P-field octet.
 */
typedef enum
{
    CDS_DAY_16BIT = 0, /**< 16-bit day segment (2 octets). */
    CDS_DAY_24BIT = 1, /**< 24-bit day segment (3 octets). */
} cds_day_length_t;

/**
 * @brief Resolution / length of the sub-millisecond segment (CCSDS 301.0-B-4 §3.3.2, P-field bits
 *        6-7).
 *
 * The enumerator values equal the encoded 2-bit pattern; the pattern '11' is
 * reserved by the standard and has no enumerator here.
 */
typedef enum
{
    CDS_SUBMS_NONE = 0, /**< Absent: millisecond resolution. */
    CDS_SUBMS_US = 1,   /**< 16-bit microsecond-of-millisecond (0..999). */
    CDS_SUBMS_PS = 2,   /**< 32-bit picosecond-of-millisecond (0..999_999_999). */
} cds_subms_t;

/**
 * @brief Structure of a CDS time code as described by its P-field (CCSDS 301.0-B-4 §3.3.2).
 *
 * Captures the epoch in use, the width of the day segment and the resolution
 * of the optional sub-millisecond segment.
 */
typedef struct
{
    cds_epoch_t epoch;           /**< Epoch identification. */
    cds_day_length_t day_length; /**< 16- or 24-bit day segment. */
    cds_subms_t submillisecond;  /**< Sub-millisecond segment resolution. */
} cds_format_t;

/**
 * @brief A decoded CDS time value, independent of the encoded format.
 *
 * @note @p submilliseconds holds microseconds-of-millisecond (0..999) when the
 *       format is #CDS_SUBMS_US, or picoseconds-of-millisecond
 *       (0..999_999_999) when #CDS_SUBMS_PS. It is unused (0) for
 *       #CDS_SUBMS_NONE.
 */
typedef struct
{
    uint32_t days;            /**< Whole days since the epoch. */
    uint32_t ms_of_day;       /**< Milliseconds of day, 0..#CDS_MS_OF_DAY_MAX. */
    uint32_t submilliseconds; /**< Sub-millisecond remainder (see note above). */
} cds_time_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Validate that a format selects only defined field lengths and resolutions.
 *
 * @param[in] fmt Format to validate.
 *
 * @return #CDS_OK when valid; #CDS_ERR_NULL if @p fmt is NULL; #CDS_ERR_FORMAT if the epoch,
 *         day length or sub-millisecond resolution is not a defined value.
 */
cds_status_t cds_format_validate(const cds_format_t *fmt);

/**
 * @brief Number of octets in the day segment (2 or 3).
 *
 * @param[in] fmt Format to size.
 *
 * @return 2 or 3 on success, or 0 if @p fmt is NULL or invalid.
 */
size_t cds_day_size(const cds_format_t *fmt);

/**
 * @brief Number of octets in the sub-millisecond segment (0, 2 or 4).
 *
 * @param[in] fmt Format to size.
 *
 * @return 0, 2 or 4 depending on the resolution, or 0 if @p fmt is NULL or invalid.
 */
size_t cds_subms_size(const cds_format_t *fmt);

/**
 * @brief Number of T-field octets a format needs (day + ms-of-day + sub-ms).
 *
 * @param[in] fmt Format to size.
 *
 * @return T-field length in octets, or 0 if @p fmt is NULL or invalid.
 */
size_t cds_tfield_size(const cds_format_t *fmt);

/**
 * @brief Total octets of a self-identified code (P-field + T-field).
 *
 * @param[in] fmt Format to size.
 *
 * @return Total length in octets, or 0 if @p fmt is NULL or invalid.
 */
size_t cds_size(const cds_format_t *fmt);

/**
 * @brief Encode the one-octet P-field (preamble) describing a format.
 *
 * @param[in]  fmt     Format to describe.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @param[out] written Receives the number of octets produced (always 1).
 *
 * @return #CDS_OK on success; #CDS_ERR_NULL if @p buf or @p written is NULL;
 *         #CDS_ERR_FORMAT for an invalid format; #CDS_ERR_BUFFER if @p buf_len is too small.
 */
cds_status_t cds_pfield_encode(const cds_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written);

/**
 * @brief Decode a P-field into a format.
 *
 * @param[in]  buf      Input buffer positioned at the P-field.
 * @param[in]  buf_len  Number of octets available in @p buf.
 * @param[out] fmt      Receives the decoded format.
 * @param[out] consumed Receives the number of octets read (always 1).
 *
 * @return #CDS_OK on success; #CDS_ERR_NULL if any pointer is NULL; #CDS_ERR_BUFFER if
 *         @p buf_len is too small; #CDS_ERR_PFIELD_ID if the identification is not the CDS id;
 *         #CDS_ERR_FORMAT if the sub-millisecond field holds the reserved pattern '11'.
 */
cds_status_t cds_pfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               cds_format_t *fmt,
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
 * @return #CDS_OK on success; #CDS_ERR_NULL if @p time, @p buf or @p written is NULL;
 *         #CDS_ERR_FORMAT for an invalid format or a value that does not fit its segment;
 *         #CDS_ERR_BUFFER if @p buf_len is too small.
 */
cds_status_t cds_tfield_encode(const cds_time_t *time,
                               const cds_format_t *fmt,
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
 * @return #CDS_OK on success; #CDS_ERR_NULL if @p buf, @p time or @p consumed is NULL;
 *         #CDS_ERR_FORMAT for an invalid format; #CDS_ERR_BUFFER if @p buf_len is too small.
 */
cds_status_t cds_tfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               const cds_format_t *fmt,
                               cds_time_t *time,
                               size_t *consumed);

/**
 * @brief Encode a self-identified CDS code: P-field followed by T-field.
 *
 * @note On failure the contents of @p buf are unspecified, since the P-field may
 *       already be written when the T-field stage rejects the value. Only a
 *       #CDS_OK return sets @p written, so a caller that checks the status never
 *       transmits a partial code.
 *
 * @param[in]  time    Time value to encode.
 * @param[in]  fmt     Format to encode.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @param[out] written Receives the total number of octets produced.
 *
 * @return #CDS_OK on success; #CDS_ERR_NULL if @p written or another required pointer is NULL;
 *         #CDS_ERR_FORMAT for an invalid format or an out-of-range value; #CDS_ERR_BUFFER if
 *         @p buf_len is too small.
 */
cds_status_t cds_encode(const cds_time_t *time,
                        const cds_format_t *fmt,
                        uint8_t *buf,
                        size_t buf_len,
                        size_t *written);

/**
 * @brief Decode a self-identified CDS code: parse the P-field, then the T-field.
 *
 * @param[in]  buf      Input buffer positioned at the P-field.
 * @param[in]  buf_len  Number of octets available in @p buf.
 * @param[out] fmt      Receives the recovered format.
 * @param[out] time     Receives the decoded time value.
 * @param[out] consumed Receives the total number of octets read.
 *
 * @return #CDS_OK on success; #CDS_ERR_NULL if @p consumed or another required pointer is NULL;
 *         #CDS_ERR_BUFFER if @p buf_len is too small; #CDS_ERR_PFIELD_ID or #CDS_ERR_FORMAT
 *         on an invalid P-field.
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
