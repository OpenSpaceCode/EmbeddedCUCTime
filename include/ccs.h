/**
 * @file    ccs.h
 * @brief   CCSDS Calendar Segmented Time Code (CCS) encoder/decoder
 *
 * Implements the CCSDS Calendar Segmented Time Code (CCS) as per
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.4.
 *
 * A CCS time code is a segmented binary coded decimal (BCD) time code: a
 * calendar date, a time of day, and an optional run of sub-second segments,
 * every octet carrying two decimal digits. The date is written either as
 * month-of-year plus day-of-month or as day-of-year, selected by the calendar
 * variation flag. It is UTC-based, so leap-second corrections apply. The value
 * is carried in a TIME SPECIFICATION FIELD (T-field) preceded by a one-octet
 * TIME CODE PREAMBLE FIELD (P-field) describing its structure.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CCS_H
#define CCS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

/** @brief Number of P-field (preamble) octets; CCS defines exactly one (CCSDS 301.0-B-4 §3.4.2). */
#define CCS_PFIELD_OCTETS 1

/** @brief Maximum number of sub-second segments, i.e. the finest resolution of 10^-12 s. */
#define CCS_SUBSECOND_SEGMENTS_MAX 6

/** @brief Mandatory T-field octets: 4 date (year + calendar) + 3 time of day (h, m, s). */
#define CCS_TFIELD_OCTETS_MIN 7

/** @brief Maximum number of T-field octets: mandatory (7) plus every sub-second segment (6). */
#define CCS_TFIELD_OCTETS_MAX (CCS_TFIELD_OCTETS_MIN + CCS_SUBSECOND_SEGMENTS_MAX)

/** @brief Maximum octets of a self-identified code: P-field + T-field (14). */
#define CCS_OCTETS_MAX (CCS_PFIELD_OCTETS + CCS_TFIELD_OCTETS_MAX)

/** @brief Lowest year the four-digit BCD year segment carries (CCSDS 301.0-B-4 annex A). */
#define CCS_YEAR_MIN 1

/** @brief Highest year the four-digit BCD year segment carries (CCSDS 301.0-B-4 annex A). */
#define CCS_YEAR_MAX 9999

/** @brief Highest month-of-year (CCSDS 301.0-B-4 annex A). */
#define CCS_MONTH_MAX 12

/** @brief Highest hour-of-day (CCSDS 301.0-B-4 annex A). */
#define CCS_HOUR_MAX 23

/** @brief Highest minute-of-hour (CCSDS 301.0-B-4 annex A). */
#define CCS_MINUTE_MAX 59

/**
 * @brief Highest second-of-minute (CCSDS 301.0-B-4 annex A).
 *
 * The nominal range is 0..59; a minute carrying a positive leap second reaches
 * 60.
 */
#define CCS_SECOND_MAX 60

/** @brief Highest value a sub-second segment carries, being two decimal digits. */
#define CCS_SUBSECOND_MAX 99

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief Result codes returned by the CCS functions.
 */
typedef enum
{
    CCS_OK = 0,          /**< Success. */
    CCS_ERR_NULL,        /**< A required pointer argument was NULL. */
    CCS_ERR_BUFFER,      /**< The supplied buffer was too small. */
    CCS_ERR_FORMAT,      /**< Invalid format, or a value outside the range of its segment. */
    CCS_ERR_PFIELD_ID,   /**< The P-field time code identification is not CCS. */
    CCS_ERR_UNSUPPORTED, /**< The P-field requests more octets than this library supports. */
    CCS_ERR_BCD,         /**< An encoded segment does not hold valid BCD digits. */
} ccs_status_t;

/**
 * @brief Calendar variation of the date segments (CCSDS 301.0-B-4 §3.4.2, P-field bit 4).
 *
 * The enumerator values equal the encoded bit so they map directly onto the
 * P-field octet.
 */
typedef enum
{
    CCS_VARIATION_MONTH_DAY = 0,   /**< Month-of-year and day-of-month segments (bit = 0). */
    CCS_VARIATION_DAY_OF_YEAR = 1, /**< Day-of-year segment (bit = 1). */
} ccs_variation_t;

