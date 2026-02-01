#ifndef COM1_H
#define COM1_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
#define COM1_PORT 0x3F8
#define COM1_DATA (COM1_PORT + 0)
#define COM1_INT_ENABLE (COM1_PORT + 1)
#define COM1_FIFO_CTRL (COM1_PORT + 2)
#define COM1_LINE_CTRL (COM1_PORT + 3)
#define COM1_MODEM_CTRL (COM1_PORT + 4)
#define COM1_LINE_STATUS (COM1_PORT + 5)
void com1_init(void);
void com1_write_byte(char c);
void com1_write_string(char *str);
void com1_write_hex_byte(u8 value);
void com1_write_hex_word(u16 value);
void com1_write_hex_dword(u32 value);
void com1_write_hex_qword(u64 value);
void com1_newline(void);
u8 com1_read_byte(void);
int com1_has_data(void);
void com1_write_dec(u64 value);
void com1_printf(const char *format, ...);
void com1_set_mirror_callback(void (*callback)(char));
void com1_off_mirror_callback(void);
#endif