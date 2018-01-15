#ifndef _GPT_LZMA_H_
#define _GTP_LZMA_H_

#include <lzma.h>
#include "gpt.h"

int read_from_xz(FILE *in_file, struct ptable *out_pt);

// helper function
int get_xz_is_valid_eos_gpt(const char *filepath, uint64_t *size);

#endif // _GPT_LZMA_H_
