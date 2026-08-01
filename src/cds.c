/**
 * @file    cds.c
 * @brief   CCSDS Day Segmented Time Code (CDS) encoder/decoder
 *
 * Implements the CCSDS Day Segmented Time Code (CDS) as per
 * CCSDS 301.0-B-4 (Time Code Formats), Section 3.3.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cds.h"

/* P-field bit fields, bit 0 = MSB (CCSDS 301.0-B-4, 3.3.2). */
#define CDS_P_ID_SHIFT 4 /* bits 1-3: time code identification */
#define CDS_P_ID_MASK 0x07u
#define CDS_P_ID_VALUE 0x04u /* CDS identification = 100 */
#define CDS_P_EPOCH_SHIFT 3  /* bit 4: epoch identification */
#define CDS_P_EPOCH_MASK 0x01u
#define CDS_P_DAY_SHIFT 2 /* bit 5: day segment length */
#define CDS_P_DAY_MASK 0x01u
#define CDS_P_SUBMS_MASK 0x03u /* bits 6-7: sub-millisecond resolution */

#define CDS_DAY_16BIT_OCTETS 2
#define CDS_DAY_24BIT_OCTETS 3
#define CDS_SUBMS_US_OCTETS 2
#define CDS_SUBMS_PS_OCTETS 4

#define CDS_DAY_16BIT_MAX 0xFFFFu
#define CDS_DAY_24BIT_MAX 0xFFFFFFu

static void cds_write_be(uint8_t *buf, uint32_t value, size_t octets)
{
    for (size_t i = 0; i < octets; i++)
    {
        buf[i] = (uint8_t)(value >> ((octets - 1u - i) * 8u));
    }
}

static uint32_t cds_read_be(const uint8_t *buf, size_t octets)
{
    uint32_t value = 0;
    for (size_t i = 0; i < octets; i++)
    {
        value = (value << 8) | buf[i];
    }
    return value;
}

cds_status_t cds_format_validate(const cds_format_t *fmt)
{
    if (fmt == NULL)
    {
        return CDS_ERR_NULL;
    }
    if (fmt->epoch != CDS_EPOCH_CCSDS && fmt->epoch != CDS_EPOCH_AGENCY)
    {
        return CDS_ERR_FORMAT;
    }
    if (fmt->day_length != CDS_DAY_16BIT && fmt->day_length != CDS_DAY_24BIT)
    {
        return CDS_ERR_FORMAT;
    }
    if (fmt->submillisecond != CDS_SUBMS_NONE && fmt->submillisecond != CDS_SUBMS_US &&
        fmt->submillisecond != CDS_SUBMS_PS)
    {
        return CDS_ERR_FORMAT;
    }
    return CDS_OK;
}

size_t cds_day_size(const cds_format_t *fmt)
{
    if (fmt == NULL)
    {
        return 0;
    }
    return fmt->day_length == CDS_DAY_24BIT ? CDS_DAY_24BIT_OCTETS : CDS_DAY_16BIT_OCTETS;
}

size_t cds_subms_size(const cds_format_t *fmt)
{
    if (fmt == NULL)
    {
        return 0;
    }
    switch (fmt->submillisecond)
    {
    case CDS_SUBMS_US:
        return CDS_SUBMS_US_OCTETS;
    case CDS_SUBMS_PS:
        return CDS_SUBMS_PS_OCTETS;
    default:
        return 0;
    }
}

size_t cds_tfield_size(const cds_format_t *fmt)
{
    if (fmt == NULL)
    {
        return 0;
    }
    return cds_day_size(fmt) + CDS_MS_OF_DAY_OCTETS + cds_subms_size(fmt);
}

size_t cds_size(const cds_format_t *fmt)
{
    if (fmt == NULL)
    {
        return 0;
    }
    return CDS_PFIELD_OCTETS + cds_tfield_size(fmt);
}

cds_status_t cds_pfield_encode(const cds_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written)
{
    cds_status_t status = cds_format_validate(fmt);
    if (status != CDS_OK)
    {
        return status;
    }
    if (buf == NULL || written == NULL)
    {
        return CDS_ERR_NULL;
    }
    if (buf_len < CDS_PFIELD_OCTETS)
    {
        return CDS_ERR_BUFFER;
    }

    buf[0] = (uint8_t)((CDS_P_ID_VALUE << CDS_P_ID_SHIFT) |
                       (((uint8_t)fmt->epoch & CDS_P_EPOCH_MASK) << CDS_P_EPOCH_SHIFT) |
                       (((uint8_t)fmt->day_length & CDS_P_DAY_MASK) << CDS_P_DAY_SHIFT) |
                       ((uint8_t)fmt->submillisecond & CDS_P_SUBMS_MASK));
    *written = CDS_PFIELD_OCTETS;
    return CDS_OK;
}

