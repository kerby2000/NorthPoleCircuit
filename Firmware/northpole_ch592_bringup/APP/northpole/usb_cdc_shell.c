#include "usb_cdc_shell.h"

#include "CONFIG.h"
#include "app_config.h"

#include <string.h>

#define CDC_EP0_SIZE 64u
#define CDC_PACKET_SIZE 64u
#define CDC_RX_RING_SIZE 256u
#define CDC_TX_RING_SIZE 2048u

#define CDC_REQ_SET_LINE_CODING 0x20u
#define CDC_REQ_GET_LINE_CODING 0x21u
#define CDC_REQ_SET_CONTROL_LINE_STATE 0x22u

typedef struct {
    uint8_t bRequestType;
    uint8_t bRequest;
    uint8_t wValueL;
    uint8_t wValueH;
    uint8_t wIndexL;
    uint8_t wIndexH;
    uint8_t wLengthL;
    uint8_t wLengthH;
} usb_setup_req_t;

typedef struct {
    uint32_t baud_rate;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t data_bits;
} __attribute__((packed)) cdc_line_coding_t;

static const uint8_t dev_desc[] = {
    0x12, 0x01, 0x10, 0x01, 0x02, 0x00, 0x00, CDC_EP0_SIZE,
    0x86, 0x1a, 0x40, 0x80, 0x00, 0x01, 0x01, 0x02,
    0x03, 0x01,
};

static const uint8_t cfg_desc[] = {
    0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0x80, 0x30,
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,
    0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, 0x00, 0x01,
    0x05, 0x24, 0x01, 0x01, 0x00,
    0x07, 0x05, 0x84, 0x03, 0x08, 0x00, 0x10,
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0a, 0x00, 0x00, 0x00,
    0x07, 0x05, 0x01, 0x02, CDC_PACKET_SIZE, 0x00, 0x00,
    0x07, 0x05, 0x81, 0x02, CDC_PACKET_SIZE, 0x00, 0x00,
};

static const uint8_t lang_desc[] = {0x04, 0x03, 0x09, 0x04};

__attribute__((aligned(4))) static uint8_t ep0_buf[CDC_PACKET_SIZE];
__attribute__((aligned(4))) static uint8_t ep1_buf[2u * CDC_PACKET_SIZE];
__attribute__((aligned(4))) static uint8_t ep4_buf[CDC_PACKET_SIZE];

static volatile uint8_t configured;
static volatile uint8_t ep1_tx_busy;
static volatile uint8_t ep1_zlp_pending;
static volatile uint8_t pending_address;
static volatile uint8_t set_line_coding_pending;

static cdc_line_coding_t line_coding = {115200u, 0u, 0u, 8u};
static const uint8_t *ctrl_ptr;
static uint16_t ctrl_remaining;

static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static uint8_t rx_ring[CDC_RX_RING_SIZE];
static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;
static uint8_t tx_ring[CDC_TX_RING_SIZE];

static uint8_t string_desc[64];

static uint16_t ring_next(uint16_t value, uint16_t size)
{
    value++;
    return value >= size ? 0u : value;
}

static void rx_push(uint8_t value)
{
    uint16_t next = ring_next(rx_head, CDC_RX_RING_SIZE);

    if (next != rx_tail) {
        rx_ring[rx_head] = value;
        rx_head = next;
    }
}

static int rx_pop(uint8_t *value)
{
    if (rx_head == rx_tail) {
        return 0;
    }
    *value = rx_ring[rx_tail];
    rx_tail = ring_next(rx_tail, CDC_RX_RING_SIZE);
    return 1;
}

static int tx_pop(uint8_t *value)
{
    if (tx_head == tx_tail) {
        return 0;
    }
    *value = tx_ring[tx_tail];
    tx_tail = ring_next(tx_tail, CDC_TX_RING_SIZE);
    return 1;
}

static int tx_push(uint8_t value)
{
    uint16_t next = ring_next(tx_head, CDC_TX_RING_SIZE);

    if (next == tx_tail) {
        return 0;
    }
    tx_ring[tx_head] = value;
    tx_head = next;
    return 1;
}

static uint8_t make_string_desc(const char *text)
{
    uint8_t len = 2u;

    string_desc[1] = 0x03u;
    while (*text && len < sizeof(string_desc) - 1u) {
        string_desc[len++] = (uint8_t)*text++;
        string_desc[len++] = 0u;
    }
    string_desc[0] = len;
    return len;
}

static void ep0_set_stall(void)
{
    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
}

