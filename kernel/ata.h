#pragma once
#include <stdint.h>

void ata_init(void);

// Try to detect and register primary ATA disk as block device "ata0"
int ata_detect_and_register(void);
