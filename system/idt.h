#ifndef EC_IDT_H
#define EC_IDT_H

/* Minimal CPU-exception support (diagnostic).
 *
 * idt_init() installs an IDT covering vectors 0..31 and brings up COM1
 * (0x3F8).  diag_mark() emits single-character breadcrumbs to COM1 so we
 * can see how far the kernel gets before dying.  Any CPU fault lands in
 * fault_handler(), which dumps vector/err/eip/cr2 to COM1 and the screen
 * and halts instead of triple-faulting. */
void idt_init(void);
void diag_mark(char c);

#endif
