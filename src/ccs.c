/**
 * @file    ccs.c
 * @brief   CCSDS Calendar Segmented Time Code (CCS) encoder/decoder
 *
 * Implements the CCSDS Calendar Segmented Time Code (CCS) as per
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.4.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "ccs.h"

/* Bit-0-is-MSB masks for the P-field octet (CCSDS 301.0-B-4, 3.4.2). */
/** @brief P-field extension flag (bit 0): another P-field octet follows. */
#define CCS_P_EXTENSION 0x80u

/** @brief Left shift of the P-field time code identification field (bits 1-3). */
#define CCS_P_ID_SHIFT 4

/** @brief Mask for the 3-bit time code identification field. */
#define CCS_P_ID_MASK 0x07u

/** @brief Time code identification value denoting CCS (binary 101). */
#define CCS_P_ID_VALUE 0x05u

/** @brief Left shift of the P-field calendar variation flag (bit 4). */
#define CCS_P_VARIATION_SHIFT 3

/** @brief Mask for the 1-bit calendar variation flag. */
#define CCS_P_VARIATION_MASK 0x01u

/** @brief Mask for the 3-bit resolution field (bits 5-7), holding the sub-second segment count. */
#define CCS_P_RESOLUTION_MASK 0x07u

/** @brief Resolution pattern '111', which the standard leaves unused. */
#define CCS_P_RESOLUTION_UNUSED 0x07u

/** @brief Octets in the date segments: two for the year and two for the calendar variation. */
#define CCS_DATE_OCTETS 4

/** @brief Mask isolating one BCD digit (a nibble). */
#define CCS_BCD_DIGIT_MASK 0x0Fu

/** @brief Highest value a BCD digit carries. */
#define CCS_BCD_DIGIT_MAX 9u

/** @brief Mask for the four unused most significant bits of the day-of-year segment. */
#define CCS_DOY_UNUSED_MASK 0xF0u

/**
 * @brief Pack two decimal digits into one BCD octet.
 *
 * @param[in] value Value to pack, assumed to be 0..99.
 *
 * @return The octet holding the tens digit in its high nibble and the units digit in its low one.
 */
static uint8_t ccs_bcd_encode(uint8_t value)
{
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

/**
 * @brief Unpack one BCD octet into the value of its two decimal digits.
 *
 * @param[in]  octet Octet to unpack.
 * @param[out] value Receives the value 0..99; untouched when a nibble is not a decimal digit.
 *
 * @return #CCS_OK on success, or #CCS_ERR_BCD if either nibble exceeds 9.
 */
static ccs_status_t ccs_bcd_decode(uint8_t octet, uint8_t *value)
{
    uint8_t tens = (uint8_t)(octet >> 4);
    uint8_t units = (uint8_t)(octet & CCS_BCD_DIGIT_MASK);
    if ((tens > CCS_BCD_DIGIT_MAX) || (units > CCS_BCD_DIGIT_MAX))
    {
        return CCS_ERR_BCD;
    }

    *value = (uint8_t)((tens * 10u) + units);

    return CCS_OK;
}

bool ccs_is_leap_year(uint16_t year)
{
    return ((year % 4u) == 0u) && (((year % 100u) != 0u) || ((year % 400u) == 0u));
}

/**
 * @brief Length of a month in days, accounting for leap years.
 *
 * @param[in] year  Year A.D. the month falls in.
 * @param[in] month Month of year, assumed to have been range checked to 1..12.
 *
 * @return Days in the month.
 */
static uint8_t ccs_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[CCS_MONTH_MAX] =
        {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};

    if ((month == 2u) && ccs_is_leap_year(year))
    {
        return 29u;
    }

    return days[month - 1u];
}

ccs_status_t ccs_format_validate(const ccs_format_t *fmt)
{
    if (!fmt)
    {
        return CCS_ERR_NULL;
    }
    if ((fmt->variation != CCS_VARIATION_MONTH_DAY) &&
        (fmt->variation != CCS_VARIATION_DAY_OF_YEAR))
    {
        return CCS_ERR_FORMAT;
    }
    if (fmt->subsecond_segments > CCS_SUBSECOND_SEGMENTS_MAX)
    {
        return CCS_ERR_FORMAT;
    }
    return CCS_OK;
}

/**
 * @brief Validate the calendar fields the variation puts on the wire.
 *
 * @param[in] time Time value to check (assumed non-NULL).
 * @param[in] fmt  Format deciding which date fields are in use (assumed non-NULL and valid).
 *
 * @return #CCS_OK when the date is a real calendar date; #CCS_ERR_FORMAT otherwise.
 */