static void ep0_send_next(void)
{
    uint8_t len = ctrl_remaining > CDC_EP0_SIZE ? CDC_EP0_SIZE : (uint8_t)ctrl_remaining;

    if (len > 0u && ctrl_ptr) {
        memcpy(ep0_buf, ctrl_ptr, len);
        ctrl_ptr += len;
        ctrl_remaining -= len;
    }
    R8_UEP0_T_LEN = len;
    R8_UEP0_CTRL ^= RB_UEP_T_TOG;
    R8_UEP0_CTRL = (R8_UEP0_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

static void ep0_send(const uint8_t *data, uint16_t len, uint16_t requested)
{
    if (len > requested) {
        len = requested;
    }
    ctrl_ptr = data;
    ctrl_remaining = len;
    ep0_send_next();
}

static void ep0_status_in(void)
{
    R8_UEP0_T_LEN = 0;
    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
}

static void ep0_status_out(void)
{
    R8_UEP0_T_LEN = 0;
    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
}

static void handle_setup(void)
{
    usb_setup_req_t *req = (usb_setup_req_t *)ep0_buf;
    uint16_t w_length = (uint16_t)req->wLengthL | ((uint16_t)req->wLengthH << 8);
    uint8_t request_type = req->bRequestType & USB_REQ_TYP_MASK;

    ctrl_ptr = 0;
    ctrl_remaining = 0;
    set_line_coding_pending = 0;

    if (request_type == USB_REQ_TYP_STANDARD) {
        switch (req->bRequest) {
        case USB_GET_DESCRIPTOR:
            switch (req->wValueH) {
            case 1:
                ep0_send(dev_desc, sizeof(dev_desc), w_length);
                return;
            case 2:
                ep0_send(cfg_desc, sizeof(cfg_desc), w_length);
                return;
            case 3:
                if (req->wValueL == 0) {
                    ep0_send(lang_desc, sizeof(lang_desc), w_length);
                } else if (req->wValueL == 1) {
                    ep0_send(string_desc, make_string_desc("North Pole"), w_length);
                } else if (req->wValueL == 2) {
                    ep0_send(string_desc, make_string_desc("North Pole CDC Shell"), w_length);
                } else if (req->wValueL == 3) {
                    ep0_send(string_desc, make_string_desc("NPCH5920001"), w_length);
                } else {
                    ep0_set_stall();
                }
                return;
            default:
                ep0_set_stall();
                return;
            }
        case USB_SET_ADDRESS:
            pending_address = req->wValueL;
            ep0_status_in();
            return;
        case USB_SET_CONFIGURATION:
            configured = req->wValueL ? 1u : 0u;
            ep1_tx_busy = 0;
            R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            R8_UEP4_CTRL = UEP_T_RES_NAK;
            ep0_status_in();
            return;
        case USB_GET_CONFIGURATION:
            ep0_buf[0] = configured ? 1u : 0u;
            ep0_send(ep0_buf, 1u, w_length);
            return;
        case USB_GET_INTERFACE:
        case USB_GET_STATUS:
            ep0_buf[0] = 0;
            ep0_buf[1] = 0;
            ep0_send(ep0_buf, req->bRequest == USB_GET_STATUS ? 2u : 1u, w_length);
            return;
        case USB_CLEAR_FEATURE:
            R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            ep1_tx_busy = 0;
            ep0_status_in();
            return;
        default:
            ep0_set_stall();
            return;
        }
    }

    if (request_type == USB_REQ_TYP_CLASS) {
        switch (req->bRequest) {
        case CDC_REQ_SET_LINE_CODING:
            set_line_coding_pending = 1;
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
            return;
        case CDC_REQ_GET_LINE_CODING:
            memcpy(ep0_buf, &line_coding, sizeof(line_coding));
            ep0_send(ep0_buf, sizeof(line_coding), w_length);
            return;
        case CDC_REQ_SET_CONTROL_LINE_STATE:
            ep0_status_in();
            return;
        default:
            ep0_set_stall();
            return;
        }
    }

    ep0_set_stall();
}

static void reset_usb_state(void)
{
    configured = 0;
    ep1_tx_busy = 0;
    ep1_zlp_pending = 0;
    pending_address = 0;
    set_line_coding_pending = 0;
    ctrl_ptr = 0;
    ctrl_remaining = 0;

    R8_USB_DEV_AD = 0;
    R8_UEP0_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
    R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP4_CTRL = UEP_T_RES_NAK;
}

static void start_tx_packet(void)
{
    uint8_t len = 0;

    if (!configured || ep1_tx_busy || tx_head == tx_tail) {
        if (configured && !ep1_tx_busy && ep1_zlp_pending && tx_head == tx_tail) {
            ep1_zlp_pending = 0;
            ep1_tx_busy = 1;
            R8_UEP1_T_LEN = 0;
            R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
        }
        return;
    }

    ep1_zlp_pending = 0;
    while (len < CDC_PACKET_SIZE && tx_pop(&ep1_buf[CDC_PACKET_SIZE + len])) {
        len++;
    }

    if (len > 0u) {
        if (len == CDC_PACKET_SIZE && tx_head == tx_tail) {
            ep1_zlp_pending = 1;
        }
        ep1_tx_busy = 1;
        R8_UEP1_T_LEN = len;
        R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    }
}

void usb_cdc_shell_init(void)
{
#if APP_USB_CDC_SHELL_ENABLE
    rx_head = rx_tail = 0;
    tx_head = tx_tail = 0;

    R8_USB_CTRL = 0x00;
    R8_UEP4_1_MOD = RB_UEP4_TX_EN | RB_UEP1_TX_EN | RB_UEP1_RX_EN;
    R8_UEP2_3_MOD = 0x00;
    R16_UEP0_DMA = (uint16_t)(uint32_t)&ep0_buf[0];
    R16_UEP1_DMA = (uint16_t)(uint32_t)&ep1_buf[0];
    R16_UEP3_DMA = (uint16_t)(uint32_t)&ep4_buf[0];

    reset_usb_state();
    R8_UDEV_CTRL = RB_UD_PD_DIS;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R8_USB_INT_FG = 0xff;
    R8_USB_INT_EN = RB_UIE_TRANSFER | RB_UIE_BUS_RST | RB_UIE_SUSPEND;
    PFIC_EnableIRQ(USB_IRQn);
    R8_UDEV_CTRL |= RB_UD_PORT_EN;
#endif
}

void usb_cdc_shell_poll(void)
{
#if APP_USB_CDC_SHELL_ENABLE
    start_tx_packet();
#endif
}

uint8_t usb_cdc_shell_ready(void)
{
    return configured;
}

size_t usb_cdc_shell_write(const uint8_t *data, size_t len)
{
    size_t written = 0;

#if APP_USB_CDC_SHELL_ENABLE
    while (written < len && tx_push(data[written])) {
        written++;
    }
    start_tx_packet();
#else
    (void)data;
    (void)len;
#endif
    return written;
}

size_t usb_cdc_shell_write_wait(const uint8_t *data, size_t len, uint32_t max_wait_loops)
{
    size_t written = 0;
    uint32_t wait_loops = 0;

#if APP_USB_CDC_SHELL_ENABLE
    if (!configured) {
        return 0;
    }

    while (written < len && configured) {
        while (written < len && tx_push(data[written])) {
            written++;
            wait_loops = 0;
        }
        start_tx_packet();
        if (written >= len) {
            break;
        }
        if (++wait_loops >= max_wait_loops) {
            break;
        }
        mDelayuS(100);
    }
#else
    (void)data;
    (void)len;
    (void)max_wait_loops;
#endif
    return written;
}

int usb_cdc_shell_read_line(char *buffer, size_t buffer_size)
{
    static char line[96];
    static size_t used;
    uint8_t ch;

    if (buffer_size == 0) {
        return 0;
    }

    while (rx_pop(&ch)) {
        if (ch == '\r' || ch == '\n') {
            line[used] = '\0';
            strncpy(buffer, line, buffer_size - 1u);
            buffer[buffer_size - 1u] = '\0';
            used = 0;
            return 1;
        }
        if (used < sizeof(line) - 1u) {
            line[used++] = (char)ch;
        }
    }
    return 0;
}

__attribute__((interrupt("WCH-Interrupt-fast")))
__attribute__((section(".highcode")))
void USB_IRQHandler(void)
{
#if APP_USB_CDC_SHELL_ENABLE
    uint8_t int_fg = R8_USB_INT_FG;
    uint8_t int_st = R8_USB_INT_ST;

    if (int_fg & RB_UIF_TRANSFER) {
        if (int_st & RB_UIS_SETUP_ACT) {
            handle_setup();
        } else {
            switch (int_st & 0x3f) {
            case UIS_TOKEN_OUT | 1:
                if (int_fg & RB_U_TOG_OK) {
                    uint8_t len = R8_USB_RX_LEN;
                    uint8_t i;
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_NAK;
                    for (i = 0; i < len; ++i) {
                        rx_push(ep1_buf[i]);
                    }
                    R8_UEP1_CTRL ^= RB_UEP_R_TOG;
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
                }
                break;
            case UIS_TOKEN_IN | 1:
                R8_UEP1_CTRL ^= RB_UEP_T_TOG;
                R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                ep1_tx_busy = 0;
                break;
            case UIS_TOKEN_IN | 0:
                if (pending_address) {
                    R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | pending_address;
                    pending_address = 0;
                    R8_UEP0_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
                } else if (ctrl_remaining > 0u) {
                    ep0_send_next();
                } else {
                    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                }
                break;
            case UIS_TOKEN_OUT | 0:
                if (set_line_coding_pending && R8_USB_RX_LEN >= sizeof(line_coding)) {
                    memcpy(&line_coding, ep0_buf, sizeof(line_coding));
                    set_line_coding_pending = 0;
                    ep0_status_in();
                }
                break;
            default:
                break;
            }
        }
        R8_USB_INT_FG = RB_UIF_TRANSFER;
    }

    if (int_fg & RB_UIF_BUS_RST) {
        reset_usb_state();
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }

    if (int_fg & RB_UIF_SUSPEND) {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
#else
    R8_USB_INT_FG = R8_USB_INT_FG;
#endif
}
