#ifndef SWM_PROTOCOL_H
#define SWM_PROTOCOL_H

#include <swm/swm.h>

int swm_protocol_send_event(swm_state_t *swm, uint64_t peer, uint16_t type,
    uint32_t object_id, uint32_t serial, const void *body, size_t body_len);
int swm_protocol_send_event_nb(swm_state_t *swm, uint64_t peer,
    uint16_t type, uint32_t object_id, uint32_t serial, const void *body,
    size_t body_len);
void swm_protocol_clear_client_cursor(swm_state_t *swm);
void swm_protocol_shell_changed(swm_state_t *swm);
void swm_protocol_shell_removed(swm_state_t *swm, uint32_t surface_id);
void swm_protocol_shell_flush(swm_state_t *swm);
void swm_protocol_drop_client(swm_state_t *swm, swm_client_t *client,
    const char *reason);
swm_client_t *swm_protocol_alloc_client(swm_state_t *swm, uint64_t peer);
int swm_protocol_setup_service(swm_state_t *swm);
int swm_protocol_dispatch(swm_state_t *swm);

#endif
