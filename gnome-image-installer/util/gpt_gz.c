#include "gpt_gz.h"

int read_from_gzip(FILE *in_file, struct ptable *out_pt)
{
    gzFile file;
    uint8_t buffer[CHUNK_SIZE];
    int bytes_read;
    int size;

    if(NULL == in_file || NULL == out_pt) {
        return GPT_ERROR_NULL_INPUT;;
    }

    file = gzdopen(fileno(in_file), "r");
    if(file==0) {
        return GPT_ERROR_GZIP_OPEN;
    }
    bytes_read = gzread(file, buffer, CHUNK_SIZE);
    gzclose(file);
    
    size = sizeof(*out_pt); // should be 2048
    if(bytes_read < size) {
        // not enough bytes read
        return GPT_ERROR_INVALID_GZIP;
    }
    memset(out_pt, 0, size);
    memcpy(out_pt, buffer, size);

    // debug prints, we can as well return now
    //print_gpt_data(out_pt);
    return GPT_SUCCESS;
}

uint64_t get_gzip_disk_image_size(const char *filepath)
{
    if(NULL == filepath) return 0;
    FILE *in_file = fopen(filepath, "r");
    if(NULL == in_file) return 0;
    struct ptable pt;
    if(read_from_gzip(in_file, &pt) == GPT_SUCCESS) {
        return get_disk_size(&pt);
    }
    // error reading from disk
    return 0;
}

int get_gzip_is_valid_eos_gpt(const char *filepath)
{
    if(NULL == filepath) return 0;
    FILE *in_file = fopen(filepath, "r");
    if(NULL == in_file) return 0;
    struct ptable pt;
    if(read_from_gzip(in_file, &pt) == GPT_SUCCESS) {
        return is_eos_gpt_valid(&pt);
    }
    // error reading from disk
    return 0;
}
