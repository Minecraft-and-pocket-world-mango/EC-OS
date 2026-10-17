#include "idt.h"
#include "io.h"
#include "vga.h"
#include <stdint.h>

/* Frame pushed by the stubs in kernel/kernel_entry.asm (see isr_common). */
typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t vector, err;
    uint32_t eip, cs, eflags;
} regs_t;

extern uint32_t isr_stub_table[32];

/* ------------------------------------------------------------------ */
/*  COM1 debug port (0x3F8)                                            */
/* ------------------------------------------------------------------ */
#define COM1 0x3F8

static void serial_init(void)
{
    outb(COM1 + 1, 0x00);   /* disable interrupts        */
    outb(COM1 + 3, 0x80);   /* enable DLAB               */
    outb(COM1 + 0, 0x01);   /* divisor lo = 1 (115200)   */
    outb(COM1 + 1, 0x00);   /* divisor hi = 0            */
    outb(COM1 + 3, 0x03);   /* 8N1                       */
    outb(COM1 + 2, 0xC7);   /* FIFO on, clear, 14B       */
    outb(COM1 + 4, 0x0B);   /* DTR/RTS/OUT2              */
}

static void serial_putc(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
        ;
    outb(COM1, (uint8_t)c);
}

static void serial_print(const char *s)
{
    while (*s)
        serial_putc(*s++);
}

static void serial_hex(uint32_t v)
{
    static const char d[] = "0123456789ABCDEF";
    int i;
    serial_print("0x");
    for (i = 7; i >= 0; i--)
        serial_putc(d[(v >> (i * 4)) & 0xF]);
}

void diag_mark(char c)
{
    serial_putc(c);
}

/* ------------------------------------------------------------------ */
/*  IDT                                                                */
/* ------------------------------------------------------------------ */
struct idt_entry {
    uint16_t off_lo;
    uint16_t sel;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t off_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[32];
static struct idt_ptr   idtp;

void idt_init(void)
{
    int i;

    serial_init();

    for (i = 0; i < 32; i++) {
        uint32_t a = isr_stub_table[i];
        idt[i].off_lo = (uint16_t)(a & 0xFFFF);
        idt[i].sel    = 0x08;               /* kernel code selector */
        idt[i].zero   = 0;
        idt[i].flags  = 0x8E;               /* present, DPL0, 32-bit int gate */
        idt[i].off_hi = (uint16_t)(a >> 16);
    }

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idtp));
}

static uint32_t read_cr2(void)
{
    uint32_t v;
    __asm__ volatile("mov %%cr2, %0" : "=r"(v));
    return v;
}

static volatile int handling = 0;

/* Screen helper (best effort -- vga outputs into the back buffer and only
 * touches the LFB in gfx_flip(), which is guarded when fb is unset). */
static void scr_hex(uint32_t v)
{
    static const char d[] = "0123456789ABCDEF";
    int i;
    vga_putchar('0');
    vga_putchar('x');
    for (i = 7; i >= 0; i--)
        vga_putchar(d[(v >> (i * 4)) & 0xF]);
}

void fault_handler(regs_t *r)
{
    if (handling)
        for (;;)
            __asm__ volatile("cli; hlt");
    handling = 1;

    serial_print("\r\n*** EC-OS CPU FAULT ***\r\n");
    serial_print("vec=");    serial_hex(r->vector);
    serial_print(" err=");   serial_hex(r->err);
    serial_print(" eip=");   serial_hex(r->eip);
    serial_print(" cs=");    serial_hex(r->cs);
    serial_print(" efl=");   serial_hex(r->eflags);
    serial_print(" cr2=");   serial_hex(read_cr2());
    serial_print("\r\n");
    serial_print("eax="); serial_hex(r->eax);
    serial_print(" ebx="); serial_hex(r->ebx);
    serial_print(" ecx="); serial_hex(r->ecx);
    serial_print(" edx="); serial_hex(r->edx);
    serial_print(" esi="); serial_hex(r->esi);
    serial_print(" edi="); serial_hex(r->edi);
    serial_print(" ebp="); serial_hex(r->ebp);
    serial_print("\r\n");

    /* Try to show it on screen too (vector + faulting eip + cr2). */
    vga_clear();
    vga_print("CPU FAULT  vec=");
    scr_hex(r->vector);
    vga_putchar('\n');
    vga_print("eip=");
    scr_hex(r->eip);
    vga_putchar('\n');
    vga_print("cr2=");
    scr_hex(read_cr2());
    vga_putchar('\n');

    serial_print("*** halted ***\r\n");
    for (;;)
        __asm__ volatile("cli; hlt");
}
