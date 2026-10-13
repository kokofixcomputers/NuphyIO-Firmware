#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "keyeng.h"
#include "nph_proto.h"
void    usb_init(nph_t *proto);
void    usb_task(void);                         /* call every loop; also pumps queued config responses */
void    usb_send(const ke_report_t *r);         /* remember the desired HID state; sent as soon as the endpoint is free */
uint8_t usb_leds(void);                         /* host LED bits: 1 = num, 2 = caps, 4 = scroll */
bool    usb_mounted(void);
void    usb_enable(bool on);                    /* off: detach from the bus and shut the USB peripheral and its clock down (wireless mode); on: attach again */
bool    usb_suspended(void);                    /* the host has suspended the bus (sleeping / display off) */
bool    usb_remote_wakeup(void);                /* ask a suspended host to wake up (false if it did not allow it) */
bool    usb_take_resume(void);                  /* true once after the host resumed the bus */
void    usb_mouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel, int8_t pan);
void    usb_send_raw(const uint8_t *frame, uint8_t len);   /* unframed report on the config channel (macro-recording key events) */
