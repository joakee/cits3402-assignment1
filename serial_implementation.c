#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include "pdf_header.h"
#include "toy_hash.h"

#define SOLVED_DIR "solved"

// which file a table entry came from; 
// 1, 2 = pdf (or at least something) is there, 
//    0 = empty/unused 
#define PDF1 1
#define PDF2 2

// one slot in the collision table; the 48-bit hash and the file tag are
// packed into a single word so a slot is 16 bytes, and an all-zero slot
// reads as empty (a real entry always has the file bits set, even when the
// hash itself happens to be 0)

// repreresents a single slot in the collision table
// - 1 slot = 16 bytes  
//          = 48 bit hash + 16 bit file tag + 64 bit nonce 
//          = 64 bit tag + 64 bit nonce
//          = 8 byte tag + 8 byte nonce = 16 byte HashEntry  
typedef struct HashEntry {
    uint64_t tag;   // (file << 48) | hash
    uint64_t nonce; // the nonce that produced this hash
} HashEntry;

#define HASH_MASK  0x0000ffffffffffffULL // toy_hash only outputs 48 bits
#define TABLE_CAP  (1ULL << 27)          // 2^27 slots * 16 B = 2 GiB
#define TABLE_MASK (TABLE_CAP - 1)
#define MAX_TRIES  50000000ULL            // bail out after this many nonce pairs

// every pass of the search loop adds at most 2 entries, so MAX_TRIES keeps
// the table at most about 75% full to make sure the probe is given an empty slot
_Static_assert(2 * MAX_TRIES < TABLE_CAP, "MAX_TRIES would overflow the table"); // IS THIS PART NECESSARY? IT IS HARD-CODED

// reads the whole pdf into memory
// returns the buffer (in which case the caller frees), or returns NULL
// length is handed back via out_size (needed for writing it back out later)
static unsigned char *load_pdf(const char *path, size_t *out_size)
{
    // open file for 'reading binary'
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return NULL;
    }

    // seek to the end to find the file length
    if (fseek(f, 0, SEEK_END) != 0) {
        perror(path);
        fclose(f);
        return NULL;
    }
    long len = ftell(f);

    if (len < 0) {
        perror(path);
        fclose(f);
        return NULL;
    }
    // go back to the beginning so we can copy the pdf for solving
    rewind(f);

    if (len < STUDENT_ID_OFFSET + STUDENT_ID_LENGTH) {
        fprintf(stderr, "%s: too short to be a valid PDF (%ld bytes)\n", path, len);
        fclose(f);
        return NULL;
    }

    unsigned char *buf = malloc((size_t)len);
    if (!buf) {
        fprintf(stderr, "%s: out of memory\n", path);
        fclose(f);
        return NULL;
    }

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
    // open file for 'write binary' so we can copy pdf contents
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return 0;
    }

    if (fwrite(buf, 1, len, f) != len) {
        fprintf(stderr, "%s: short write\n", path);
        fclose(f);
        return 0;
    }

    // fclose could fail on flush, so a good fwrite isn't enough unto itself
    if (fclose(f) != 0) {
        perror(path);
        return 0;
    }
    return 1;
}

// "test_files/1_kilo_a.pdf" -> "solved/1_kilo_a.pdf" 
static void solved_path(const char *path, char *out, size_t out_size) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(out, out_size, SOLVED_DIR "/%s", base);
}

