#include <kernel/drivers/disk/pata/pata.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/interrupts/idt.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

// Точка входа(если что-то сломалось то здесь)
void kmain() {
  init_idt();
  init_heap();
  pata_identify(NULL);
  char* VGA_MEM = (char*)0xB8000;
  char* msg = "OTSOS started!";
  for (int i = 0; i < strlen(msg); i++) {
    VGA_MEM[i * 2] = msg[i];
  }

  com1_init();

  com1_write_string("OTSOS started at address ");
  com1_write_hex_qword((u64)kmain);
  com1_newline();


  char dir[256] =
      "/"; // Потом когда добавим файловую систему нужнл будет немного изменить


  while (1) {

   
  }
}