static ccs_status_t ccs_date_validate(const ccs_time_t *time, const ccs_format_t *fmt)
{
    if ((time->year < CCS_YEAR_MIN) || (time->year > CCS_YEAR_MAX))
    {
        return CCS_ERR_FORMAT;
    }

    if (fmt->variation == CCS_VARIATION_DAY_OF_YEAR)
    {
        uint16_t days_in_year = ccs_is_leap_year(time->year) ? 366u : 365u;
        if ((time->day_of_year < 1u) || (time->day_of_year > days_in_year))
        {
            return CCS_ERR_FORMAT;
        }
        return CCS_OK;
    }

    if ((time->month < 1u) || (time->month > CCS_MONTH_MAX))
    {
        return CCS_ERR_FORMAT;
    }
    if ((time->day < 1u) || (time->day > ccs_days_in_month(time->year, time->month)))
    {
        return CCS_ERR_FORMAT;
    }
    return CCS_OK;
}

ccs_status_t ccs_time_validate(const ccs_time_t *time, const ccs_format_t *fmt)
{
    ccs_status_t status = ccs_format_validate(fmt);
    if (status != CCS_OK)
    {
        return status;
    }
    if (!time)
    {
        return CCS_ERR_NULL;
    }

    status = ccs_date_validate(time, fmt);
    if (status != CCS_OK)
    {
        return status;
    }

    if ((time->hour > CCS_HOUR_MAX) || (time->minute > CCS_MINUTE_MAX) ||
        (time->second > CCS_SECOND_MAX))
    {
        return CCS_ERR_FORMAT;
    }

    for (uint8_t i = 0; i < fmt->subsecond_segments; i++)
    {
        if (time->subseconds[i] > CCS_SUBSECOND_MAX)
        {
            return CCS_ERR_FORMAT;
        }
    }
    return CCS_OK;
}

size_t ccs_tfield_size(const ccs_format_t *fmt)
{
    if (ccs_format_validate(fmt) != CCS_OK)
    {
        return 0;
    }
    return (size_t)CCS_TFIELD_OCTETS_MIN + fmt->subsecond_segments;
}

size_t ccs_size(const ccs_format_t *fmt)
{
    size_t tfield = ccs_tfield_size(fmt);
    if (tfield == 0)
    {
        return 0;
    }
    return CCS_PFIELD_OCTETS + tfield;
}

ccs_status_t ccs_pfield_encode(const ccs_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written)
{
    ccs_status_t status = ccs_format_validate(fmt);
    if (status != CCS_OK)
    {
        return status;
    }
    if ((!buf) || (!written))
    {
        return CCS_ERR_NULL;
    }
    if (buf_len < CCS_PFIELD_OCTETS)
    {
        return CCS_ERR_BUFFER;
    }

    buf[0] = (uint8_t)((CCS_P_ID_VALUE << CCS_P_ID_SHIFT) |
                       (((uint8_t)fmt->variation & CCS_P_VARIATION_MASK) << CCS_P_VARIATION_SHIFT) |
                       (fmt->subsecond_segments & CCS_P_RESOLUTION_MASK));
    *written = CCS_PFIELD_OCTETS;
    return CCS_OK;
}

ccs_status_t ccs_pfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               ccs_format_t *fmt,
                               size_t *consumed)
{
    if ((!buf) || (!fmt) || (!consumed))
    {
        return CCS_ERR_NULL;
    }
    if (buf_len < CCS_PFIELD_OCTETS)
    {
        return CCS_ERR_BUFFER;
    }

    uint8_t octet = buf[0];
    if (((octet >> CCS_P_ID_SHIFT) & CCS_P_ID_MASK) != CCS_P_ID_VALUE)
    {
        return CCS_ERR_PFIELD_ID;
    }

    /* A second P-field octet would be signalled here; CCS defines only one. */
    if ((octet & CCS_P_EXTENSION) != 0u)
    {
        return CCS_ERR_UNSUPPORTED;
    }

    uint8_t resolution = (uint8_t)(octet & CCS_P_RESOLUTION_MASK);
    /* Bits 5-7 = '111' is not used (CCSDS 301.0-B-4, 3.4.2). */
    if (resolution == CCS_P_RESOLUTION_UNUSED)
    {
        return CCS_ERR_FORMAT;
    }

    fmt->variation = (ccs_variation_t)((octet >> CCS_P_VARIATION_SHIFT) & CCS_P_VARIATION_MASK);
    fmt->subsecond_segments = resolution;
    *consumed = CCS_PFIELD_OCTETS;
    return CCS_OK;
}

/**
 * @brief Write the four date octets: the year, then the segments of the calendar variation.
 *
 * @param[out] buf  Output buffer positioned at the year segment, holding at least 4 octets.
 * @param[in]  time Time value to encode (assumed non-NULL and in range).
 * @param[in]  fmt  Format deciding which date fields are written (assumed non-NULL and valid).
 */