/**
 * @brief Structure of a CCS time code as described by its P-field (CCSDS 301.0-B-4 §3.4.2).
 *
 * Captures which calendar variation writes the date and how many sub-second
 * segments follow the seconds segment.
 */
typedef struct
{
    ccs_variation_t variation;  /**< Calendar variation of the date segments. */
    uint8_t subsecond_segments; /**< Sub-second segments, 0..6; segment n has weight 10^-2n s. */
} ccs_format_t;

/**
 * @brief A decoded CCS time value, independent of the encoded format.
 *
 * @note @p month and @p day carry the date under #CCS_VARIATION_MONTH_DAY, while
 *       @p day_of_year carries it under #CCS_VARIATION_DAY_OF_YEAR. The fields
 *       the variation does not use are ignored on encoding and set to 0 on
 *       decoding.
 *
 * @note @p subseconds holds one two-digit segment per element: element 0 counts
 *       hundredths of a second, element 1 units of 10^-4 s, and so on down to
 *       10^-12 s. Only the first @c subsecond_segments elements of the format
 *       are used; the rest are ignored on encoding and set to 0 on decoding.
 */
typedef struct
{
    uint16_t year;        /**< Year A.D., 1..9999. */
    uint8_t month;        /**< Month of year, 1..12 (month/day variation). */
    uint8_t day;          /**< Day of month, 1..31 by month (month/day variation). */
    uint16_t day_of_year; /**< Day of year, 1..365 or 366 (day-of-year variation). */
    uint8_t hour;         /**< Hour of day, 0..23. */
    uint8_t minute;       /**< Minute of hour, 0..59. */
    uint8_t second;       /**< Second of minute, 0..60 with a leap second. */
    uint8_t subseconds[CCS_SUBSECOND_SEGMENTS_MAX]; /**< Sub-second segments, each 0..99. */
} ccs_time_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Test whether a year is a leap year (CCSDS 301.0-B-4 annex A).
 *
 * Every year divisible by 4 is a leap year, except years divisible by 100 and
 * not divisible by 400.
 *
 * @param[in] year Year A.D. to test.
 *
 * @return true when @p year is a leap year.
 */
bool ccs_is_leap_year(uint16_t year);

/**
 * @brief Validate that a format selects a defined calendar variation and resolution.
 *
 * @param[in] fmt Format to validate.
 *
 * @return #CCS_OK when valid; #CCS_ERR_NULL if @p fmt is NULL; #CCS_ERR_FORMAT if the calendar
 *         variation is not a defined value or more than #CCS_SUBSECOND_SEGMENTS_MAX sub-second
 *         segments are requested.
 */
ccs_status_t ccs_format_validate(const ccs_format_t *fmt);

/**
 * @brief Validate that a time value lies within the segment ranges of the standard.
 *
 * Checks the calendar fields the variation uses, including the length of the
 * month and the leap-year length of the year, plus the time of day and every
 * sub-second segment the format encodes.
 *
 * @param[in] time Time value to validate.
 * @param[in] fmt  Format deciding which fields are in use.
 *
 * @return #CCS_OK when every field is in range; #CCS_ERR_NULL if @p time or @p fmt is NULL;
 *         #CCS_ERR_FORMAT for an invalid format or an out-of-range field.
 */
ccs_status_t ccs_time_validate(const ccs_time_t *time, const ccs_format_t *fmt);

/**
 * @brief Number of T-field octets a format needs (date + time of day + sub-second segments).
 *
 * @param[in] fmt Format to size.
 *
 * @return T-field length in octets, or 0 if @p fmt is NULL or invalid.
 */
size_t ccs_tfield_size(const ccs_format_t *fmt);

/**
 * @brief Total octets of a self-identified code (P-field + T-field).
 *
 * @param[in] fmt Format to size.
 *
 * @return Total length in octets, or 0 if @p fmt is NULL or invalid.
 */
size_t ccs_size(const ccs_format_t *fmt);

/**
 * @brief Encode the one-octet P-field (preamble) describing a format.
 *
 * @param[in]  fmt     Format to describe.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @param[out] written Receives the number of octets produced (always 1).
 *
 * @return #CCS_OK on success; #CCS_ERR_NULL if @p buf or @p written is NULL;
 *         #CCS_ERR_FORMAT for an invalid format; #CCS_ERR_BUFFER if @p buf_len is too small.
 */
