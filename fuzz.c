#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define ARR_SIZE(x) sizeof(x)/sizeof(x[0])

/* byte array wrapper */
typedef struct {
    int *buffer;
    size_t len;
} list_t;

/* instruction and its number of argument */
int vm_instruction[][2] = {
    { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 0 },
    { 5, 0 }, { 6, 1 }, { 7, 1 }, { 8, 1 },
    { 9, 1 }, { 10,1 }, { 11,1 }, { 12,1 },
    { 13,1 }, { 14,0 }, { 15,0 }, { 16,3 },
    { 17,0 }, { 18,0 }
};

/*
 * open 2 pipes for communicating with
 * child main process and send fuzzing
 * input and wait for child to read
 * on the input
 */
void fuzz(list_t *bytearray, uint32_t testcase, int32_t coverage)
{
    int pipefd[2];
    char file_name[256];
    pid_t cpid;

    pipe(pipefd);

    /* child process will execute
     * main program and take input
     * from stdin via pipefd
     */
    if ((cpid = fork()) == 0)
    {
        char *argv[] = { "./main", NULL };
        int newfile = open("/dev/null", O_CREAT|O_WRONLY|O_TRUNC);
        alarm(2);

        close(pipefd[1]);
        dup2(pipefd[0], 0);
        dup2(newfile, 1);
        dup2(newfile, 2);
        execve("./main", argv, NULL);
    }
    else
    {
        /*
         * parent process will wait 
         * for child to kill itself.
         * prints the child process
         * exit code and if the child
         * is signalled to get killed
         * and child process is not
         * has 14 exit code than
         * only it store input case
         * into a file for further
         * analysis
         */
        int wstatus;

        for (uint32_t j = 0; j < bytearray->len; j++)
            write(pipefd[1], &bytearray->buffer[j], sizeof(int));

        close(pipefd[1]);
        close(pipefd[0]);

        waitpid(cpid, &wstatus, WUNTRACED | WCONTINUED);
        do
        {
            if (WIFEXITED(wstatus))
            {
                printf("exited, status=%d\n", WEXITSTATUS(wstatus));
            }
            else if (WIFSIGNALED(wstatus))
            {
                wstatus = WTERMSIG(wstatus);
                if (wstatus != 14)
                {
                    int new_fd;
                    sprintf(file_name, "testcase/test_%d", testcase);
                    new_fd = open(file_name, O_WRONLY|O_CREAT|O_TRUNC, 0777);
                    write(new_fd, bytearray->buffer, bytearray->len);
                    close(new_fd);
                    dprintf(coverage, "killed by signal: %d, testcase: %d\n", wstatus, testcase);
                }
                printf("killed by signal: %d\n", wstatus);
            }
            else if (WIFSTOPPED(wstatus))
            {
                printf("stopped by signal %d\n", WSTOPSIG(wstatus));
            }
            else if (WIFCONTINUED(wstatus))
            {
                printf("continued\n");
            }
        } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
    }
}

/*
 * creates random
 * input which make 
 * sense for the program
 * to execute 
 * and send it to
 * program if that
 * program exited 
 * with some error
 * then we do further
 * studies.
 */
void worker(list_t *bytearray) {
    size_t nread = (time(0) + clock()) % bytearray->len;

    srand(nread);

    if (nread < 10)
        nread += 10;

    for (int i = 0; i < nread; i++)
    {
        bytearray->buffer[i] = vm_instruction[rand() % ARR_SIZE(vm_instruction)][0];
        for (int j = 0; j < vm_instruction[bytearray->buffer[i]][j], ++i < nread; j++)
            bytearray->buffer[i] = rand() % 10;
    }
    bytearray->len = nread;
}

/*
 * runs number
 * of time for
 * executing our
 * testcases
 */
int main(int argc, char *argv[])
{
    list_t bytearray;
    int code[128];
    uint32_t samples;
    int file = open("samples/coverage", O_WRONLY|O_CREAT|O_TRUNC, 0777);

    for (samples = 0; samples < 128; samples++)
    {
        bytearray.buffer = &code[0];
        bytearray.len = ARR_SIZE(code);

        worker(&bytearray);
        fuzz(&bytearray, samples, file);
    }
    close(file);
    return 0;
}
