/* !DEFINES!

$define %func hid_interface_get as function with args device_t
$define %func hid_set_boot_protocol as function with args hid_interface_t *
$define %func hid_interrupt_submit as function with args hid_interface_t *, void *, u32, callback, void *

*/
/* !SPACE!

$space %export hid_interface_get, hid_set_boot_protocol, hid_interrupt_submit

*/
#include <kernel/drivers/HID/hid.h>
#include <kernel/mm/kmem.h>
#include <mlibc/mlibc.h>
#define HID_REQ_SET_PROTOCOL 0x0B
hid_interface_t *
hid_interface_get(device_t dev)
{
	hid_interface_t *hid;
	usb_interface_t *usb;
	u8 index;

	usb = usb_interface_get(dev);
	if (usb == NULL || usb->class_code != USB_CLASS_HID) return (NULL);
	hid = kmem_calloc(1, sizeof(*hid));
	if (hid == NULL) return (NULL);
	for (index = 0; index < usb->endpoint_count; index++) {
		if ((usb->endpoints[index].address & USB_DIR_IN) &&
		    (usb->endpoints[index].attributes & 3) == USB_ENDPOINT_XFER_INT) {
			hid->interrupt_in = &usb->endpoints[index]; break;
		}
	}
	if (hid->interrupt_in == NULL) { kmem_free(hid); return (NULL); }
	hid->usb = usb;
	return (hid);
}
int
hid_set_boot_protocol(hid_interface_t *hid)
{
	usb_setup_t setup;
	if (hid == NULL) return (-1);
	memset(&setup, 0, sizeof(setup));
	setup.bmRequestType = USB_DIR_OUT | USB_REQ_TYPE_CLASS | USB_RECIP_INTERFACE;
	setup.bRequest = HID_REQ_SET_PROTOCOL;
	setup.wIndex = hid->usb->number;
	return (usb_control_transfer(hid->usb->device, &setup, NULL, 0, 1000));
}
int
hid_interrupt_submit(hid_interface_t *hid, void *buffer, u32 length,
    usb_complete_t complete, void *arg)
{
	if (hid == NULL) return (-1);
	return (usb_interrupt_submit(hid->usb->device, hid->interrupt_in, buffer,
    length, complete, arg));
}