cds_status_t cds_pfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               cds_format_t *fmt,
                               size_t *consumed)
{
    if (buf == NULL || fmt == NULL || consumed == NULL)
    {
        return CDS_ERR_NULL;
    }
    if (buf_len < CDS_PFIELD_OCTETS)
    {
        return CDS_ERR_BUFFER;
    }

    uint8_t octet = buf[0];
    if (((octet >> CDS_P_ID_SHIFT) & CDS_P_ID_MASK) != CDS_P_ID_VALUE)
    {
        return CDS_ERR_PFIELD_ID;
    }

    fmt->epoch = (cds_epoch_t)((octet >> CDS_P_EPOCH_SHIFT) & CDS_P_EPOCH_MASK);
    fmt->day_length = (cds_day_length_t)((octet >> CDS_P_DAY_SHIFT) & CDS_P_DAY_MASK);
    fmt->submillisecond = (cds_subms_t)(octet & CDS_P_SUBMS_MASK);

    /* Bits 6-7 = '11' is reserved for future use (CCSDS 301.0-B-4, 3.3.2). */
    if (fmt->submillisecond != CDS_SUBMS_NONE && fmt->submillisecond != CDS_SUBMS_US &&
        fmt->submillisecond != CDS_SUBMS_PS)
    {
        return CDS_ERR_FORMAT;
    }

    *consumed = CDS_PFIELD_OCTETS;
    return CDS_OK;
}

static cds_status_t cds_check_ranges(const cds_time_t *time, const cds_format_t *fmt)
{
    uint32_t day_max = fmt->day_length == CDS_DAY_24BIT ? CDS_DAY_24BIT_MAX : CDS_DAY_16BIT_MAX;
    if (time->days > day_max)
    {
        return CDS_ERR_FORMAT;
    }
    if (fmt->submillisecond == CDS_SUBMS_US && time->submilliseconds > CDS_DAY_16BIT_MAX)
    {
        return CDS_ERR_FORMAT;
    }
    return CDS_OK;
}

cds_status_t cds_tfield_encode(const cds_time_t *time,
                               const cds_format_t *fmt,
                               uint8_t *buf,
                               size_t buf_len,
                               size_t *written)
{
    cds_status_t status = cds_format_validate(fmt);
    if (status != CDS_OK)
    {
        return status;
    }
    if (time == NULL || buf == NULL || written == NULL)
    {
        return CDS_ERR_NULL;
    }

    size_t size = cds_tfield_size(fmt);
    if (buf_len < size)
    {
        return CDS_ERR_BUFFER;
    }
    status = cds_check_ranges(time, fmt);
    if (status != CDS_OK)
    {
        return status;
    }

    size_t day_octets = cds_day_size(fmt);
    size_t subms_octets = cds_subms_size(fmt);
    cds_write_be(buf, time->days, day_octets);
    cds_write_be(buf + day_octets, time->ms_of_day, CDS_MS_OF_DAY_OCTETS);
    if (subms_octets > 0)
    {
        cds_write_be(buf + day_octets + CDS_MS_OF_DAY_OCTETS, time->submilliseconds, subms_octets);
    }

    *written = size;
    return CDS_OK;
}

cds_status_t cds_tfield_decode(const uint8_t *buf,
                               size_t buf_len,
                               const cds_format_t *fmt,
                               cds_time_t *time,
                               size_t *consumed)
{
    cds_status_t status = cds_format_validate(fmt);
    if (status != CDS_OK)
    {
        return status;
    }
    if (buf == NULL || time == NULL || consumed == NULL)
    {
        return CDS_ERR_NULL;
    }

    size_t size = cds_tfield_size(fmt);
    if (buf_len < size)
    {
        return CDS_ERR_BUFFER;
    }

    size_t day_octets = cds_day_size(fmt);
    size_t subms_octets = cds_subms_size(fmt);
    time->days = cds_read_be(buf, day_octets);
    time->ms_of_day = cds_read_be(buf + day_octets, CDS_MS_OF_DAY_OCTETS);
    time->submilliseconds =
        subms_octets > 0 ? cds_read_be(buf + day_octets + CDS_MS_OF_DAY_OCTETS, subms_octets) : 0u;

    *consumed = size;
    return CDS_OK;
}

cds_status_t cds_encode(const cds_time_t *time,
                        const cds_format_t *fmt,
                        uint8_t *buf,
                        size_t buf_len,
                        size_t *written)
{
    if (written == NULL)
    {
        return CDS_ERR_NULL;
    }

    size_t p_len = 0;
    cds_status_t status = cds_pfield_encode(fmt, buf, buf_len, &p_len);
    if (status != CDS_OK)
    {
        return status;
    }

    size_t t_len = 0;
    status = cds_tfield_encode(time, fmt, buf + p_len, buf_len - p_len, &t_len);
    if (status != CDS_OK)
    {
        return status;
    }

    *written = p_len + t_len;
    return CDS_OK;
}

cds_status_t cds_decode(const uint8_t *buf,
                        size_t buf_len,
                        cds_format_t *fmt,
                        cds_time_t *time,
                        size_t *consumed)
{
    if (consumed == NULL)
    {
        return CDS_ERR_NULL;
    }

    size_t p_len = 0;
    cds_status_t status = cds_pfield_decode(buf, buf_len, fmt, &p_len);
    if (status != CDS_OK)
    {
        return status;
    }

    size_t t_len = 0;
    status = cds_tfield_decode(buf + p_len, buf_len - p_len, fmt, time, &t_len);
    if (status != CDS_OK)
    {
        return status;
    }

    *consumed = p_len + t_len;
    return CDS_OK;
}
