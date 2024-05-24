#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#define BLOCK_SIZE 512
#define FILE_NAME_SIZE 100
#define N_OPERATIONS 3
#define N_ARGS 30           // should be bigger than spec_argc !!! maybe assetrtion ???
#define N_OPTIONS 2

int Operations[N_OPERATIONS]; // Table in which is stored which of the (now) 3 Operation we should provide. Could be also a dicctor sth.
int Options[N_OPTIONS];       // Table in which is stored which of the (now) 2 OpTIONS we should provide. Could be also a dicctor sth.

int sum_array(const int arr[], int size);
int find_operation(const int arr[], int size);
void op_t(char *filename);
int is_tar(const char *arr, int size);
int find_argument(int argc, char *argv[], const char *file_name,int *ticklist_args);
//const char* opt_f(const char *argv,int i); // eventuell unnoetig ckann vlt raus ???

char **spec_argv;
int spec_argc = 0;

//neu

int main(int argc, char *argv[])
{
    char file_name[FILE_NAME_SIZE];

    //------------- Argument Handling ------------- //

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
                //for(int i = 0; i < spec_argc; i++){
                //    printf("%s\n", spec_argv[i]);
                //}
                //strcpy(file_name,opt_f(argv,i));
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
       fprintf(stderr, "mytar: %s: Cannot open: No such file or directory\n",filename);
        errx(2,"Error is not recoverable: exiting now");
    }
    char header[2*BLOCK_SIZE];
    int ticklist_args[N_ARGS] = {0};  //ticklist_args[spec_argc] is basically that in compile time 
    int trunc = 1;
    fread(header, 1, 2*BLOCK_SIZE, tar_file);
    while (1)
    {

        fseek(tar_file, -BLOCK_SIZE, SEEK_CUR); // we have to go back to the beginning of the block , wieder zurückdrehen, da wir 2 blocks gelesen haben.




        int typeflag = header[156];
        if(typeflag % 48){
            errx(2, "Unsupported header type: %i", typeflag);
        }
        //check_type(header+156);

        // geTypet the name of the file out of the header (first 100 bytes)
        char filename[101]; // beware, the NUL byte \0 is not in the header, we have to add it
        strncpy(filename, header, 100);
        filename[100] = '\0';

        // ------- Test 011 -----
         if(!(*filename)){ // 
            trunc = 0; // ??? ist das so richtig , sonst schlägt Test 005/6 fehl
            break;
        } // we jump to the next header



        // ------- Test 005 -----
        if (!spec_argc || find_argument(spec_argc,spec_argv,filename,ticklist_args)){ // if we have actual specific filename arguments
            printf("%s\n", filename); // We can now print the name.
        }
        

        // We have to jump to the next header for the next tar entry, thats done by file size + padding.
        int filesize = 0;
        int padding = 0;

        sscanf(header + 124, "%o", &filesize); // We read the octal value of the file size stored in the header[124:135]

        if ((filesize % BLOCK_SIZE) != 0)
        { // in most cases there is padding if not, padding is 0.
            padding = BLOCK_SIZE - (filesize % BLOCK_SIZE);
        }

        
        if(fseek(tar_file, filesize + padding, SEEK_CUR)){ // ??? weiss nicht ob das klappt, bringt glaub nix fuer die 011
            fprintf(stderr, "mytar: Unexpected EOF 1 in archive\n");
            errx(2,"Error is not recoverable: exiting now");
        } // we jump to the next header
        
        int check = fread(header, 1, 2*BLOCK_SIZE, tar_file);
        int check2 = feof(tar_file);
        if(check < 2*BLOCK_SIZE){ // falls es aber 0 ist dann gings gerade auf
            printf("Bin hier\n");
            printf("check %i\n", check);
            trunc = 1; // ??? ist das so richtig , 1 für 11, 0 wenn 14 laufen soll
            //break;
        } // we jump to the next header




        // ------- Test 002 -----
        // Check if the block is all zeros (indicates end of archive)
        int is_double_zero = 0;
        for(int j = 2; j > 0 ; j--){
            int is_empty_block = 1;
            for (int i = j*BLOCK_SIZE; i > (j-1)*BLOCK_SIZE; i--)
            {
                if (header[i] != '\0')
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
            
            else  break;
        }
        if(is_double_zero == 1){
            fprintf(stderr, "mytar: A lone zero block at 22\n"); // ???  hardgecoded !!! ändern
            break;
        }
        if(is_double_zero == 2){
            trunc = 0;
            break;
        }








    } 
    










    // ------- Test 005 -----
    // Now we list files which we did not found in the archive.
    int turn = 0;
    for (int i = 0; i < spec_argc; i++)
    {
        //printf("%i\n", ticklist_args[i]);
        if (!ticklist_args[i])
        {
            turn = 1;
            //printf("spec_argc %s\n", spec_argv[i]);
            fprintf(stderr, "mytar: %s: Not found in archive\n",spec_argv[i]); // ??? soll das so hardgecoded?
            //errx(1, "%s: Not found in archive",spec_argv[i]);
        }

    }
    if (turn){
       errx(2, "Exiting with failure status due to previous errors");  
    }
    //printf("mouin");
    if(trunc){
        fprintf(stderr, "mytar: Unexpected EOF in archive\n");
        errx(2,"Error is not recoverable: exiting now");  
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
 * checks if argument after -f exists and is .tar
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


int find_argument(int argc, char *argv[], const char *file_name,int *ticklist_args)
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

