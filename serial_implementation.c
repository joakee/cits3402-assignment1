#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "pdf_header.h"

#define SOLVED_DIR "solved"

// reads a whole ass pdf into memory
// returns the buffer (in which case the caller frees), or returns NULL
// length is handed back via out_size (needed for writing it back out later)
static unsigned char *load_pdf(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }

    if (fseek(f, 0, SEEK_END) != 0) { perror(path); fclose(f); return NULL; }
    long len = ftell(f);

    if (len < 0) { perror(path); fclose(f); return NULL; }
    rewind(f);

    if (len < STUDENT_ID_OFFSET + STUDENT_ID_LENGTH) {
        fprintf(stderr, "%s: too short to be a valid PDF (%ld bytes)\n", path, len);
        fclose(f);
        return NULL;
    }

    unsigned char *buf = malloc((size_t)len);
    if (!buf) { fprintf(stderr, "%s: out of memory\n", path); fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "%s: short read\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_size = (size_t)len;
    return buf;
}

// write the buffer out to a new file under `solved/` (for now)
// returns 1 on yay, 0 on nay
static int save_pdf(const char *path, const unsigned char *buf, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 0; }

    if (fwrite(buf, 1, len, f) != len) {
        fprintf(stderr, "%s: short write\n", path);
        fclose(f);
        return 0;
    }

    // fclose could fail on flush, so a good fwrite isn't enough unto itseld */
    if (fclose(f) != 0) { perror(path); return 0; }
    return 1;
}

// "test_files/1_kilo_a.pdf" -> "solved/1_kilo_a.pdf" 
static void solved_path(const char *path, char *out, size_t out_size)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(out, out_size, SOLVED_DIR "/%s", base);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <pdf_file1> <pdf_file2> <student_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *path1 = argv[1];
    const char *path2 = argv[2];
    const char *sn    = argv[3];

    if (strlen(sn) != STUDENT_ID_LENGTH) {
        fprintf(stderr, "Student ID must be exactly %d characters\n", STUDENT_ID_LENGTH);
        return EXIT_FAILURE;
    }

    if (mkdir(SOLVED_DIR, 0755) != 0 && errno != EEXIST) {
        perror(SOLVED_DIR);
        return EXIT_FAILURE;
    }

    size_t size1; size_t size2;

    unsigned char *pdf1 = load_pdf(path1, &size1);
    unsigned char *pdf2 = load_pdf(path2, &size2);
    if (!pdf1) return EXIT_FAILURE;

    set_student_number(pdf1, sn);
    if (!verify_sn(pdf1, sn)) {
        fprintf(stderr, "Write of student ID to %s failed.\n", path1);
        free(pdf1);
        return EXIT_FAILURE;
    }

    set_student_number(pdf2, sn);
    if (!verify_sn(pdf2, sn)) {
        fprintf(stderr, "Write of student ID to %s failed.\n", path1);
        free(pdf1);
        return EXIT_FAILURE;
    }


    char out1[1024];
    solved_path(path1, out1, sizeof out1);

    if (!save_pdf(out1, pdf1, size1)) {
        free(pdf1);
        return EXIT_FAILURE;
    }

    printf("Wrote student ID to %s\n", out1);

    free(pdf1);
    return EXIT_SUCCESS;
}
