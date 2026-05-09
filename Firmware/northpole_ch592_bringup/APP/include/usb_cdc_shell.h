#ifndef USB_CDC_SHELL_H
#define USB_CDC_SHELL_H

#include <stddef.h>
#include <stdint.h>

void usb_cdc_shell_init(void);
void usb_cdc_shell_poll(void);
uint8_t usb_cdc_shell_ready(void);
size_t usb_cdc_shell_write(const uint8_t *data, size_t len);
int usb_cdc_shell_read_line(char *buffer, size_t buffer_size);

#endif /* USB_CDC_SHELL_H */
