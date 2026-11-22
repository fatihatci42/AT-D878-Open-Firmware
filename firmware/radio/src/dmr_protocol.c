/**
 * @file    dmr_protocol.c
 * @brief   DMR Protocol Layer & Talker Alias Decoder Implementation
 * @version 0.1.0
 */

#include "dmr_protocol.h"
#include "hal_systick.h"
#include <string.h>
#include <stdio.h>

static digital_monitor_mode_t s_monitor_mode = DIGITAL_MONITOR_DOUBLE_SLOT;
static dmr_call_info_t s_active_call;
static dmr_call_info_t s_last_heard[DMR_LAST_HEARD_COUNT];
static uint8_t s_last_heard_head = 0;
static uint8_t s_last_heard_count = 0;

int dmr_protocol_init(void)
{
    memset(&s_active_call, 0, sizeof(s_active_call));
    memset(s_last_heard, 0, sizeof(s_last_heard));
    s_last_heard_head = 0;
    s_last_heard_count = 0;
    s_monitor_mode = DIGITAL_MONITOR_DOUBLE_SLOT;
    return 0;
}

void dmr_protocol_set_monitor_mode(digital_monitor_mode_t mode)
{
    s_monitor_mode = mode;
}

int dmr_protocol_get_active_call(dmr_call_info_t *call)
{
    if (!call) return -1;
    memcpy(call, &s_active_call, sizeof(dmr_call_info_t));
    return s_active_call.is_active ? 0 : -1;
}

int dmr_protocol_get_last_heard(dmr_call_info_t *list, size_t max_count)
{
    if (!list || max_count == 0) return 0;
    size_t count = s_last_heard_count;
    if (count > max_count) count = max_count;

    for (size_t i = 0; i < count; i++) {
        uint8_t idx = (s_last_heard_head + DMR_LAST_HEARD_COUNT - 1 - i) % DMR_LAST_HEARD_COUNT;
        list[i] = s_last_heard[idx];
    }
    return (int)count;
}

void dmr_protocol_process_frame(const uint8_t *frame_data, size_t len)
{
    if (!frame_data || len < 12) return;

    /* DMR Full Link Control (LC) Header Çözümleme */
    /* Byte 0: FLCO (Full Link Control Opcode)
     * Byte 1: FID (Feature ID)
     * Byte 2-4: Service Options / Group Address (Destination ID / Talkgroup)
     * Byte 5-7: Source Address (Caller DMR ID)
     */
    uint8_t flco = frame_data[0] & 0x3F;
    if (flco == 0x00 || flco == 0x03) {
        /* Grup Çağrısı (0x00) veya Özel Çağrı (0x03) */
        uint32_t dst = ((uint32_t)frame_data[2] << 16) | ((uint32_t)frame_data[3] << 8) | frame_data[4];
        uint32_t src = ((uint32_t)frame_data[5] << 16) | ((uint32_t)frame_data[6] << 8) | frame_data[7];

        s_active_call.src_id = src;
        s_active_call.dst_id = dst;
        s_active_call.call_type = (flco == 0x00) ? SCT3258_CALL_GROUP : SCT3258_CALL_PRIVATE;
        s_active_call.is_active = true;
        s_active_call.start_time_ms = hal_tick_get();

        /* Talker Alias çıkarma (Varsa) */
        if (len >= 24 && frame_data[12] == 0x04) {
            /* 7-bit veya UTF-8 gömülü Talker Alias */
            size_t a_len = frame_data[13] & 0x1F;
            if (a_len > DMR_MAX_ALIAS_LEN - 1) a_len = DMR_MAX_ALIAS_LEN - 1;
            memcpy(s_active_call.talker_alias, &frame_data[14], a_len);
            s_active_call.talker_alias[a_len] = '\0';
        } else {
            snprintf(s_active_call.talker_alias, sizeof(s_active_call.talker_alias), "ID:%lu", (unsigned long)src);
        }

        /* Last Heard listesine kaydet */
        s_last_heard[s_last_heard_head] = s_active_call;
        s_last_heard_head = (s_last_heard_head + 1) % DMR_LAST_HEARD_COUNT;
        if (s_last_heard_count < DMR_LAST_HEARD_COUNT) {
            s_last_heard_count++;
        }
    }
}
