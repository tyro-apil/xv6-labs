#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static char *tx_bufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static char *rx_bufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;
struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;
  
  initlock(&e1000_lock, "e1000");
  regs = xregs;
  
  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();
  
  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_bufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_bufs[i] = kalloc();
    if (!rx_bufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_bufs[i];
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);
  
  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;
  
  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
                    E1000_TCTL_PSP |  // pad short packets
                    (0x10 << E1000_TCTL_CT_SHIFT) |  // collision stuff
                    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20);  // inter-pkt gap
  
  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN |   // enable receiver
                    E1000_RCTL_BAM |   // enable broadcast
                    E1000_RCTL_SZ_2048 |  // 2048-byte rx buffers
                    E1000_RCTL_SECRC;  // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0;  // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0;  // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7);  // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(char *buf, int len)
{
  // Get current transmit descriptor index
  uint32 idx = regs[E1000_TDT];
  
  // Check if the descriptor is available (DD bit set means hardware is done with it)
  if((tx_ring[idx].status & E1000_TXD_STAT_DD) == 0) {
    // Descriptor is still in use
    return -1;
  }
  
  // Free any previously transmitted packet buffer
  if(tx_bufs[idx]) {
    kfree(tx_bufs[idx]);
  }
  
  // Allocate memory and copy the packet
  tx_bufs[idx] = kalloc();
  if(!tx_bufs[idx]) {
    return -1;  // Out of memory
  }
  memmove(tx_bufs[idx], buf, len);
  
  // Fill in the descriptor
  tx_ring[idx].addr = (uint64)tx_bufs[idx];
  tx_ring[idx].length = len;
  
  // Set the necessary command bits:
  // RS (Report Status) - set DD bit when done
  // EOP (End of Packet) - this is the entire packet
  tx_ring[idx].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
  
  // Clear the status
  tx_ring[idx].status = 0;
  
  // Update the tail pointer to tell the hardware there's a new packet
  regs[E1000_TDT] = (idx + 1) % TX_RING_SIZE;
  
  return 0;
}

static void
e1000_recv(void)
{
  // Process all received packets
  while(1) {
    // Get the next expected receive descriptor
    uint32 idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    
    // Check if the descriptor has a packet (DD bit set)
    if((rx_ring[idx].status & E1000_RXD_STAT_DD) == 0) {
      // No more packets received
      break;
    }
    
    // Make sure we got a valid packet (EOP bit set)
    if((rx_ring[idx].status & E1000_RXD_STAT_EOP) == 0) {
      // Error: packet too large for one buffer
      rx_ring[idx].status = 0;
      regs[E1000_RDT] = idx;
      continue;
    }
    
    // Get the packet length
    int length = rx_ring[idx].length;
    
    // Deliver the packet to the networking stack
    if(length > 0) {
      net_rx(rx_bufs[idx], length);
    }
    
    // Allocate a new buffer for this descriptor
    char *new_buf = kalloc();
    if(!new_buf) {
      panic("e1000_recv: out of memory");
    }
    
    // Get the old buffer
    char *old_buf = rx_bufs[idx];
    
    // Update the descriptor with the new buffer
    rx_bufs[idx] = new_buf;
    rx_ring[idx].addr = (uint64)new_buf;
    
    // Reset the status
    rx_ring[idx].status = 0;
    
    // Update the tail to tell the hardware we've processed this descriptor
    regs[E1000_RDT] = idx;
    
    // Free the old buffer now that we're done with it
    kfree(old_buf);
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;
  
  e1000_recv();
}