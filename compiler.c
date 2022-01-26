#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <search.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define ARR_LEN(x) sizeof(x)/sizeof(x[0])

#define errMsg(msg) do {  \
    fprintf(stderr, msg); \
    exit(EXIT_FAILURE);   \
} while (0)


/*
 * instruction of
 * vm machine 
 */
typedef struct {
        char name[8];
        int instruction;
        int nargs;

} VM_INSTRUCTION;

/* instruction support */
static VM_INSTRUCTION vm_instructions[] = {
    { "noop",    0,     0 },
    { "iadd",    1,     0 },
    { "isub",    2,     0 },
    { "imul",    3,     0 },
    { "ilt",     4,     0 },
    { "ieq",     5,     0 },
    { "br",      6,     1 },
    { "brt",     7,     1 },
    { "brf",     8,     1 },
    { "iconst",  9,     1 },
    { "load",    10,    1 },
    { "gload",   11,    1 },
    { "store",   12,    1 },
    { "gstore",  13,    1 },
    { "print",   14,    0 },
    { "pop",     15,    0 },
    { "call",    16,    3 },
    { "ret",     17,    0 },
    { "halt",    18,    0 }
};

typedef struct {
    char **buf_ptr;
    size_t len;
} list_t;


void
compile(list_t *bytearray, char *file)
{
    char *addr;
    size_t cur_index, i = 0;
    char *str;
    char **ptr;
    struct stat status;
    int fd;


    if ((fd = open(file, O_RDONLY)) < 0)
        errMsg("open");
    
    // get file descriptor information 
    fstat(fd, &status);

    // map file in memory with 
    // read permission
    if ((addr = mmap(0, status.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
        errMsg("mmap");

    close(fd);


    /* 
     * parse string from memory mapped data 
     * and store in char ** array
     */
    ptr = (char **)malloc(sizeof(char *)*(i+1));
    if (ptr == NULL)
        errMsg("malloc");


    /*
     * if string start with # then it will
     * consumes whole string all string 
     * and does store any space for it.
     *
     * else we map string in array that is 
     * without C space characters
     */
    for (cur_index = 0; cur_index < status.st_size; cur_index++)
    {
        if (addr[cur_index] == '#')
            while (++cur_index < status.st_size &&
                    addr[cur_index] != '\n');


        else if (!isspace(addr[cur_index]))
        {
            unsigned size = cur_index;
            while (++cur_index < status.st_size && 
                    !isspace(addr[cur_index]));

            ptr[i++] = strndup(&addr[size], cur_index-size);
            ptr = realloc(ptr, (i+1)*sizeof(char *));
        }
    }
    ptr[i] = NULL;

    bytearray->len = i;
    bytearray->buf_ptr = ptr;

    /* free file from memory */
    if (munmap(addr, status.st_size) == -1)
        errMsg("munmap");

}


/* 
 * free all elements in 
 * array
 */

static void 
xfree(list_t *bytearray)
{
    unsigned tour = 0;
    while (bytearray->buf_ptr[tour] != NULL)
        free(bytearray->buf_ptr[tour++]);
    free(bytearray->buf_ptr);
}

/*
 * comparing vm->name for binary search
 * and qsort 
 */

static int
compar(const void *p1, const void *p2)
{
    VM_INSTRUCTION *m1 = (VM_INSTRUCTION *)p1;
    VM_INSTRUCTION *m2 = (VM_INSTRUCTION *)p2;

    return strcmp(m1->name, m2->name);
}


int 
main(int argc, char **argv)
{
    /* file output flag */
    bool output_flag = false;
    char *str;
    int *instruction_array;
    int cur_ptr = 0, opt;
    list_t bytearray;


    /*
     * parse argument
     * for 'o' argument
     * for storing as output file
     */
    while ((opt = getopt(argc, argv, "o:")) != -1)
    {
        switch (opt)
        {
            case 'o':
                output_flag = true;
                str = optarg;
                break;
            default:
                fprintf(stderr, "Usage argv > 0 [%s]:(\n", __FILE__);
                exit(EXIT_FAILURE);
        }
    }
    if (optind >= argc)
        errMsg("Invalid args :(\n");

    compile(&bytearray, argv[optind]);

    instruction_array = malloc(sizeof(int) * bytearray.len);
    if (instruction_array == NULL)
        errMsg("malloc");

    /* quick sorting instruction for fast access */
    qsort(vm_instructions, ARR_LEN(vm_instructions), sizeof(VM_INSTRUCTION), compar);

    /*
     * validate instruction one
     * after and another 
     * and also validate number
     * of argument for a instruction
     */
    for (cur_ptr = 0; cur_ptr < bytearray.len; ++cur_ptr)
    {
        VM_INSTRUCTION key;
        VM_INSTRUCTION *res;
        
        /* binary search for instruct name */
        sscanf(bytearray.buf_ptr[cur_ptr], "%7s", key.name);
        res = bsearch(&key, vm_instructions, ARR_LEN(vm_instructions), sizeof(VM_INSTRUCTION), compar);

        if (res == NULL)
        {
            fprintf(stderr, "Invalid Instruction \"%s\":(\n", key.name);
            goto out;
        }

        /* copy instruction */
        instruction_array[cur_ptr] = res->instruction;
        opt = res->nargs;
repeat:
        if (opt-- > 0 && (cur_ptr+1 < bytearray.len))
        {
            instruction_array[cur_ptr] = atoi(bytearray.buf_ptr[++cur_ptr]);
            goto repeat;
        }
    }

    /* 
     * if output in file
     * flag is set then 
     * open  new file
     * descriptor with
     * str pointing on 
     * it
     */
    if (output_flag)
    {
        int new_fd = open(str, O_WRONLY|O_CREAT|O_TRUNC, 0755);
        if (new_fd < 0)
            errMsg("open");
        write(new_fd, &instruction_array[0], sizeof(int)*bytearray.len);

        close(new_fd);
    }
    else
        write(1, &instruction_array[0], sizeof(int)*bytearray.len);

out:
    /* free allocated memory */
    
    xfree(&bytearray);
    free(instruction_array);
    return 0;
}
