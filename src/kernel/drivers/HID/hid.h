/* !DEFINES!

$define %type hid_interface_t as opaque USB HID interface wrapper
$define %func hid_interface_get as function with args device_t
$define %func hid_set_boot_protocol as function with args hid_interface_t *
$define %func hid_interrupt_submit as function with args hid_interface_t *, void *, u32, callback, void *

*/
/* !SPACE!

$space %export hid_interface_get, hid_set_boot_protocol, hid_interrupt_submit

*/
#ifndef KERNEL_DRIVERS_HID_HID_H
#define KERNEL_DRIVERS_HID_HID_H
#include <kernel/drivers/USB/usb.h>
typedef struct {
	usb_interface_t	*usb;
	usb_endpoint_t	*interrupt_in;
} hid_interface_t;
hid_interface_t *hid_interface_get(device_t dev);
int hid_set_boot_protocol(hid_interface_t *hid);
int hid_interrupt_submit(hid_interface_t *hid, void *buffer, u32 length,
    usb_complete_t complete, void *arg);
#endif
