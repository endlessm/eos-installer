#ifndef _GPT_LZMA_H_
#define _GTP_LZMA_H_

#include <lzma.h>
#include "gpt.h"

int read_from_xz(FILE *in_file, struct ptable *out_pt);

// helper function
uint64_t get_xz_disk_image_size(const char *filepath);

#endif // _GPT_LZMA_H_
