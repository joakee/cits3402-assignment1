#include "pdf_header.h"

#include <string.h>

void set_nonce(unsigned char *pdf, uint64_t nonce)
{
    static const char hex[] = "0123456789abcdef";

    for (int i = NONCE_LENGTH - 1; i >= 0; --i) {
        pdf[NONCE_OFFSET + i] =
            (unsigned char)hex[nonce & 0xf];
        nonce >>= 4;
    }
}

void set_student_number(unsigned char *pdf, const char *id)
{
    memcpy(pdf + STUDENT_ID_OFFSET, id, STUDENT_ID_LENGTH);
}

int verify_sn(const unsigned char *pdf, const char *expected_id) {
    return memcmp(pdf + STUDENT_ID_OFFSET,
                  expected_id,
                  STUDENT_ID_LENGTH) == 0;
}