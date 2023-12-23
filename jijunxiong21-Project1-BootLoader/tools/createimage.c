#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_FILE "./image" // image file name
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..." // how to use this program

#define SECTOR_SIZE 512 // sector size
#define BOOT_LOADER_SIG_OFFSET 0x1fe // boot loader signature offset
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2) // os size location, storing os size by 2 bytes
#define DECMP_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 4) // decompress size location, storing decompress size by 2 bytes. sectors
#define DECMP_START_LOC (BOOT_LOADER_SIG_OFFSET - 6) // decompress start position location (sec). 2bytes. sectors
#define DECMP_START_OFF (BOOT_LOADER_SIG_OFFSET - 8)
#define KERNEL_SZ_OFF   (BOOT_LOADER_SIG_OFFSET - 10)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0)) // convert number of bytes to sectors. sector := nbytes / 512 + 1 [if nbytes % 512 != 0]. e.g. 1024 bytes -> 2 sectors, 1025 bytes -> 3 sectors. This can be used to calculate the last sector occupied by certain program (where to padding) by given address

#define MAX_TASK_NAME_LEN 32 // max length of task name

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    int entry;    // entry point
    int filesz;   // actual size of program
    int phyaddr;  // start address in SD card (read as file)
    char task_name[MAX_TASK_NAME_LEN];
} task_info_t;

#define TASK_MAXNUM 16 // max number of tasks
static task_info_t taskinfo[TASK_MAXNUM]; // task info array. will be used later

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

// data to be compressed
#define BUFFER_SIZE 30000
char data[BUFFER_SIZE];
char compressed[BUFFER_SIZE];
char extracted[BUFFER_SIZE];
// int data_len = strlen(data);
// struct libdeflate_compressor *compressor;
// struct libdeflate_decompressor *decompressor;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_segment_comp(int data_sz, FILE *img, int *phyaddr, int *out_nbytes);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img);
static void write_para_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE * img, short decmp_sec_off, short decmp_begin_sec, short decmp_sec_num);

int main(int argc, char **argv)
{
    char *progname = argv[0]; // program name is the first argument

    /* process command line options */
    options.vm = 0; // default: not using vm
    options.extended = 0; // default: not printing extended info
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) { // parsing options by cmd line
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1); // create image. abandon the first argument (program name)
    return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;     // number of tasks
    int nbytes_kernel = 0;        // number of bytes of kernel
    int phyaddr = 0;              // physical address
    FILE *fp = NULL, *img = NULL; // file pointer of input file and image file
    Elf64_Ehdr ehdr;              // ELF header (first part)
    Elf64_Phdr phdr;              // program header (second part)

    int filesz_tmp;
    int out_nbytes;

    // // do compress
    // int out_nbytes = deflate_deflate_compress(compressor, data, data_len, compressed, BUFFER_SIZE);
    // printf("original: %d\n", data_len);
    // printf("compressed: %d\n", out_nbytes);

    // // do decompress
    // int restore_nbytes = 0;
    // if(deflate_deflate_decompress(decompressor, compressed, out_nbytes, extracted, data_len, &restore_nbytes)){
    //     printf("An error occurred during decompression.\n");
    //     exit(1);
    // }
    // printf("decompressed: %d\n%s\n", restore_nbytes, extracted);



    /* open the image file */
    img = fopen(IMAGE_FILE, "w"); // open image file (./image)
    assert(img != NULL);          // check whether image file is opened successfully

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 2;
        int data_sz = 0;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD) continue;

            // /* write segment to the image */
            // write_segment(phdr, fp, img, &phyaddr);
            if (strcmp(*files, "bootblock") == 0) {
                /* write segment to the image */
                write_segment(phdr, fp, img, &phyaddr);
            }


            fseek(fp, phdr.p_offset, SEEK_SET);
            filesz_tmp = phdr.p_filesz;
            phyaddr += filesz_tmp;
            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                for (int i = 0; i < filesz_tmp; ++i) {
                    data[nbytes_kernel++] = fgetc(fp);
                }
                // fread(data + nbytes_kernel, get_filesz(phdr), 1, fp);
                // nbytes_kernel += get_filesz(phdr);
            }
            else if (taskidx >= 0){
                for (int i = 0; i < filesz_tmp; ++i) {
                    data[data_sz++] = fgetc(fp);
                }
                // fread(data + data_sz, get_filesz(phdr), 1, fp);
                // data_sz += get_filesz(phdr); // calcul ate task file size
            }
        }

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */

        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }

        // write kernel to image file
        else if (strcmp(*files, "main") == 0) {
            // write_padding(img, &phyaddr, nbytes_kernel + SECTOR_SIZE);
            write_segment_comp(nbytes_kernel, img, &phyaddr, &out_nbytes);
        }

        // write app to image file
        else if (taskidx < TASK_MAXNUM) {
            write_segment_comp(data_sz, img, &phyaddr, &out_nbytes);
            if (taskidx == 0) {
                taskinfo[0].phyaddr = nbytes_kernel + SECTOR_SIZE;
            }
            else {
                taskinfo[taskidx].phyaddr = taskinfo[taskidx - 1].phyaddr + taskinfo[taskidx - 1].filesz;
            }

            taskinfo[taskidx].entry = get_entrypoint(ehdr);
            taskinfo[taskidx].filesz = out_nbytes;
            strcpy(taskinfo[taskidx].task_name, *files);
            taskinfo[taskidx].task_name[MAX_TASK_NAME_LEN - 1] = '\0'; // in case of overflow
        }
        else if (taskidx >= TASK_MAXNUM) {
            error("too many tasks\n");
        }

        fclose(fp);
        files++;
    }

    // check tasks info
    for (int i = 0; i < tasknum; i++)
    {
        printf("%x %x %x %s\n", taskinfo[i].entry, taskinfo[i].phyaddr, taskinfo[i].filesz, taskinfo[i].task_name);
    }

    write_img_info(nbytes_kernel, taskinfo, tasknum, img);
    phyaddr += TASK_MAXNUM * sizeof(task_info_t) + sizeof(int); // to write decompressor


    int decmp_sz = 0;
    short decmp_begin_sec = phyaddr / SECTOR_SIZE;
    short decmp_sec_off = phyaddr % SECTOR_SIZE;
    /* open input file */
    fp = fopen("decompress", "r");
    assert(fp != NULL);

    /* read ELF header */
    read_ehdr(&ehdr, fp);
    printf("0x%04lx: %s\n", ehdr.e_entry, *files);

    /* for each program header */
    for (int ph = 0; ph < ehdr.e_phnum; ph++) {

        /* read program header */
        read_phdr(&phdr, fp, ph, ehdr);

        if (phdr.p_type != PT_LOAD) continue;

        /* write segment to the image */
        write_segment(phdr, fp, img, &phyaddr);
        decmp_sz += get_filesz(phdr);
    }

    short decmp_end_sec = NBYTES2SEC(phyaddr);
    short decmp_sec_num = decmp_end_sec - decmp_begin_sec;

    write_para_info(nbytes_kernel, taskinfo, tasknum, img, decmp_sec_off, decmp_begin_sec, decmp_sec_num);


    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

