#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>

#if defined(__linux__) || defined(__APPLE__)
    #define make_dir(path) mkdir(path, 0755)
#else
    #include <direct.h>
    #define make_dir(path) _mkdir(path)
#endif

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
    uint32_t length;
    
    uint8_t unknown[20];
};


void pack(int infile_count, char** infiles, char* out_filename){
    printf("Packing %d files\n", infile_count);

    FILE* out_f = fopen(out_filename, "wb");
    if (!out_f){
        printf("Couldn't create output file %s\n", out_filename);
        return;
    }

    struct header h = {
        .magic = "S3P0",
        .entries = infile_count
    };

    fwrite(&h, sizeof(h), 1, out_f);

    struct entry *entries = malloc(sizeof(struct entry) * infile_count);
    memset(entries, 0, sizeof(struct entry) * infile_count);
    fwrite(entries, sizeof(struct entry), infile_count, out_f);

    for(int i = 0; i < infile_count; i++){
        printf("Packing %s\n", infiles[i]);

        entries[i].offset = ftell(out_f);
        FILE* audio = fopen(infiles[i], "rb");
        if (!audio){
            printf("Error opening %s\n", infiles[i]);
            fclose(out_f);
            return;
        }

        int length = 0;
        fseek(audio, 0, SEEK_END);
        length = ftell(audio);
        rewind(audio);

        struct s3v0 audio_header= {
            .magic = "S3V0",
            .filestart = 0x20,
            .length = length
        };

        fwrite(&audio_header, sizeof(struct s3v0), 1, out_f);

        uint8_t* buffer = malloc(length);
        fread(buffer, 1, length, audio);
        fwrite(buffer, 1, length, out_f);

        entries[i].length = ftell(out_f) - entries[i].offset;
        
        free(buffer);
        fclose(audio);
    }
    
    uint32_t endbytes = 0x12345678; //Unclear what this value is for
    fwrite(&endbytes, sizeof(uint32_t), 1, out_f);

    //Repopulate the entries now that they have relevant offsets
    fseek(out_f, sizeof(struct header), 0);
    fwrite(entries, sizeof(struct entry), infile_count, out_f);

    fclose(out_f);
}

void convert(const char* path) {
    printf("%s\n", path);

    FILE* f = fopen(path, "rb");
    if(!f) {
        printf("Couldn't open!\n");
        return;
    }

    char* out = malloc(strlen(path) + strlen(".out") + 1);
    sprintf(out, "%s.out", path);

    if(make_dir(out) && errno != EEXIST) {
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
        sprintf(out_file, "%s/%d.wma", out, i);
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
    if(argc < 2 || strcmp(argv[argc - 1], "-o") == 0 || strcmp(argv[argc - 1], "-pack") == 0) {
        printf("Usage:\n");
        printf("Unpack: s3p_extract file.s3p [file2.s3p] [file3.s3p]\n");
        printf("Repack: s3p_extract -pack [-o filename.s3p] file.wma [file2.wma] [file3.wma]\n"); 
        return 1;
    }

    if (strcmp(argv[1], "-pack") == 0){
        if (strcmp(argv[2], "-o") == 0){
            if (argc < 5){
                printf("Enter at least one input file\n");
                return 1;
            }
            pack(argc - 4, &argv[4], argv[3]);
        }
        else{
            pack(argc-2, &argv[2], "out.s3p");
        }
        return 0;
    }

    for(int i = 1; i < argc; i++) {
        convert(argv[i]);
    }
    return 0;
}
