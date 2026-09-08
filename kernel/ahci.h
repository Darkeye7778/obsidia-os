#pragma once

/* Discovers the first PCI AHCI controller and registers bounded SATA block
   devices. Returns the number registered, zero when absent, or negative on a
   controller/initialization error. */
int ahci_detect_and_register(void);

