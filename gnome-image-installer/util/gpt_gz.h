#ifndef _GPT_GZ_H_
#define _GPT_GZ_H_

#include <zlib.h>
#include "gpt.h"

int read_from_gzip(FILE *in_file, struct ptable *out_pt);

// helper function
int get_gzip_is_valid_eos_gpt(const char *filepath, uint64_t *size);

#endif // _GPT_GZ_H_