// write segment to image file
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

// write segment to image file
static void write_segment_comp(int data_sz, FILE *img, int *phyaddr, int *out_nbytes)
{
    // prepare environment
    deflate_set_memory_allocator((void * (*)(int))malloc, free);
    struct libdeflate_compressor * compressor = deflate_alloc_compressor(1);
    // struct libdeflate_decompressor * decompressor = deflate_alloc_decompressor();

    // do compress
    // int deflate_deflate_compress(struct libdeflate_compressor *compressor, const void *in, int in_nbytes, void *out, int out_nbytes_avail);
    *out_nbytes = deflate_deflate_compress(compressor, data, data_sz, compressed, BUFFER_SIZE);
    printf("original: %d\n", data_sz);
    printf("compressed: %d\n", *out_nbytes);

    fseek(img, *phyaddr, SEEK_SET);
    fwrite(compressed, *out_nbytes, 1, img);
    *phyaddr += data_sz;
}

// write padding bytes to image file
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE * img)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...

    // calculate app volume size
    short task_off = SECTOR_SIZE + nbytes_kernel;
    int tasknum_int = tasknum;

    for (int task_idx = 0; task_idx < tasknum; ++task_idx) {
        task_off += taskinfo[task_idx].filesz;
    }

    // write task numbers and app info array to the end of image file
    fseek(img, task_off, SEEK_SET); // locate to the end of image file
    fwrite(&tasknum_int, sizeof(int), 1, img); // write task number to image file
    fwrite(taskinfo, TASK_MAXNUM * sizeof(task_info_t), 1, img); // write task info to image file
}

static void write_para_info(int nbytes_kernel, task_info_t *taskinfo,short tasknum, FILE * img, 
                           short decmp_sec_off, short decmp_begin_sec, short decmp_sec_num)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...

    // convert kernel size to sectors
    short kernel_sz = (short)nbytes_kernel;
    short kernel_sec_num = NBYTES2SEC(nbytes_kernel);
    // calculate app volume size
    short task_off = SECTOR_SIZE + nbytes_kernel;

    for (int task_idx = 0; task_idx < tasknum; ++task_idx) {
        task_off += taskinfo[task_idx].filesz;
    }

    // write decmp size (to DECMP_SIZE_LOC)
    fseek(img, KERNEL_SZ_OFF, SEEK_SET); // locate to os size location, from the beginning of the file, offset is OS_SIZE_LOC
    fwrite(&kernel_sz, sizeof(short), 1, img);
    fwrite(&decmp_sec_off, sizeof(short), 1, img); // write the offset in the 1st sector
    fwrite(&decmp_begin_sec, sizeof(short), 1, img); // write the 1st sector of decompress ELF
    fwrite(&decmp_sec_num, sizeof(short), 1, img); // write decompress size
    fwrite(&kernel_sec_num, sizeof(short), 1, img); // write os size to image file
    // write app volume offset (to BOOT_LOADER_SIG_OFFSET)
    fwrite(&task_off, sizeof(short), 1, img); // write app volume size to image file
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