// drop (hash, nonce, file) into the table, unless the OTHER file already
// produced the same hash earlier; in which case the collision has been found
// returns 1 on that cross-file match  (*matched_nonce gets the stored entry's nonce), 
// 0 otherwise, &the entry gets stored
static int insert_or_match(HashEntry *table, uint64_t hash, uint64_t nonce,
                           int file, uint64_t *matched_nonce)
{
    // add an int to represent which file in the unused 16 bits of the uint64_t
    uint64_t tag = ((uint64_t)file << 48) | hash;

    // linear probing -- toy_hash is well mixed already, so the low bits of
    // the hash make a fine starting index
    uint64_t i = hash & TABLE_MASK;
    for (; table[i].tag != 0; i = (i + 1) & TABLE_MASK) {
        if (table[i].tag == tag) {
            return 0; // this file already produced this hash
        }
        if ((table[i].tag & HASH_MASK) == hash) {
            *matched_nonce = table[i].nonce; // the other file got there first
            return 1;
        }
    }

    table[i].tag = tag;
    table[i].nonce = nonce;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <pdf_file1> <pdf_file2>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *path1        = argv[1];
    const char *path2        = argv[2];
    const char *sn           = "22709404";

    if (mkdir(SOLVED_DIR, 0755) != 0 && errno != EEXIST) {
        perror(SOLVED_DIR);
        return EXIT_FAILURE;
    }

    size_t size1; 
    size_t size2;

    unsigned char *pdf1 = load_pdf(path1, &size1);
    unsigned char *pdf2 = load_pdf(path2, &size2);
    if (!pdf1 || !pdf2) {
        free(pdf1);
        free(pdf2);
        return EXIT_FAILURE;
    }

    set_student_number(pdf1, sn);
    if (!verify_sn(pdf1, sn)) {
        fprintf(stderr, "Write of student ID to %s failed.\n", path1);
        free(pdf1);
        free(pdf2);
        return EXIT_FAILURE;
    }

    set_student_number(pdf2, sn);
    if (!verify_sn(pdf2, sn)) {
        fprintf(stderr, "Write of student ID to %s failed.\n", path1);
        free(pdf1);
        free(pdf2);
        return EXIT_FAILURE;
    }

    // TEST HASHES HERE
    // The collision table. calloc zeroes every slot, and zero means empty 
    HashEntry *hash_table = calloc(TABLE_CAP, sizeof(HashEntry));
    if (!hash_table) {
        fprintf(stderr, "out of memory for the hash table (%llu slots)\n",
                (unsigned long long)TABLE_CAP);
        free(pdf1);  
        free(pdf2);
        return EXIT_FAILURE;
    }

    uint64_t nonce_min = 0;
    uint64_t nonce_max = 2ULL << 47;
    int found = 0;            // (Y1/N0)
    uint64_t n1 = 0, n2 = 0; // matched nonce pair
    // stop once the table is nearly full, the birthday
    // paradox hd more than a fair shot atp (>~99.99%, 50M pairs)
    uint64_t loop_upper_bound = MAX_TRIES;

    struct timespec start_time, end_time;
    timespec_get(&start_time, TIME_UTC);

    for (uint64_t i = 0; i < loop_upper_bound; i++) {
        // Assign both PDFs to nonces which start on opposite ends of the range,
        // the min then increments and the max decrements with each iteration
        set_nonce(pdf1, nonce_min);
        set_nonce(pdf2, nonce_max);

        // hash updated PDFs and run each one through the table. the
        // other file having already spat out that hash means we stop
        // computing the same-try case; pdf1 lands in the table before 
        // pdf2 rocks up to the bbq
        uint64_t hash_pdf1 = toy_hash(pdf1, size1);
        uint64_t hash_pdf2 = toy_hash(pdf2, size2);

        uint64_t matched_nonce; // the other file's nonce that we collided with
        if (insert_or_match(hash_table, hash_pdf1, nonce_min, PDF1, &matched_nonce)) {
            n1 = nonce_min;
            n2 = matched_nonce;
            found = 1;
            break;
        }
        if (insert_or_match(hash_table, hash_pdf2, nonce_max, PDF2, &matched_nonce)) {
            n2 = nonce_max;
            n1 = matched_nonce;
            found = 1;
            break;
        }

        nonce_min++;
        nonce_max--;
    }

    timespec_get(&end_time, TIME_UTC);
    double time_difference = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    free(hash_table);
    if (!found) {
        fprintf(stderr, "no collision found after %llu tries\n",
               (unsigned long long)loop_upper_bound);
    free(pdf1);
    free(pdf2);
    return EXIT_FAILURE;
}

    // buffers hold last tried nonces, not the matched pair
    // rearm them w the winners before anything else is written
    set_nonce(pdf1, n1);
    set_nonce(pdf2, n2);


    char out1[1024]; char out2[1024];
    solved_path(path1, out1, sizeof(out1));
    solved_path(path2, out2, sizeof(out2));

    uint64_t h1 = toy_hash(pdf1, size1);
    uint64_t h2 = toy_hash(pdf2, size2);
    if (h1 != h2) {
        fprintf(stderr, "internal error: matched pair failed re-verification "
                    "(%012llx vs %012llx)\n",
                    (unsigned long long)h1, (unsigned long long)h2);
        free(pdf1);
        free(pdf2);
        return EXIT_FAILURE;
    }

    printf("collision verified: nonce1=%llu nonce2=%llu shared hash=%012llx\n",
        (unsigned long long)n1, (unsigned long long)n2, (unsigned long long)h1);
    printf("elapsed nonce calculation time: %f\n", time_difference);


    if (!save_pdf(out1, pdf1, size1)) {
        free(pdf1);
        free(pdf2);
        return EXIT_FAILURE;
    }
    if (!save_pdf(out2, pdf2, size2)) {
        free(pdf1);
        free(pdf2);
        return EXIT_FAILURE;
    }

    free(pdf1);
    free(pdf2);
    return EXIT_SUCCESS;
}
