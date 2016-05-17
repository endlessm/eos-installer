#include "gpt.h"

#ifdef DEBUG_PRINTS
void attributes_to_ascii(const uint8_t *attr, char *s)
{
    sprintf(s, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
            attr[0], attr[1], attr[2], attr[3],
            attr[4], attr[5], attr[6], attr[7]
            );
}

uint8_t is_nth_flag_set(uint64_t flags, uint8_t n) 
{
    return (uint8_t)((flags >> n) & 1UL);
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
}
#endif

uint64_t get_disk_size(struct ptable *pt)
{
    if(NULL==pt) return 0;
    return SECTOR_SIZE +    // mbr
        SECTOR_SIZE +       // gpt header
        pt->header.ptable_count * pt->header.ptable_partition_size + //size of partition table
        pt->header.last_usable_lba * SECTOR_SIZE; // rest of the usable disk size
}
