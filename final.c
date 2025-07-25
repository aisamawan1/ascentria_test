#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#pragma pack(push, 1)
typedef struct
{
    uint16_t filename_id;
    uint32_t sequence_in_file;
    uint16_t number_records;
    uint16_t data_length;
    uint16_t flags;
    uint8_t compression_type;
    uint8_t spare;
    uint8_t data[2048 - 14];
} flashdata;
#pragma pack(pop)

char nibble_to_char(uint8_t nib)
{
    if (nib <= 9)
        return '0' + nib;
    else if (nib == 0xA)
        return ' ';
    else if (nib == 0xB)
        return ':';
    else if (nib == 0xC)
        return '/';
    else if (nib == 0xD)
        return '\r';
    else if (nib == 0xE)
        return '\n';
}

void decompress_nibbles(uint8_t *input, int length, FILE *output)
{
    for (int i = 0; i < length; i++)
    {
        uint8_t byte = input[i];
        uint8_t low = byte & 0x0F;
        uint8_t high = byte >> 4;

        if (low == 0x8)
        {
            for (int j = 0; j < high; j++)
            {
                fputc(' ', output);
            }
            continue;
        }

        if (low == 0xF)
        {
            if (i + 1 >= length)
                break;
            uint8_t next = input[++i];
            uint8_t low2 = next & 0x0F;
            uint8_t high2 = next >> 4;
            uint8_t literal = (high << 4) | low2;
            fputc(literal, output);
            fputc(nibble_to_char(high2), output);
            continue;
        }
        else
        {
            fputc(nibble_to_char(low), output);
        }

        if (high == 0xF)
        {
            if (i + 1 >= length)
                break;
            uint8_t next = input[++i];
            uint8_t low2 = next & 0x0F;
            uint8_t high2 = next >> 4;
            uint8_t literal = (low2 << 4) | high2;
            fputc(nibble_to_char(low), output);
            fputc(literal, output);
            continue;
        }
        else
        {
            fputc(nibble_to_char(high), output);
        }
    }
}

int hex_line_to_bytes(char *line, uint8_t *output)
{
    int count = 0;
    char *token = strtok(line, " \r\n");

    while (token && count < 256)
    {
        output[count++] = (uint8_t)strtol(token, NULL, 16);
        token = strtok(NULL, " \r\n");
    }
    return count;
}

void extract_block(uint8_t *block_bytes, FILE *out)
{
    flashdata *block = (flashdata *)block_bytes;

    if (block->filename_id != 0x0002 || block->data_length > 2048 - 14)
        return;
    int offset = 0;
    for (int i = 0; i < block->number_records; i++)
    {
        if (offset + 3 >= block->data_length)
            break;
        uint8_t *rec = block->data + offset;
        int rec_len = rec[0];
        if (rec_len < 3 || offset + rec_len > block->data_length)
            break;
        uint8_t *compressed = rec + 3;
        int comp_len = rec_len - 3;
        decompress_nibbles(compressed, comp_len, out);
        offset += rec_len;
    }
}

int main()
{
    FILE *fptr;
    FILE *fout;
    char data[256];
    uint8_t proceed = 0;
    fptr = fopen("INTDATA", "r");
    fout = fopen("output.txt", "w");

    if(fout == NULL)
    {
        printf("Output file failed to open.");
    }

    if (fptr == NULL)
    {
        printf("INTDATA file failed to open.");
    }
    else
    {
        printf("The file is now opened.\n");
        uint8_t block_buffer[2033] = {0};
        int block_index = 0;

        while (fgets(data, sizeof(data), fptr))
        {
            if (strncmp(data, "BLK", 3) == 0)
            {
                block_index = 0;
                fgets(data, sizeof(data), fptr);
                continue;
            }
            uint8_t bytes[256];
            int byte_count = hex_line_to_bytes(data, bytes);
            if (block_index + byte_count <= 2033)
            {
                memcpy(block_buffer + block_index, bytes, byte_count);
                block_index += byte_count;
            }
            else
            {
                extract_block(block_buffer, fout);
                block_index = 0;
            }
        }
        fclose(fptr);
    }
    return 0;
}