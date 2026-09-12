/**
 * @file    cuc.c
 * @brief   CCSDS Unsegmented Time Code (CUC) encoder/decoder
 *
 * Implements the CCSDS Unsegmented Time Code (CUC) as per
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.2.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cuc.h"

/* Bit-0-is-MSB masks for P-field octet 1 (CCSDS 301.0-B-4, 3.2.2). */
/** @brief P-field octet 1 extension flag (bit 0): another P-field octet follows. */
#define CUC_P1_EXTENSION 0x80u

/** @brief Left shift of the P-field octet 1 time code identification field (bits 1-3). */
#define CUC_P1_ID_SHIFT 4

/** @brief Mask for the 3-bit time code identification field. */
#define CUC_P1_ID_MASK 0x07u

/** @brief Left shift of the P-field octet 1 basic-octet count (bits 4-5), holding (count - 1). */
#define CUC_P1_BASIC_SHIFT 2

/** @brief Mask for the 2-bit basic-octet count field in P-field octet 1. */
#define CUC_P1_BASIC_MASK 0x03u

/** @brief Mask for the 2-bit fractional-octet count field in P-field octet 1 (bits 6-7). */
#define CUC_P1_FRAC_MASK 0x03u

/* Masks for P-field octet 2. */
/** @brief P-field octet 2 extension flag (bit 0): a third P-field octet would follow. */
#define CUC_P2_EXTENSION 0x80u

/** @brief Left shift of the additional basic-octet count in P-field octet 2 (bits 1-2). */
#define CUC_P2_ADD_BASIC_SHIFT 5

/** @brief Mask for the 2-bit additional basic-octet count field in P-field octet 2. */
#define CUC_P2_ADD_BASIC_MASK 0x03u

/** @brief Left shift of the additional fractional-octet count in P-field octet 2 (bits 3-5). */
#define CUC_P2_ADD_FRAC_SHIFT 2

/** @brief Mask for the 3-bit additional fractional-octet count field in P-field octet 2. */
#define CUC_P2_ADD_FRAC_MASK 0x07u

/** @brief Maximum basic-time octets encodable in P-field octet 1 alone. */
#define CUC_P1_BASIC_MAX 4

/** @brief Maximum fractional-time octets encodable in P-field octet 1 alone. */
#define CUC_P1_FRAC_MAX 3

/** @brief Fractional octets stored in the Q0.64 representation; deeper octets fall below 2^-64 s.
 */
#define CUC_FRACTION_STORED_OCTETS 8

cuc_status_t cuc_format_validate(const cuc_format_t *fmt)
{
    if (!fmt)
    {
        return CUC_ERR_NULL;
    }

    if ((fmt->epoch != CUC_EPOCH_CCSDS) && (fmt->epoch != CUC_EPOCH_AGENCY))
    {
        return CUC_ERR_FORMAT;
    }

    if ((fmt->basic_octets < CUC_BASIC_OCTETS_MIN) || (fmt->basic_octets > CUC_BASIC_OCTETS_MAX))
    {
        return CUC_ERR_FORMAT;
    }

    if (fmt->fraction_octets > CUC_FRACTION_OCTETS_MAX)
    {
        return CUC_ERR_FORMAT;
    }

    return CUC_OK;
}

/**
 * @brief Test whether a format needs a second P-field octet.
 *
 * @param[in] fmt Format to inspect (assumed non-NULL and valid).
 *
 * @return true if the basic or fractional octet count exceeds what P-field octet 1 can hold.
 */
static bool cuc_format_is_extended(const cuc_format_t *fmt)
{
    return (fmt->basic_octets > CUC_P1_BASIC_MAX) || (fmt->fraction_octets > CUC_P1_FRAC_MAX);
}

size_t cuc_pfield_size(const cuc_format_t *fmt)
{
    if (!fmt)
    {
        return 0;
    }

    return cuc_format_is_extended(fmt) ? 2u : 1u;
}

size_t cuc_tfield_size(const cuc_format_t *fmt)
{
    if (!fmt)
    {
        return 0;
    }

    return (size_t)fmt->basic_octets + (size_t)fmt->fraction_octets;
}

