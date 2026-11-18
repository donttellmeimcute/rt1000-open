#include "apo.h"

static const uint32_t APO_MINUTES[APO_NOPTS] = {0, 15, 30, 60, 120};

uint32_t rt1000_apo_minutes(uint8_t idx){
    return idx < APO_NOPTS ? APO_MINUTES[idx] : 0;
}

uint32_t rt1000_apo_timeout_ms(uint8_t idx){
    return rt1000_apo_minutes(idx) * 60000u;
}

uint32_t rt1000_apo_remaining_ms(uint32_t last_activity_ms, uint32_t now_ms,
                                 uint8_t idx){
    uint32_t timeout = rt1000_apo_timeout_ms(idx);
    if(timeout == 0) return 0;
    uint32_t elapsed = now_ms - last_activity_ms; /* wrap-safe */
    return elapsed >= timeout ? 0 : timeout - elapsed;
}

bool rt1000_apo_warn_due(uint32_t last_activity_ms, uint32_t now_ms,
                         uint8_t idx){
    uint32_t rem = rt1000_apo_remaining_ms(last_activity_ms, now_ms, idx);
    return rem != 0 && rem <= APO_WARN_MS;
}

bool rt1000_apo_due(uint32_t last_activity_ms, uint32_t now_ms, uint8_t idx){
    uint32_t timeout = rt1000_apo_timeout_ms(idx);
    if(timeout == 0) return false;
    return (now_ms - last_activity_ms) >= timeout; /* wrap-safe */
}

bool rt1000_apo_signal_active(int16_t rssi_dbm, uint8_t sql){
    int16_t thr = (int16_t)(-120 + (sql > 9 ? 9 : sql) * 4);
    return rssi_dbm > thr;
}

uint8_t rt1000_apo_idx_sanitize(uint8_t idx){
    return idx < APO_NOPTS ? idx : 0;
}