static void ccs_encode_date(uint8_t *buf, const ccs_time_t *time, const ccs_format_t *fmt)
{
    buf[0] = ccs_bcd_encode((uint8_t)(time->year / 100u));
    buf[1] = ccs_bcd_encode((uint8_t)(time->year % 100u));

    if (fmt->variation == CCS_VARIATION_DAY_OF_YEAR)
    {
        /* The four most significant bits of the segment are unused and set to zero (3.4.1.2). */
        buf[2] = (uint8_t)(time->day_of_year / 100u);
        buf[3] = ccs_bcd_encode((uint8_t)(time->day_of_year % 100u));
        return;
    }

    buf[2] = ccs_bcd_encode(time->month);
    buf[3] = ccs_bcd_encode(time->day);
}

/**
 * @brief Write the three time-of-day octets: hour, minute and second.
 *
 * @param[out] buf  Output buffer positioned at the hour segment, holding at least 3 octets.
 * @param[in]  time Time value to encode (assumed non-NULL and in range).
 */
static void ccs_encode_time_of_day(uint8_t *buf, const ccs_time_t *time)
{
    buf[0] = ccs_bcd_encode(time->hour);
    buf[1] = ccs_bcd_encode(time->minute);
    buf[2] = ccs_bcd_encode(time->second);
}

ccs_status_t ccs_tfield_encode(const ccs_time_t *time,
                               const ccs_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written)
{
    ccs_status_t status = ccs_format_validate(fmt);
    if (status != CCS_OK)
    {
        return status;
    }
    if ((!time) || (!buf) || (!written))
    {
        return CCS_ERR_NULL;
    }

    size_t size = ccs_tfield_size(fmt);
    if (buf_len < size)
    {
        return CCS_ERR_BUFFER;
    }
    status = ccs_time_validate(time, fmt);
    if (status != CCS_OK)
    {
        return status;
    }

    ccs_encode_date(buf, time, fmt);
    ccs_encode_time_of_day(&buf[CCS_DATE_OCTETS], time);
    for (uint8_t i = 0; i < fmt->subsecond_segments; i++)
    {
        buf[CCS_TFIELD_OCTETS_MIN + i] = ccs_bcd_encode(time->subseconds[i]);
    }

    *written = size;
    return CCS_OK;
}

/**
 * @brief Read the day-of-year segment, whose four most significant bits are unused.
 *
 * @param[in]  buf  Input buffer positioned at the day-of-year segment, holding at least 2 octets.
 * @param[out] time Receives the day of year; the month and day fields are cleared.
 *
 * @return #CCS_OK on success, or #CCS_ERR_BCD if the unused bits are set or a nibble exceeds 9.
 */
static ccs_status_t ccs_decode_day_of_year(const uint8_t *buf, ccs_time_t *time)
{
    uint8_t hundreds = (uint8_t)(buf[0] & CCS_BCD_DIGIT_MASK);
    if (((buf[0] & CCS_DOY_UNUSED_MASK) != 0u) || (hundreds > CCS_BCD_DIGIT_MAX))
    {
        return CCS_ERR_BCD;
    }

    uint8_t units = 0;
    ccs_status_t status = ccs_bcd_decode(buf[1], &units);
    if (status != CCS_OK)
    {
        return status;
    }

    time->month = 0;
    time->day = 0;
    time->day_of_year = (uint16_t)(((uint16_t)hundreds * 100u) + units);

    return CCS_OK;
}

/**
 * @brief Read the four date octets into the fields the calendar variation uses.
 *
 * @param[in]  buf  Input buffer positioned at the year segment, holding at least 4 octets.
 * @param[in]  fmt  Format deciding which date fields are read (assumed non-NULL and valid).
 * @param[out] time Receives the year and the date of the variation; unused fields are cleared.
 *
 * @return #CCS_OK on success, or #CCS_ERR_BCD if a segment does not hold valid BCD digits.
 */
static ccs_status_t ccs_decode_date(const uint8_t *buf, const ccs_format_t *fmt, ccs_time_t *time)
{
    uint8_t hundreds = 0;
    uint8_t units = 0;
    ccs_status_t status = ccs_bcd_decode(buf[0], &hundreds);
    if (status != CCS_OK)
    {
        return status;
    }
    status = ccs_bcd_decode(buf[1], &units);
    if (status != CCS_OK)
    {
        return status;
    }
    time->year = (uint16_t)(((uint16_t)hundreds * 100u) + units);

    if (fmt->variation == CCS_VARIATION_DAY_OF_YEAR)
    {
        return ccs_decode_day_of_year(&buf[2], time);
    }

    time->day_of_year = 0;
    status = ccs_bcd_decode(buf[2], &time->month);
    if (status != CCS_OK)
    {
        return status;
    }

    return ccs_bcd_decode(buf[3], &time->day);
}

