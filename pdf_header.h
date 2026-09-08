#ifndef PDF_HEADER_H
#define PDF_HEADER_H

#include <stdint.h>

#define NONCE_OFFSET      16
#define NONCE_LENGTH      16
#define STUDENT_ID_OFFSET 45
#define STUDENT_ID_LENGTH 8

void set_nonce(unsigned char *pdf, uint64_t nonce);
void set_student_number(unsigned char *pdf, const char *id);

#endif