ccs_status_t ccs_pfield_encode(const ccs_format_t *fmt,
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
 * @return #CCS_OK on success; #CCS_ERR_NULL if any pointer is NULL; #CCS_ERR_BUFFER if
 *         @p buf_len is too small; #CCS_ERR_PFIELD_ID if the identification is not the CCS id;
 *         #CCS_ERR_FORMAT if the resolution field holds the unused pattern '111';
 *         #CCS_ERR_UNSUPPORTED if the extension flag announces a second P-field octet.
 */
ccs_status_t ccs_pfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               ccs_format_t *fmt,
                               size_t *consumed);

/**
 * @brief Encode only the T-field (time data) for a value using a given format.
 *
 * Use this when the P-field is conveyed implicitly (external metadata).
 *
 * @param[in]  time    Time value to encode; validated against the segment ranges first.
 * @param[in]  fmt     Format describing the octet layout.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @param[out] written Receives the number of octets produced.
 *
 * @return #CCS_OK on success; #CCS_ERR_NULL if @p time, @p buf or @p written is NULL;
 *         #CCS_ERR_FORMAT for an invalid format or a value outside the range of its segment;
 *         #CCS_ERR_BUFFER if @p buf_len is too small.
 */
ccs_status_t ccs_tfield_encode(const ccs_time_t *time,
                               const ccs_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written);

/**
 * @brief Decode only the T-field using a caller-supplied format.
 *
 * @note Every segment is checked for valid BCD digits, but the decoded value is
 *       not range checked against the standard: a well-formed code may still
 *       carry, say, month 19. Call ccs_time_validate() when the source is not
 *       trusted.
 *
 * @param[in]  buf      Input buffer positioned at the T-field.
 * @param[in]  buf_len  Number of octets available in @p buf.
 * @param[in]  fmt      Format describing the octet layout.
 * @param[out] time     Receives the decoded time value.
 * @param[out] consumed Receives the number of octets read.
 *
 * @return #CCS_OK on success; #CCS_ERR_NULL if @p buf, @p time or @p consumed is NULL;
 *         #CCS_ERR_FORMAT for an invalid format; #CCS_ERR_BUFFER if @p buf_len is too small;
 *         #CCS_ERR_BCD if a segment does not hold valid BCD digits.
 */
ccs_status_t ccs_tfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               const ccs_format_t *fmt,
                               ccs_time_t *time,
                               size_t *consumed);

/**
 * @brief Encode a self-identified CCS code: P-field followed by T-field.
 *
 * @param[in]  time    Time value to encode.
 * @param[in]  fmt     Format to encode.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @param[out] written Receives the total number of octets produced.
 *
 * @return #CCS_OK on success; #CCS_ERR_NULL if @p written or another required pointer is NULL;
 *         #CCS_ERR_FORMAT for an invalid format or an out-of-range value; #CCS_ERR_BUFFER if
 *         @p buf_len is too small.
 */
ccs_status_t ccs_encode(const ccs_time_t *time,
                        const ccs_format_t *fmt,
                        uint8_t *buf,
                        size_t buf_len,
                        size_t *written);

/**
 * @brief Decode a self-identified CCS code: parse the P-field, then the T-field.
 *
 * @param[in]  buf      Input buffer positioned at the P-field.
 * @param[in]  buf_len  Number of octets available in @p buf.
 * @param[out] fmt      Receives the recovered format.
 * @param[out] time     Receives the decoded time value.
 * @param[out] consumed Receives the total number of octets read.
 *
 * @return #CCS_OK on success; #CCS_ERR_NULL if @p consumed or another required pointer is NULL;
 *         #CCS_ERR_BUFFER if @p buf_len is too small; #CCS_ERR_PFIELD_ID, #CCS_ERR_FORMAT or
 *         #CCS_ERR_UNSUPPORTED on an invalid P-field; #CCS_ERR_BCD on a malformed segment.
 */
ccs_status_t ccs_decode(const uint8_t *buf,
                        size_t buf_len,
                        ccs_format_t *fmt,
                        ccs_time_t *time,
                        size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif /* CCS_H */
