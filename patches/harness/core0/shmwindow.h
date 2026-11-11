/*
 * shmwindow.h - core 0's side of the PSRAM megabyte Linux does not own.
 *
 * The region is 0x3df00000..0x3e000000: mapped by init_linux_mmap() on both
 * cores, excluded from Linux's memory node by the DTS. See docs/cores.md.
 */

#ifndef SHMWINDOW_H
#define SHMWINDOW_H

/* Starts the writer task on core 0. Silent: it publishes into the window. */
void shmwindow_start(void);

#endif /* SHMWINDOW_H */
