#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>

struct header {
    char magic[4]; // S3P0
    uint32_t entries;
};

struct entry {
    uint32_t offset;
    uint32_t length;
};

// actually 64 bytes but we don't care about the other fields (yet!)
struct s3v0 {
    char magic[4]; // S3V0
    uint32_t filestart;
};

void convert(const char* path) {
    printf("%s\n", path);
    
    FILE* f = fopen(path, "rb");
    if(!f) {
        printf("Couldn't open!\n");
        return;
    }
    
    char* out = malloc(strlen(path) + strlen(".out") + 1);
    sprintf(out, "%s.out", path);
    
    if(mkdir(out, 0) && errno != EEXIST) {
        printf("Couldn't make out dir\n");
        goto CLEANUP;
    }
    
    struct header h;
    fread(&h, sizeof(h), 1, f);
    if(memcmp(h.magic, "S3P0", 4)) {
        printf("Bad magic!\n");
        goto CLEANUP;
    }
    
    struct entry *entries = malloc(sizeof(struct entry) * h.entries);
    fread(entries, sizeof(struct entry), h.entries, f);
    
    for(uint32_t i = 0; i < h.entries; i++) {
        printf("%f%%\n", (float)(i+1)/(float)h.entries * 100);
        char* out_file = malloc(strlen(out) + 100);
        sprintf(out_file, "%s/%d.wma", out, i+1);
        FILE* out_f = fopen(out_file, "wb");
        free(out_file);
        if(!out_f) {
            printf("Couldn't open output %s\n", out_file);
            goto CLEANUP;
        }
        
        fseek(f, entries[i].offset, SEEK_SET);
        void* buffer = malloc(entries[i].length);
        fread(buffer, 1, entries[i].length, f);
        
        struct s3v0 *file_header = (struct s3v0*)buffer;
        if(memcmp(file_header->magic, "S3V0", 4)) {
            printf("Bad magic! Need S3V0 got %c%c%c%c\n",
                file_header->magic[0],
                file_header->magic[1],
                file_header->magic[2],
                file_header->magic[3]
            );
            free(buffer);
            goto CLEANUP;
        }
        
        fwrite(buffer + file_header->filestart, 1, entries[i].length - file_header->filestart, out_f);
        free(buffer);
        fclose(out_f);
    }
    
    CLEANUP:
        fclose(f);
        free(out);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("Usage: s3p_extract file.s3p [file2.s3p] [file3.s3p]\n");
        return 1;
    }
    for(int i = 1; i < argc; i++) {
        convert(argv[i]);
    }
    return 0;
}