size_t cuc_size(const cuc_format_t *fmt)
{
    return cuc_pfield_size(fmt) + cuc_tfield_size(fmt);
}

cuc_status_t cuc_pfield_encode(const cuc_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written)
{
    cuc_status_t status = cuc_format_validate(fmt);
    if (status != CUC_OK)
    {
        return status;
    }

    if ((!buf) || (!written))
    {
        return CUC_ERR_NULL;
    }

    bool extended = cuc_format_is_extended(fmt);
    size_t size = extended ? 2u : 1u;
    if (buf_len < size)
    {
        return CUC_ERR_BUFFER;
    }

    uint8_t oct1_basic = extended ? CUC_P1_BASIC_MAX : fmt->basic_octets;
    uint8_t oct1_frac =
        fmt->fraction_octets > CUC_P1_FRAC_MAX ? CUC_P1_FRAC_MAX : fmt->fraction_octets;
    buf[0] = (uint8_t)((extended ? CUC_P1_EXTENSION : 0u) |
                       (((uint8_t)fmt->epoch & CUC_P1_ID_MASK) << CUC_P1_ID_SHIFT) |
                       (((oct1_basic - 1u) & CUC_P1_BASIC_MASK) << CUC_P1_BASIC_SHIFT) |
                       (oct1_frac & CUC_P1_FRAC_MASK));

    if (extended)
    {
        uint8_t add_basic = (uint8_t)(fmt->basic_octets - oct1_basic);
        uint8_t add_frac = (uint8_t)(fmt->fraction_octets - oct1_frac);
        buf[1] = (uint8_t)(((add_basic & CUC_P2_ADD_BASIC_MASK) << CUC_P2_ADD_BASIC_SHIFT) |
                           ((add_frac & CUC_P2_ADD_FRAC_MASK) << CUC_P2_ADD_FRAC_SHIFT));
    }

    *written = size;

    return CUC_OK;
}

cuc_status_t cuc_pfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               cuc_format_t *fmt,
                               size_t *consumed)
{
    if ((!buf) || (!fmt) || (!consumed))
    {
        return CUC_ERR_NULL;
    }

    if (buf_len < 1u)
    {
        return CUC_ERR_BUFFER;
    }

    uint8_t oct1 = buf[0];
    uint8_t id = (oct1 >> CUC_P1_ID_SHIFT) & CUC_P1_ID_MASK;
    if ((id != CUC_EPOCH_CCSDS) && (id != CUC_EPOCH_AGENCY))
    {
        return CUC_ERR_PFIELD_ID;
    }

    fmt->epoch = (cuc_epoch_t)id;
    fmt->basic_octets = (uint8_t)(((oct1 >> CUC_P1_BASIC_SHIFT) & CUC_P1_BASIC_MASK) + 1u);
    fmt->fraction_octets = (uint8_t)(oct1 & CUC_P1_FRAC_MASK);

    if ((oct1 & CUC_P1_EXTENSION) == 0u)
    {
        *consumed = 1u;

        return CUC_OK;
    }

    if (buf_len < 2u)
    {
        return CUC_ERR_BUFFER;
    }

    uint8_t oct2 = buf[1];
    /* A third P-field octet would be signalled here; this library defines only two. */
    if ((oct2 & CUC_P2_EXTENSION) != 0u)
    {
        return CUC_ERR_UNSUPPORTED;
    }

    fmt->basic_octets += (uint8_t)((oct2 >> CUC_P2_ADD_BASIC_SHIFT) & CUC_P2_ADD_BASIC_MASK);
    fmt->fraction_octets += (uint8_t)((oct2 >> CUC_P2_ADD_FRAC_SHIFT) & CUC_P2_ADD_FRAC_MASK);
    *consumed = 2u;

    return CUC_OK;
}

