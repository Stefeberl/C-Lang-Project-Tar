#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
// gcc -Wall -Wextra -o /afs/ms/u/e/eberles/Projekt/mytar /afs/ms/u/e/eberles/Projekt/mytar.c && ./run-tests.sh $(cat phase-1.tests)

#define BLOCK_SIZE 512
#define FILE_NAME_SIZE 100
#define N_OPERATIONS 3
#define N_ARGS 30 // should be bigger than spec_argc !!! maybe assetrtion ???
#define N_OPTIONS 2

int Operations[N_OPERATIONS]; // Table in which is stored which of the (now) 3 Operation we should provide. Could be also a dicctor sth.
int Options[N_OPTIONS];       // Table in which is stored which of the (now) 2 OpTIONS we should provide. Could be also a dicctor sth.

int sum_array(const int arr[], int size);
int find_operation(const int arr[], int size);
void op_t(char *filename);
int is_tar(const char *arr, int size);
int find_argument(int argc, char *argv[], const char *file_name, int *ticklist_args);
void dump_hex(char *buffer, size_t size);

char **spec_argv;  // args array for specific filenames
int spec_argc = 0; // number of specific filenames

int main(int argc, char *argv[])
{
    char file_name[FILE_NAME_SIZE];

    //------------- Argument Handling ------------- //

    // ------ Test 001 ------
    if (argc < 2)
    {
        err(2, "need at least one option");
    }

    int turn = 0;
    for (int i = 0; i < argc; i++)
    {

        char *arg = argv[i];
        if (arg[0] == '-')
        {

            char flag = arg[1];
            switch (flag)
            {
            // Operations:
            case 't':
                Operations[0] = 1;
                break;
            case 'x':
                Operations[1] = 1;
                break;
            case 'c':
                Operations[2] = 1;
                break;
            // Options:
            case 'v':
                Options[0] = 1;
                break;
            case 'f':
                i++;
                spec_argv = argv + 4;
                spec_argc = argc - 4;
                is_tar(*(argv + i), strlen(*(argv + i))); // checks if argument after -f exists and is .tar
                strcpy(file_name, *(argv + i));           // same as strcpy(file_name, argv[i]));

                Options[1] = 1;
                break;
            default:
                errx(2, "Unknown option: %s", arg);
            }
            turn = 1;
        }
    }
    if (!turn)
    {
        errx(2, "You must specify one of the '-Acdtrux', '--delete' or '--test-label' options\nTry 'tar --help' or 'tar --usage' for more information.");
    }

    int op_sum = sum_array(Operations, N_OPERATIONS);
    if (op_sum == 0)
    { // checking if at least one of the Operations is taken
        errx(1, "Must specify one of -c, -r, -t, -u, -x");
    }
    if (op_sum > 1)
    {
        errx(1, "Can't specify more than one Operation."); // could done more explicit, is good for now ???
    }

    //--------------------- Functionality ---------------------

    int operation = find_operation(Operations, N_OPERATIONS);

    switch (operation)
    {
    case 0: // t
        op_t(file_name);
        //  case 1: //x
        //      exit(0);
        //  case 2: //c
        //      exit(0);
    }

    // ----Main return -----

    return 0;
}

//--- Operations ----

