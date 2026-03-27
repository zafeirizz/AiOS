#ifndef E1000_H
#define E1000_H

#include <stdint.h>

/* Intel e1000 register offsets */
#define E1000_REG_CTRL        0x0000  /* Device Control */
#define E1000_REG_STATUS      0x0008  /* Device Status */
#define E1000_REG_EECD        0x0010  /* EEPROM/Flash Control/Data */
#define E1000_REG_EERD        0x0014  /* EEPROM Read */
#define E1000_REG_CTRL_EXT    0x0018  /* Extended Device Control */
#define E1000_REG_FLA         0x001C  /* Flash Access */
#define E1000_REG_MDIC        0x0020  /* MDI Control */
#define E1000_REG_SCTL        0x0024  /* SerDes Control */
#define E1000_REG_FEXTNVM     0x0028  /* Future Extended NVM */
#define E1000_REG_FCAL        0x0028  /* Flow Control Address Low */
#define E1000_REG_FCAH        0x002C  /* Flow Control Address High */
#define E1000_REG_FCT         0x0030  /* Flow Control Type */
#define E1000_REG_VET         0x0038  /* VLAN Ether Type */
#define E1000_REG_ICR         0x00C0  /* Interrupt Cause Read */
#define E1000_REG_ITR         0x00C4  /* Interrupt Throttling */
#define E1000_REG_ICS         0x00C8  /* Interrupt Cause Set */
#define E1000_REG_IMS         0x00D0  /* Interrupt Mask Set/Read */
#define E1000_REG_IMC         0x00D8  /* Interrupt Mask Clear */
#define E1000_REG_IAM         0x00E0  /* Interrupt Acknowledge Auto Mask */
#define E1000_REG_RCTL        0x0100  /* Receive Control */
#define E1000_REG_FCTTV       0x0170  /* Flow Control Transmit Timer Value */
#define E1000_REG_TXCW        0x0178  /* Transmit Configuration Word */
#define E1000_REG_RXCW        0x0180  /* Receive Configuration Word */
#define E1000_REG_TCTL        0x0400  /* Transmit Control */
#define E1000_REG_TIPG        0x0410  /* Transmit Inter Packet Gap */
#define E1000_REG_AIFS        0x0458  /* Adaptive IFS Throttle */
#define E1000_REG_LEDCTL      0x0E00  /* LED Control */
#define E1000_REG_PBA         0x1000  /* Packet Buffer Allocation */
#define E1000_REG_FCRTL       0x2160  /* Flow Control Receive Threshold Low */
#define E1000_REG_FCRTH       0x2168  /* Flow Control Receive Threshold High */
#define E1000_REG_RDBAL       0x2800  /* Receive Descriptor Base Address Low */
#define E1000_REG_RDBAH       0x2804  /* Receive Descriptor Base Address High */
#define E1000_REG_RDRLEN      0x2808  /* Receive Descriptor Length */
#define E1000_REG_RDH         0x2810  /* Receive Descriptor Head */
#define E1000_REG_RDT         0x2818  /* Receive Descriptor Tail */
#define E1000_REG_RDTR        0x2820  /* Receive Delay Timer */
#define E1000_REG_RDBAL0      E1000_REG_RDBAL  /* Receive Descriptor Base Address Low Queue 0 */
#define E1000_REG_RDBAH0      E1000_REG_RDBAH  /* Receive Descriptor Base Address High Queue 0 */
#define E1000_REG_RDRLEN0     E1000_REG_RDRLEN /* Receive Descriptor Length Queue 0 */
#define E1000_REG_RDH0        E1000_REG_RDH    /* Receive Descriptor Head Queue 0 */
#define E1000_REG_RDT0        E1000_REG_RDT    /* Receive Descriptor Tail Queue 0 */
#define E1000_REG_RDTR0       E1000_REG_RDTR   /* Receive Delay Timer Queue 0 */
#define E1000_REG_RXDCTL0     0x2828  /* Receive Descriptor Control Queue 0 */
#define E1000_REG_RADV        0x282C  /* Receive Interrupt Absolute Delay Timer */
#define E1000_REG_RSRPD       0x2C00  /* Receive Small Packet Detect */
#define E1000_REG_TDBAL       0x3800  /* Transmit Descriptor Base Address Low */
#define E1000_REG_TDBAH       0x3804  /* Transmit Descriptor Base Address High */
#define E1000_REG_TDLEN       0x3808  /* Transmit Descriptor Length */
#define E1000_REG_TDH         0x3810  /* Transmit Descriptor Head */
#define E1000_REG_TDT         0x3818  /* Transmit Descriptor Tail */
#define E1000_REG_TIDV        0x3820  /* Transmit Interrupt Delay Value */
#define E1000_REG_TXDCTL      0x3828  /* Transmit Descriptor Control */
#define E1000_REG_TADV        0x382C  /* Transmit Interrupt Absolute Delay Timer */
#define E1000_REG_TSPMT       0x3830  /* TCP Segmentation Pad and Min Threshold */
#define E1000_REG_TDBAL0      E1000_REG_TDBAL  /* Transmit Descriptor Base Address Low Queue 0 */
#define E1000_REG_TDBAH0      E1000_REG_TDBAH  /* Transmit Descriptor Base Address High Queue 0 */
#define E1000_REG_TDLEN0      E1000_REG_TDLEN  /* Transmit Descriptor Length Queue 0 */
#define E1000_REG_TDH0        E1000_REG_TDH    /* Transmit Descriptor Head Queue 0 */
#define E1000_REG_TDT0        E1000_REG_TDT    /* Transmit Descriptor Tail Queue 0 */
#define E1000_REG_TXDCTL0     E1000_REG_TXDCTL /* Transmit Descriptor Control Queue 0 */
#define E1000_REG_RAL         0x5400  /* Receive Address Low */
#define E1000_REG_RAH         0x5404  /* Receive Address High */

/* Receive Descriptor */
typedef struct {
    volatile uint64_t addr;    /* Buffer address */
    volatile uint16_t length;  /* Length */
    volatile uint16_t checksum; /* Checksum */
    volatile uint8_t status;   /* Status */
    volatile uint8_t errors;   /* Errors */
    volatile uint16_t special; /* Special */
} __attribute__((packed)) e1000_rx_desc_t;

/* Transmit Descriptor */
typedef struct {
    volatile uint64_t addr;    /* Buffer address */
    volatile uint16_t length;  /* Length */
    volatile uint8_t cso;      /* Checksum offset */
    volatile uint8_t cmd;      /* Command */
    volatile uint8_t status;   /* Status */
    volatile uint8_t css;      /* Checksum start */
    volatile uint16_t special; /* Special */
} __attribute__((packed)) e1000_tx_desc_t;

/* e1000 device structure */
typedef struct {
    uint32_t io_base;          /* I/O base address */
    uint8_t mac_addr[6];       /* MAC address */
    uint8_t* rx_buffer;        /* Receive buffer */
    uint8_t* tx_buffer;        /* Transmit buffer */
    e1000_rx_desc_t* rx_descs; /* Receive descriptors */
    e1000_tx_desc_t* tx_descs; /* Transmit descriptors */
    uint32_t rx_cur;           /* Current receive descriptor */
    uint32_t tx_cur;           /* Current transmit descriptor */
} e1000_device_t;

/* Function prototypes */
int e1000_init(uint32_t pci_addr);
void e1000_send(const void* data, uint16_t len);
int e1000_recv(void* buffer, uint16_t buffer_len);

#endif