/* !DEFINES!
$define %func usb_keyboard_probe as function with args device_t
$define %func usb_keyboard_attach as function with args device_t
*/
/* !SPACE!
$space %internal usb_keyboard_probe, usb_keyboard_attach
$space %internal usb_keyboard_complete, usb_keyboard_getchar
$space %internal usb_keyboard_handler
*/
#include <kernel/drivers/HID/hid.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/keyboard/keycodes.h>
#include <kernel/drivers/keyboard/keymap.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/mm/kmem.h>
typedef struct { hid_interface_t *hid; u8 previous[8]; u8 buffer[64]; char cbuffer[64]; u8 chead; u8 ctail; } usb_keyboard_t;
static usb_keyboard_t	*usb_keyboard_state;
static int
usb_keyboard_has(const u8 *report, u8 usage)
{ u8 i; for (i = 2; i < 8; i++) if (report[i] == usage) return (1); return (0); }
static void
usb_keyboard_complete(usb_device_t *usb, void *buffer, u32 length, int status, void *arg)
{
	usb_keyboard_t *keyboard; u8 usage, next; u32 mods, ch;
	(void)usb; (void)buffer; keyboard = arg; mods = 0;
	if (status == 0 && length >= 8) {
		if (keyboard->buffer[0] & 0x11) mods |= MOD_CTRL;
		if (keyboard->buffer[0] & 0x22) mods |= MOD_SHIFT;
		if (keyboard->buffer[0] & 0x44) mods |= MOD_ALT;
		for (usage = 4; usage < 0xE0; usage++) {
			if (usb_keyboard_has(keyboard->buffer, usage) && !usb_keyboard_has(keyboard->previous, usage)) {
				ch = keymap_ascii(usage, mods);
				if (ch != 0) {
					next = (keyboard->chead + 1) % sizeof(keyboard->cbuffer);
					if (next != keyboard->ctail) {
						keyboard->cbuffer[keyboard->chead] = (char)ch;
						keyboard->chead = next;
					}
				}
				kbd_event_put(usage, usage, KEY_EVENT_PRESS, mods, ch);
			}
			if (!usb_keyboard_has(keyboard->buffer, usage) && usb_keyboard_has(keyboard->previous, usage))
				kbd_event_put(usage, usage, KEY_EVENT_RELEASE, mods, 0);
		}
		memcpy(keyboard->previous, keyboard->buffer, sizeof(keyboard->previous));
	}
	(void)hid_interrupt_submit(keyboard->hid, keyboard->buffer, 8, usb_keyboard_complete, keyboard);
	keyboard_common_handler();
}
static int
usb_keyboard_probe(device_t dev)
{ usb_interface_t *usb = usb_interface_get(dev); return (usb != NULL && usb->class_code == USB_CLASS_HID && usb->subclass == 1 && usb->protocol == 1 ? 100 : -1); }
static void
usb_keyboard_handler(void)
{
}
static char
usb_keyboard_getchar(void)
{ usb_keyboard_t *keyboard; char c; keyboard = usb_keyboard_state; if (keyboard == NULL || keyboard->chead == keyboard->ctail) return (0); c = keyboard->cbuffer[keyboard->ctail]; keyboard->ctail = (keyboard->ctail + 1) % sizeof(keyboard->cbuffer); return (c); }
static keyboard_driver_t usb_keyboard_kbd_driver = {
	.name		= "USB Keyboard",
	.handler	= usb_keyboard_handler,
	.getchar	= usb_keyboard_getchar,
};
static int
usb_keyboard_attach(device_t dev)
{ usb_keyboard_t *keyboard; hid_interface_t *hid; hid = hid_interface_get(dev); if (hid == NULL || hid_set_boot_protocol(hid)) return (-1); keyboard = kmem_calloc(1, sizeof(*keyboard)); if (keyboard == NULL) return (-1); keyboard->hid = hid; usb_keyboard_state = keyboard; device_set_softc(dev, keyboard); if (keyboard_register_driver(&usb_keyboard_kbd_driver) != 0 || keyboard_switch_driver(&usb_keyboard_kbd_driver) != 0) { kmem_free(keyboard); return (-1); } return (hid_interrupt_submit(hid, keyboard->buffer, 8, usb_keyboard_complete, keyboard)); }
static devclass_t usb_keyboard_devclass = { .name = "usb_keyboard", .maxunit = USB_MAX_DEVICES };
static driver_t usb_keyboard_driver = { .name = "usb_keyboard", .probe = usb_keyboard_probe, .attach = usb_keyboard_attach };
DRIVER_MODULE(usb_keyboard, usb, usb_keyboard_driver, usb_keyboard_devclass, NEWBUS_PASS_LATE, NEWBUS_ORDER_MIDDLE);