/**
 * @brief Read the three time-of-day octets: hour, minute and second.
 *
 * @param[in]  buf  Input buffer positioned at the hour segment, holding at least 3 octets.
 * @param[out] time Receives the time of day.
 *
 * @return #CCS_OK on success, or #CCS_ERR_BCD if a segment does not hold valid BCD digits.
 */
static ccs_status_t ccs_decode_time_of_day(const uint8_t *buf, ccs_time_t *time)
{
    ccs_status_t status = ccs_bcd_decode(buf[0], &time->hour);
    if (status != CCS_OK)
    {
        return status;
    }
    status = ccs_bcd_decode(buf[1], &time->minute);
    if (status != CCS_OK)
    {
        return status;
    }

    return ccs_bcd_decode(buf[2], &time->second);
}

/**
 * @brief Read the optional sub-second segments the format announces.
 *
 * @param[in]  buf  Input buffer positioned at the first sub-second segment.
 * @param[in]  fmt  Format giving the number of segments (assumed non-NULL and valid).
 * @param[out] time Receives the segments; the entries the format does not use are cleared.
 *
 * @return #CCS_OK on success, or #CCS_ERR_BCD if a segment does not hold valid BCD digits.
 */
static ccs_status_t ccs_decode_subseconds(const uint8_t *buf,
                                          const ccs_format_t *fmt,
                                          ccs_time_t *time)
{
    for (uint8_t i = 0; i < CCS_SUBSECOND_SEGMENTS_MAX; i++)
    {
        time->subseconds[i] = 0;
    }

    for (uint8_t i = 0; i < fmt->subsecond_segments; i++)
    {
        ccs_status_t status = ccs_bcd_decode(buf[i], &time->subseconds[i]);
        if (status != CCS_OK)
        {
            return status;
        }
    }

    return CCS_OK;
}

ccs_status_t ccs_tfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               const ccs_format_t *fmt,
                               ccs_time_t *time,
                               size_t *consumed)
{
    ccs_status_t status = ccs_format_validate(fmt);
    if (status != CCS_OK)
    {
        return status;
    }
    if ((!buf) || (!time) || (!consumed))
    {
        return CCS_ERR_NULL;
    }

    size_t size = ccs_tfield_size(fmt);
    if (buf_len < size)
    {
        return CCS_ERR_BUFFER;
    }

    /* Decode into a local so a malformed segment cannot leave *time half written. */
    ccs_time_t decoded = {0};
    status = ccs_decode_date(buf, fmt, &decoded);
    if (status != CCS_OK)
    {
        return status;
    }
    status = ccs_decode_time_of_day(&buf[CCS_DATE_OCTETS], &decoded);
    if (status != CCS_OK)
    {
        return status;
    }
    status = ccs_decode_subseconds(&buf[CCS_TFIELD_OCTETS_MIN], fmt, &decoded);
    if (status != CCS_OK)
    {
        return status;
    }

    *time = decoded;
    *consumed = size;
    return CCS_OK;
}

ccs_status_t ccs_encode(const ccs_time_t *time,
                        const ccs_format_t *fmt,
                        uint8_t *buf,
                        size_t buf_len,
                        size_t *written)
{
    if (!written)
    {
        return CCS_ERR_NULL;
    }

    size_t p_len = 0;
    ccs_status_t status = ccs_pfield_encode(fmt, buf, buf_len, &p_len);
    if (status != CCS_OK)
    {
        return status;
    }

    size_t t_len = 0;
    status = ccs_tfield_encode(time, fmt, buf + p_len, buf_len - p_len, &t_len);
    if (status != CCS_OK)
    {
        return status;
    }

    *written = p_len + t_len;
    return CCS_OK;
}

ccs_status_t ccs_decode(const uint8_t *buf,
                        size_t buf_len,
                        ccs_format_t *fmt,
                        ccs_time_t *time,
                        size_t *consumed)
{
    if (!consumed)
    {
        return CCS_ERR_NULL;
    }

    size_t p_len = 0;
    ccs_status_t status = ccs_pfield_decode(buf, buf_len, fmt, &p_len);
    if (status != CCS_OK)
    {
        return status;
    }

    size_t t_len = 0;
    status = ccs_tfield_decode(buf + p_len, buf_len - p_len, fmt, time, &t_len);
    if (status != CCS_OK)
    {
        return status;
    }

    *consumed = p_len + t_len;
    return CCS_OK;
}