void op_t(char *filename)
{
    FILE *tar_file = fopen(filename, "r+");
    if (tar_file == NULL)
    {
        fprintf(stderr, "mytar: %s: Cannot open: No such file or directory\n", filename);
        errx(2, "Error is not recoverable: exiting now");
    }
    char header[2 * BLOCK_SIZE];     // Buffer for the header and some insides of the file, we take two blocks to search for end of tar
    int ticklist_args[N_ARGS] = {0}; // ticklist_args[spec_argc] is basically that in compile time
    int trunc = 1;                   // Bool for truncation

    int check = fread(header, 1, 2 * BLOCK_SIZE, tar_file); // Bool for EOF
    fseek(tar_file, -BLOCK_SIZE, SEEK_CUR);                 // we have to go back one block because we took 2*BLOCK_SIZE in header

    int count = 0;
    while (1)
    {
        // ------- Test 002 -----
        // Check if the block is all zeros (indicates end of archive)

        int is_double_zero = 0;
        for (int j = 0; j < 2; j++)
        {
            int is_empty_block = 1;
            for (int i = j * BLOCK_SIZE; i < (j + 1) * BLOCK_SIZE; i++)
            {
                if (header[i] != 0)
                {
                    is_empty_block = 0;
                    break;
                }
            }
            if (is_empty_block)
            {
                trunc = 0;
                is_double_zero++;
            }

            else
                break;
        }
        if (is_double_zero == 1)
        {
            fprintf(stderr, "mytar: A lone zero block at 22\n"); // ???  hardgecoded !!! ändern
            break;
        }
        if (is_double_zero == 2)
        {
            trunc = 0;
            break;
        }

        // ------- Test 007 -----
        int typeflag = header[156];
        if (typeflag % 48)
        {
            errx(2, "Unsupported header type: %i", typeflag);
        }

        // geTypet the name of the file out of the header (first 100 bytes)
        char filename[101]; // beware, the NUL byte \0 is not in the header, we have to add it
        strncpy(filename, header, 100);
        filename[100] = '\0';

        // ------- Test 005 -----
        if (!spec_argc || find_argument(spec_argc, spec_argv, filename, ticklist_args))
        {                             // if we have actual specific filename arguments
            printf("%s\n", filename); // We can now print the name.
            count++;
        }

        // We have to jump to the next header for the next tar entry, thats done by file size + padding.
        int filesize = 0;
        int padding = 0;

        sscanf(header + 124, "%o", &filesize); // We read the octal value of the file size stored in the header[124:135]

        if ((filesize % BLOCK_SIZE) != 0)
        {
            padding = BLOCK_SIZE - (filesize % BLOCK_SIZE); // in most cases there is padding if not, padding is 0.
        }

        fseek(tar_file, filesize + padding, SEEK_CUR);

        if (check < 2 * BLOCK_SIZE && check != 0)
        { // Detects truncation at Mac OS
            trunc = 1;
            break;
        }
        check = fread(header, 1, 2 * BLOCK_SIZE, tar_file);

        fseek(tar_file, -BLOCK_SIZE, SEEK_CUR); // we have to go back one block because we took 2*BLOCK_SIZE.
        if (check < 2 * BLOCK_SIZE && check != 0)
        {                                                            // Detects lonely zero block
            fprintf(stderr, "mytar: A lone zero block at %i\n", 22); // ???  Sorry for hardcoding the 22, I dont get what it means.
            trunc = 0;
            break;
        } // we jump to the next header
        if (check == 0)
        {
            trunc = 0;
            break;
        }
    }

    // ------- Test 005 -----
    // Now we list files which we did not found in the archive.
    int turn = 0;
    for (int i = 0; i < spec_argc; i++)
    {
        if (!ticklist_args[i])
        {
            turn = 1;
            fprintf(stderr, "mytar: %s: Not found in archive\n", spec_argv[i]);
        }
    }
    if (turn)
    {
        errx(2, "Exiting with failure status due to previous errors");
    }
    if (trunc)
    {
        fprintf(stderr, "mytar: Unexpected EOF in archive\n");
        errx(2, "Error is not recoverable: exiting now");
    }

    fclose(tar_file);
}

//--- Helper Funktions ----

/**
 * @brief Sums up all elements of an array
 *
 * @param arr  Array of integers
 * @param size Size of the array
 * @return int Sum of all elements
 */
int sum_array(const int *arr, int size)
{
    int sum = 0;
    for (int i = 0; i < size; ++i)
    {
        sum += arr[i];
    }
    return sum;
}

/**
 * @brief Finds the first operation that is not 0
 *
 * @param arr an array of integers in which exactly one should be 1. This marks the operation.
 * @param size size of the array
 * @return int returns the index of the first operation that is not 0
 */
int find_operation(const int *arr, int size)
{
    int op = 0;
    while (arr[op] == 0 && op < size)
    {
        op++;
    }
    return op;
}

/**
 * @brief Checks if the file is a .tar file
 *
 * @param arr Array of characters
 * @param size Size of the array
 * @return int 1 if the file is a .tar file, 0 otherwise
 */
int is_tar(const char *arr, int size)
{
    if (arr == NULL)
    {
        errx(1, "Option -f requires an argument\nUsage: List:    tar -tf <archive-filename>\n Extract: tar -xf <archive-filename>\n Create:  tar -cf <archive-filename> [filenames...]\n Help:    tar --help");
    }
    if (size < 4 || strcmp(arr + size - 4, ".tar") != 0)
        errx(1, "Error opening archive: Unrecognized archive format");

    return 1;
}

/*
 * @brief Finds the file name in the arguments
 *
 * @param argc Number of arguments
 * @param argv Array of arguments
 * @param file_name Name of the file to find
 * @param ticklist_args Array of arguments to tick off
 * @return int 1 if the file was found, 0 otherwise
 */
int find_argument(int argc, char *argv[], const char *file_name, int *ticklist_args)
{
    for (int i = 0; i < argc; ++i)
    {

        if (strcmp(argv[i], file_name) == 0)
        {
            ticklist_args[i] = 1;
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Dumps a buffer in hex format
 *
 * @param buffer
 * @param size  Size of the buffer
 */
void dump_hex(char *buffer, size_t size)
{
    printf("Blockanfang: \n");
    int is_zero_block = 1;
    for (size_t i = 0; i < size; i++)
    {

        if (i == size / 2)
        {
            printf("Blockmitte \n");
            if (is_zero_block)
            {
                printf("Block war leer\n");
            }
        }
        printf("%02x ", buffer[i]);
        if ((i + 1) % 16 == 0)
        {
            printf("\n");
        }
        if (buffer[i] != 0)
        {
            is_zero_block = 0;
        }
    }
    if (is_zero_block)
    {
        printf("Block war leer\n");
    }
}
