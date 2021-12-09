#include <glib.h>

#include "config.h"
#include "gpt.h"
#include "crc32.h"

static uint8_t GPT_GUID_EFI[] = {0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11, 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b};
static uint8_t GPT_GUID_LINUX_DATA[] = {0xaf, 0x3d, 0xc6, 0x0f, 0x83, 0x84, 0x72, 0x47, 0x8e, 0x79, 0x3d, 0x69, 0xd8, 0x47, 0x7d, 0xe4};
static uint8_t GPT_GUID_LINUX_ROOTFS1[] = {0x40, 0x95, 0x47, 0x44, 0x97, 0xf2, 0x41, 0xb2, 0x9a, 0xf7, 0xd1, 0x31, 0xd5, 0xf0, 0x45, 0x8a};
static uint8_t GPT_GUID_LINUX_ROOTFS2[] = {0xe3, 0xbc, 0x68, 0x4f, 0xcd, 0xe8, 0xb1, 0x4d, 0x96, 0xe7, 0xfb, 0xca, 0xf9, 0x84, 0xb7, 0x09};
static uint8_t GPT_GUID_LINUX_ROOTFS3[] = {0x10, 0xd7, 0xda, 0x69, 0xe4, 0x2c, 0x3c, 0x4e, 0xb1, 0x6c, 0x21, 0xa1, 0xd4, 0x9a, 0xbe, 0xd3};
static uint8_t GPT_GUID_LINUX_ROOTFS4[] = {0x45, 0xb0, 0x21, 0xb9, 0xf0, 0x1d, 0xc3, 0x41, 0xaf, 0x44, 0x4c, 0x6f, 0x28, 0x0d, 0x3f, 0xae};

uint8_t is_nth_flag_set(uint64_t flags, uint8_t n)
{
    return (uint8_t)((flags >> n) & 1UL);
}

#ifdef DEBUG_PRINTS
void attributes_to_ascii(const uint8_t *attr, char *s)
{
    sprintf(s, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
            attr[0], attr[1], attr[2], attr[3],
            attr[4], attr[5], attr[6], attr[7]
            );
}

void guid_to_ascii(const uint8_t *guid, char *s)
{
    uint32_t p1;
    uint16_t p2;
    uint16_t p3;
    unsigned char p4[8];

    memcpy(&p1, guid + 0, 4);
    memcpy(&p2, guid + 4, 2);
    memcpy(&p3, guid + 6, 2);
    memcpy(p4, guid + 8, 8);

    sprintf(s, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            p1, p2, p3, p4[0], p4[1],
            p4[2], p4[3], p4[4], p4[5], p4[6], p4[7]);
}

void gpt_header_show(const char *msg, const struct gpt_header *header)
{
    char guidstr[80];

    guid_to_ascii(header->disk_guid, guidstr);

    printf("%s:"
           "  size=%lu\n"
           "  revision=%x\n"
           "  current_lba=%llu\n"
           "  backup_lba=%llu\n"
           "  first_usable_lba=%llu\n"
           "  last_usable_lba=%llu\n"
           "  guid=%s\n"
           "  ptbl_lba=%llu\n"
           "  ptbl_count=%lu\n"
           "  ptbl_entry_size=%lu\n",
           msg,
           (unsigned long)header->header_size,
           header->revision,
           (unsigned long long)header->current_lba,
           (unsigned long long)header->backup_lba,
           (unsigned long long)header->first_usable_lba,
           (unsigned long long)header->last_usable_lba,
           guidstr,
           (unsigned long long)header->ptable_starting_lba,
           (unsigned long)header->ptable_count,
           (unsigned long)header->ptable_partition_size);
}

void u16_to_ascii(const uint8_t *u, char *s)
{
    uint32_t n;
    for (n = 0; n < 72/2; ++n) {
        s[n] = u[n*2];
    }
    s[72/2] = '\0';
}

void printHex(const uint8_t size, const uint8_t *ptr)
{
    int i=0;
    for(i=0; i<size; i++) {
        printf("%02x", ptr[i]);
    }
    printf("\n");
}

void gpt_part_show(uint32_t idx, const struct gpt_partition *part)
{
    char name[72+1];
    char attributes[25];
    uint64_t start, end, size;
    char tguidstr[80];
    char pguidstr[80];

    memset(name, 0, 73);

    guid_to_ascii(part->type_guid, tguidstr);
    guid_to_ascii(part->part_guid, pguidstr);

    u16_to_ascii(part->name, name);
    attributes_to_ascii(part->attributes, attributes);
    start = part->first_lba;
    end = part->last_lba;
    size = end - start + 1;
    printf("  p%-2u: [%8llu..%8llu] size=%8llu name=%s attributes=%s\n\t\t"
            "type guid=%s\n\t\tpart guid=%s\n\t\traw name=%s\n",
           idx,
           (unsigned long long)start,
           (unsigned long long)end,
           (unsigned long long)size,
           name, attributes,
           tguidstr, pguidstr,
           part->name
           );
    printf("name hex=");
    printHex(72, part->name);
}

void print_nth_flag(uint8_t *attr, uint8_t n)
{
    uint64_t flags;
    memcpy(&flags, attr, 8);
    uint8_t b = is_nth_flag_set(flags, 55);
    printf("is attr %d set? answ=%d (%s)\n", n, b, b?"yes":"no");
}

