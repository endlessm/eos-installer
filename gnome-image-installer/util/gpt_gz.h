#ifndef _GPT_GZ_H_
#define _GPT_GZ_H_

#include <zlib.h>
#include "gpt.h"

int read_from_gzip(FILE *in_file, struct ptable *out_pt);

// helper function
uint64_t get_gzip_disk_image_size(const char *filepath);
int get_gzip_is_valid_eos_gpt(const char *filepath);

#endif // _GPT_GZ_H_
