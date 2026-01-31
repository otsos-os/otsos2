#ifndef HARDWARE_H
#define HARDWARE_H
#ifdef __cplusplus
extern "C" {
#endif
unsigned char inb(unsigned short port);
void outb(unsigned short port, unsigned char data);
#ifdef __cplusplus
}
#endif
#endif