void print_gpt_data(struct ptable *pt)
{
    gpt_header_show("gpt header: ", &pt->header);
    for(int i=0; i<4; i++) {
        gpt_part_show(i, &pt->partitions[i]);
    }
    // now let's print the partition data
    print_nth_flag(pt->partitions[2].attributes, 55);
    // now let's test for validity
    printf("\nis gpt valid? (%s)\n\n", is_eos_gpt_valid(pt)?"yes":"no");
}
#endif

static uint64_t get_disk_size(struct ptable *pt)
{
    if(NULL==pt) return 0;
    return SECTOR_SIZE +    // mbr
        SECTOR_SIZE +       // gpt header
        pt->header.ptable_count * pt->header.ptable_partition_size + //size of partition table
        pt->header.last_usable_lba * SECTOR_SIZE; // rest of the usable disk size
}

/**
 * is_eos_gpt_valid:
 * @size: (out) (optional): location to store the disk size, in bytes, if the
 *  GPT is valid
 *
 * Checks the GPT for validity.
 *
 * Returns: 1 if the GPT is valid, 0 otherwise
 */
int is_eos_gpt_valid(struct ptable *pt, uint64_t *size)
{
    size_t i = 0;

    if(NULL==pt) return 0;

    if(memcmp(pt->header.signature, "EFI PART", 8)!=0) {
        g_warning("invalid signature");
        return 0;
    }
    if(pt->header.revision != 0x00010000) {
        g_warning("invalid revision");
        return 0;
    }
    if(pt->header.header_size != GPT_HEADER_SIZE) {
        g_warning("invalid header size");
        return 0;
    }
    if(pt->header.reserved != 0) {
        g_warning("reserved bytes must be 0");
        return 0;
    }
    if(pt->header.ptable_starting_lba != 2) {
        g_warning("starting LBA should always be 2");
        return 0;
    }
    if(pt->header.ptable_partition_size != 128) {
        g_warning("invalid partition table entry size");
        return 0;
    }
    if(pt->header.ptable_count < 2 ) {
        //  Disk images must have at least 2 partitions: the ESP and the OS
        //  partition. Endless OS images have an additional BIOS Boot partition
        //  in between, but GNOME OS images (for example) do not.
        g_warning("not enough partitions");
        return 0;
    }
    for(i=0; i<512-GPT_HEADER_SIZE; i++) {
        if(pt->header.padding[i] != 0) {
            g_warning("GPT header padding must be zeroed");
            return 0;
        }
    }
    //  crc32 of header, with 'crc' field zero'ed
    struct gpt_header testcrc_header;
    memset(&testcrc_header, 0, GPT_HEADER_SIZE);
    memcpy(&testcrc_header, &pt->header, GPT_HEADER_SIZE);
    testcrc_header.crc = 0;
    if(calc_crc32((uint8_t*)(&testcrc_header), GPT_HEADER_SIZE)!=pt->header.crc) {
        g_warning("invalid header crc");
        return 0;
    }
    //  crc32 of partition table
    int n = pt->header.ptable_count * pt->header.ptable_partition_size;
    uint8_t *buffer = (uint8_t*)malloc(n);
    memset(buffer, 0, n);
    for(i=0; i<3; i++) { // only first 3 partitions are populated, everything else is zero
        memcpy(buffer+(i*pt->header.ptable_partition_size), (uint8_t*)(&pt->partitions[i]), pt->header.ptable_partition_size);
    }
    if(calc_crc32(buffer, n) != pt->header.ptable_crc) {
        g_warning("invalid partition table crc");
        free(buffer);
        return 0;
    }
    free(buffer);

    // The first partition must be an EFI System Partition
    if(memcmp(&pt->partitions[0].type_guid, GPT_GUID_EFI, 16)!=0) {
        g_warning("first partition must be ESP");
        return 0;
    }

    // A subsequent partition must be a Linux rootfs.
    int has_root = 0;
    for (i = 1; i < pt->header.ptable_count; ++i) {
      if (memcmp(&pt->partitions[i].type_guid, GPT_GUID_LINUX_DATA, 16)==0
          || memcmp(&pt->partitions[i].type_guid, GPT_GUID_LINUX_ROOTFS1, 16)==0
          || memcmp(&pt->partitions[i].type_guid, GPT_GUID_LINUX_ROOTFS2, 16)==0
          || memcmp(&pt->partitions[i].type_guid, GPT_GUID_LINUX_ROOTFS3, 16)==0
          || memcmp(&pt->partitions[i].type_guid, GPT_GUID_LINUX_ROOTFS4, 16)==0) {
        uint64_t flags = 0;
        memcpy(&flags, pt->partitions[i].attributes, 8);
        if(!is_nth_flag_set(flags, 55)) {
          //  55th flag must be 1 for EOS images
          continue ;
        }
        has_root=1;
        break ;
      }
    }
    if (!has_root) {
      g_warning("no root partition found");
      return 0;
    }

    if (size != NULL) {
        *size = get_disk_size(pt);
    }
    return 1; // success, GPT is valid
}

int get_is_valid_eos_gpt(const char *filepath, uint64_t *size)
{
    if(NULL == filepath) return 0;
    FILE *in_file = fopen(filepath, "r");
    if(NULL == in_file) return 0;
    struct ptable pt;
    if(fread(&pt, sizeof(pt), 1, in_file) == 1) {
        fclose(in_file);
        return is_eos_gpt_valid(&pt, size);
    }
    // error reading from disk
    fclose(in_file);
    return 0;
}