cuc_status_t cuc_tfield_encode(const cuc_time_t *time,
                               const cuc_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written)
{
    cuc_status_t status = cuc_format_validate(fmt);
    if (status != CUC_OK)
    {
        return status;
    }

    if ((!time) || (!buf) || (!written))
    {
        return CUC_ERR_NULL;
    }

    size_t size = cuc_tfield_size(fmt);
    if (buf_len < size)
    {
        return CUC_ERR_BUFFER;
    }

    /* Basic time: most significant octet first. Octets above bit 56 hold the
     * high part of a 7-octet count; the field naturally rolls over mod 256^n. */
    for (uint8_t i = 0; i < fmt->basic_octets; i++)
    {
        unsigned shift = (unsigned)(fmt->basic_octets - 1u - i) * 8u;
        buf[i] = (uint8_t)(time->seconds >> shift);
    }

    /* Fractional time: most significant octet (weight 2^-8) first. */
    for (uint8_t j = 0; j < fmt->fraction_octets; j++)
    {
        uint8_t octet = 0;
        if (j < CUC_FRACTION_STORED_OCTETS)
        {
            octet = (uint8_t)(time->fraction >> (56u - (unsigned)j * 8u));
        }

        buf[fmt->basic_octets + j] = octet;
    }

    *written = size;

    return CUC_OK;
}

cuc_status_t cuc_tfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               const cuc_format_t *fmt,
                               cuc_time_t *time,
                               size_t *consumed)
{
    cuc_status_t status = cuc_format_validate(fmt);
    if (status != CUC_OK)
    {
        return status;
    }

    if ((!buf) || (!time) || (!consumed))
    {
        return CUC_ERR_NULL;
    }

    size_t size = cuc_tfield_size(fmt);
    if (buf_len < size)
    {
        return CUC_ERR_BUFFER;
    }

    uint64_t seconds = 0;
    for (uint8_t i = 0; i < fmt->basic_octets; i++)
    {
        seconds = (seconds << 8) | buf[i];
    }

    uint64_t fraction = 0;
    for (uint8_t j = 0; (j < fmt->fraction_octets) && (j < CUC_FRACTION_STORED_OCTETS); j++)
    {
        fraction |= (uint64_t)buf[fmt->basic_octets + j] << (56u - (unsigned)j * 8u);
    }

    time->seconds = seconds;
    time->fraction = fraction;
    *consumed = size;

    return CUC_OK;
}

cuc_status_t cuc_encode(const cuc_time_t *time,
                        const cuc_format_t *fmt,
                        uint8_t *buf,
                        size_t buf_len,
                        size_t *written)
{
    if (!written)
    {
        return CUC_ERR_NULL;
    }

    size_t p_len = 0;
    cuc_status_t status = cuc_pfield_encode(fmt, buf, buf_len, &p_len);
    if (status != CUC_OK)
    {
        return status;
    }

    size_t t_len = 0;
    status = cuc_tfield_encode(time, fmt, buf + p_len, buf_len - p_len, &t_len);
    if (status != CUC_OK)
    {
        return status;
    }

    *written = p_len + t_len;

    return CUC_OK;
}

cuc_status_t cuc_decode(const uint8_t *buf,
                        size_t buf_len,
                        cuc_format_t *fmt,
                        cuc_time_t *time,
                        size_t *consumed)
{
    if (!consumed)
    {
        return CUC_ERR_NULL;
    }

    size_t p_len = 0;
    cuc_status_t status = cuc_pfield_decode(buf, buf_len, fmt, &p_len);
    if (status != CUC_OK)
    {
        return status;
    }

    size_t t_len = 0;
    status = cuc_tfield_decode(buf + p_len, buf_len - p_len, fmt, time, &t_len);
    if (status != CUC_OK)
    {
        return status;
    }

    *consumed = p_len + t_len;

    return CUC_OK;
}

#ifndef CUC_NO_FLOAT

/** @brief 2^64 as a double, for converting the Q0.64 fraction to/from seconds. */
#    define CUC_TWO_POW_64 18446744073709551616.0

double cuc_time_to_seconds(const cuc_time_t *time)
{
    if (!time)
    {
        return 0.0;
    }

    return (double)time->seconds + (double)time->fraction / CUC_TWO_POW_64;
}

cuc_time_t cuc_time_from_seconds(double seconds)
{
    cuc_time_t time = {0, 0};
    if (seconds < 0.0)
    {
        return time;
    }

    time.seconds = (uint64_t)seconds;
    double frac = seconds - (double)time.seconds;
    time.fraction = (uint64_t)(frac * CUC_TWO_POW_64);

    return time;
}

#endif /* CUC_NO_FLOAT */
