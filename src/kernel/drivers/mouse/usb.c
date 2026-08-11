/* !DEFINES!
$define %func usb_mouse_probe as function with args device_t
$define %func usb_mouse_attach as function with args device_t
*/
/* !SPACE!
$space %internal usb_mouse_probe, usb_mouse_attach, usb_mouse_complete
*/
#include <kernel/drivers/HID/hid.h>
#include <kernel/drivers/input/input.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/drivers/timer.h>
#include <kernel/mm/kmem.h>
typedef struct { hid_interface_t *hid; u8 buffer[64]; s32 x, y; u32 buttons; } usb_mouse_t;
static void
usb_mouse_complete(usb_device_t *usb, void *buffer, u32 length, int status, void *arg)
{
	usb_mouse_t *mouse; u32 buttons, flags; s32 dx, dy, dz;
	(void)usb; mouse = arg;
	if (status == 0 && length >= 3) {
		buttons = (mouse->buffer[0] & 1 ? MOUSE_BUTTON_LEFT : 0) |
		    (mouse->buffer[0] & 2 ? MOUSE_BUTTON_RIGHT : 0) |
		    (mouse->buffer[0] & 4 ? MOUSE_BUTTON_MIDDLE : 0);
		dx = (s8)mouse->buffer[1]; dy = (s8)mouse->buffer[2];
		dz = length > 3 ? (s8)mouse->buffer[3] : 0; flags = 0;
		if (dx || dy) flags |= MOUSE_EVENT_MOVE;
		if (dz) flags |= MOUSE_EVENT_WHEEL;
		if (buttons != mouse->buttons) flags |= MOUSE_EVENT_BUTTON;
		mouse->buttons = buttons; mouse->x += dx; mouse->y += dy;
		if (flags) input_event_mouse(timer_get_ticks(), mouse->x, mouse->y,
		    dx, dy, dz, buttons, flags);
	}
	(void)hid_interrupt_submit(mouse->hid, mouse->buffer, sizeof(mouse->buffer),
	    usb_mouse_complete, mouse);
}
static int
usb_mouse_probe(device_t dev)
{
	usb_interface_t *usb = usb_interface_get(dev);
	return (usb != NULL && usb->class_code == USB_CLASS_HID &&
	    usb->subclass == 1 && usb->protocol == 2 ? 100 : -1);
}
static int
usb_mouse_attach(device_t dev)
{
	usb_mouse_t *mouse; hid_interface_t *hid;
	hid = hid_interface_get(dev); if (hid == NULL || hid_set_boot_protocol(hid)) return (-1);
	mouse = kmem_calloc(1, sizeof(*mouse)); if (mouse == NULL) return (-1);
	mouse->hid = hid; device_set_softc(dev, mouse);
	return (hid_interrupt_submit(hid, mouse->buffer, sizeof(mouse->buffer),
	    usb_mouse_complete, mouse));
}
static devclass_t usb_mouse_devclass = { .name = "usb_mouse", .maxunit = USB_MAX_DEVICES };
static driver_t usb_mouse_driver = { .name = "usb_mouse", .probe = usb_mouse_probe, .attach = usb_mouse_attach };
DRIVER_MODULE(usb_mouse, usb, usb_mouse_driver, usb_mouse_devclass, NEWBUS_PASS_LATE, NEWBUS_ORDER_MIDDLE);
