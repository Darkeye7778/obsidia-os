#pragma once
#include <stdint.h>
int copy_from_user(void* dst, uint64_t src, uint64_t len);
int copy_to_user(uint64_t dst, const void* src, uint64_t len);
int copy_string_from_user(char* dst, uint64_t src, uint64_t capacity);
