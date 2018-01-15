#include "gpt_lzma.h"

// taken from the lzma documentation
static int init_decoder(lzma_stream *strm)
{
	// Initialize a .xz decoder. The decoder supports a memory usage limit
	// and a set of flags.
	//
	// The memory usage of the decompressor depends on the settings used
	// to compress a .xz file. It can vary from less than a megabyte to
	// a few gigabytes, but in practice (at least for now) it rarely
	// exceeds 65 MiB because that's how much memory is required to
	// decompress files created with "xz -9". Settings requiring more
	// memory take extra effort to use and don't (at least for now)
	// provide significantly better compression in most cases.
	//
	// Memory usage limit is useful if it is important that the
	// decompressor won't consume gigabytes of memory. The need
	// for limiting depends on the application. In this example,
	// no memory usage limiting is used. This is done by setting
	// the limit to UINT64_MAX.
	//
	// The .xz format allows concatenating compressed files as is:
	//
	//     echo foo | xz > foobar.xz
	//     echo bar | xz >> foobar.xz
	//
	// When decompressing normal standalone .xz files, LZMA_CONCATENATED
	// should always be used to support decompression of concatenated
	// .xz files. If LZMA_CONCATENATED isn't used, the decoder will stop
	// after the first .xz stream. This can be useful when .xz data has
	// been embedded inside another file format.
	//
	// Flags other than LZMA_CONCATENATED are supported too, and can
	// be combined with bitwise-or. See lzma/container.h
	// (src/liblzma/api/lzma/container.h in the source package or e.g.
	// /usr/include/lzma/container.h depending on the install prefix)
	// for details.
	lzma_ret ret = lzma_stream_decoder(
			strm, UINT64_MAX, LZMA_CONCATENATED);

	// Return successfully if the initialization went fine.
	if (ret == LZMA_OK)
		return GPT_SUCCESS;

	// Something went wrong. The possible errors are documented in
	// lzma/container.h (src/liblzma/api/lzma/container.h in the source
	// package or e.g. /usr/include/lzma/container.h depending on the
	// install prefix).
	//
	// Note that LZMA_MEMLIMIT_ERROR is never possible here. If you
	// specify a very tiny limit, the error will be delayed until
	// the first headers have been parsed by a call to lzma_code().
	return GPT_ERROR_LZMA_INIT_ERROR;
}

int read_from_xz(FILE *in_file, struct ptable *out_pt) 
{
    int ret = 0;
    lzma_stream strm = LZMA_STREAM_INIT;
    unsigned char in[CHUNK_SIZE];
    unsigned char out[CHUNK_SIZE];
    int bytes_no, size;

    if(NULL == in_file || NULL == out_pt) {
        return GPT_ERROR_NULL_INPUT;
    }
    SET_BINARY_MODE(in_file);

    ret = init_decoder(&strm);
    if(ret!=0) {
        // lzma init error
        return ret;
    }

	// When LZMA_CONCATENATED flag was used when initializing the decoder,
	// we need to tell lzma_code() when there will be no more input.
	// This is done by setting action to LZMA_FINISH instead of LZMA_RUN
	// in the same way as it is done when encoding.
	//
	// When LZMA_CONCATENATED isn't used, there is no need to use
	// LZMA_FINISH to tell when all the input has been read, but it
	// is still OK to use it if you want. When LZMA_CONCATENATED isn't
	// used, the decoder will stop after the first .xz stream. In that
	// case some unused data may be left in strm->next_in.
	lzma_action action = LZMA_RUN;

	strm.next_in = NULL;
	strm.avail_in = 0;
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	strm.next_in = in;
	strm.avail_in = fread(in, 1, sizeof(in), in_file);
    lzma_ret lret = lzma_code(&strm, action);

    bytes_no = CHUNK_SIZE - strm.avail_out;
    size = sizeof(*out_pt); // should be 2048
    if(bytes_no < size) {
        // not enought bytes read
        return GPT_ERROR_INVALID_LZMA;
    }
    memset(out_pt, 0, size);
    memcpy(out_pt, out, size);
   
    // cleanup
    action = LZMA_FINISH;
    lret = lzma_code(&strm, action);
    if(lret != LZMA_OK) {
        // error closing the stream. let's ignore it
    }
    fclose(in_file);
    lzma_end(&strm);

    return GPT_SUCCESS;
}

int get_xz_is_valid_eos_gpt(const char *filepath, uint64_t *size)
{
    if(NULL == filepath) return 0;
    FILE *in_file = fopen(filepath, "r");
    if(NULL == in_file) return 0;
    struct ptable pt;
    if(read_from_xz(in_file, &pt) == GPT_SUCCESS) {
        return is_eos_gpt_valid(&pt, size);
    }
    // error reading from disk
    return 0;
}
