#ifndef _GPT_H_
#define _GPT_H_

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#include "gpt_errors.h"

//#define DEBUG_PRINTS

#define CHUNK_SIZE 2048

// for windows
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define SECTOR_SIZE 512
#define GPT_HEADER_SIZE 92
#define GPT_PART_SIZE 128


struct gpt_header
{
    uint8_t     signature[8];
    uint32_t    revision;
    uint32_t    header_size;//should be 92
    uint32_t    crc;
    uint32_t    reserved;
    uint64_t    current_lba;
    uint64_t    backup_lba;
    uint64_t    first_usable_lba;
    uint64_t    last_usable_lba;
    uint8_t     disk_guid[16];
    uint64_t    ptable_starting_lba;
    uint32_t    ptable_count;
    uint32_t    ptable_partition_size;//should be 128
    uint32_t    ptable_crc;
    uint8_t     padding[512-GPT_HEADER_SIZE];
} __attribute__((packed));

struct gpt_partition
{
    uint8_t    type_guid[16];
    uint8_t    part_guid[16]; 
    uint64_t   first_lba;
    uint64_t   last_lba;
    uint8_t    attributes[8];
    uint8_t    name[72];
} __attribute__((packed));

struct ptable {
    uint8_t mbr[SECTOR_SIZE];
    struct gpt_header header;
    struct gpt_partition partitions[4]; // we only care about the first 4 partitions   
} __attribute__((packed));

int is_eos_gpt_valid(struct ptable *pt, uint64_t *size);
uint8_t is_nth_flag_set(uint64_t flags, uint8_t n);

// helper function
int get_is_valid_eos_gpt(const char *filepath, uint64_t *size);

#ifdef DEBUG_PRINTS
void attributes_to_ascii(const uint8_t *attr, char *s);
void guid_to_ascii(const uint8_t *guid, char *s);
void gpt_header_show(const char *msg, const struct gpt_header *header);
void u16_to_ascii(const uint8_t *u, char *s);
void printHex(const uint8_t size, const uint8_t *ptr);
void gpt_part_show(uint32_t idx, const struct gpt_partition *part);
void print_nth_flag(uint8_t *attr, uint8_t n);
void print_gpt_data(struct ptable *pt);
#endif

#endif //_GPT_H_
