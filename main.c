#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdint-gcc.h>
#include <stdlib.h>
#include "mimpi.h"
#include "examples/test.h"
#include "examples/mimpi_err.h"

// TODO czy duzo wiadomosci zakleszcza sprawdzanie zakleszczen
// lista odebranych wiadomosci jest za duza

int main(int argc, char **argv)
{
    MIMPI_Init(false);
    int const world_rank = MIMPI_World_rank();
    srand(time(NULL));


    char number = 0;
//    if (world_rank == 1)
//        number = 1;
//    ASSERT_MIMPI_OK(MIMPI_Bcast(&number, 1, 1));
//    assert(number == 1);
//
//    if (world_rank == 3)
//        number = 3;
//    ASSERT_MIMPI_OK(MIMPI_Bcast(&number, 1, 3));
//    assert(number == 3);



//    for (int i = 0; i < 16; i++) {
//        if (world_rank == i) {
//            number = i;
//        }
//        ASSERT_MIMPI_OK(MIMPI_Bcast(&number, 1, i));
//        assert(number == i);
//    }

//    printf("Number: %d\n", number);
//    fflush(stdout);


    for (int i = 0; i < 1000; i++) {
        int random_root = rand() % 16;

        if (world_rank == random_root) {
            number = i % 128;
        }
        ASSERT_MIMPI_OK(MIMPI_Bcast(&number, 1, random_root));
        assert(number == i % 128);
    }


    MIMPI_Finalize();
    return test_success();
}


// ./mimpirun 5 ./main 100000 3

// extended pipe closed dziala jak dodałem usuwanie delay po barierze

// big_message.sh valgrind czas
// obstruction.sh valgrind czas
// deadlock1.sh valgrind czas




// TODO count moze byc duzy (MAX_INT) wiec zadbac o to zeby nie było overflowa

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/pipe.html     - pipe zwraca zawsze dwa najmniejsze wolne deskryptory
// https://pubs.opengroup.org/onlinepubs/009604599/functions/pipe.html
// https://stackoverflow.com/questions/29852077/will-a-process-writing-to-a-pipe-block-if-the-pipe-is-full#comment47830197_29852077 - write do pełnego pipe'a jest blokujący, read z pustego też

/*
for i in {1..1000}
do
   echo $i
   ./mimpirun 16 ./main
done


./update_public_repo
./test_on_public_repo


chmod -R 700 *

VALGRIND=1 ./test
time VALGRIND=1 ./run_test 51 2 examples_build/big_message



valgrind --track-origins=yes --trace-children=yes --track-fds=yes --leak-check=full --show-leak-kinds=all ./mimpirun 16 ./main

 */




/*
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdint-gcc.h>
#include "mimpi.h"
#include "examples/test.h"
#include "examples/mimpi_err.h"

#define WRITE_VAR "CHANNELS_WRITE_DELAY"
#define NS_PER_1_MS 1 ## 000 ## 000

 // TODO sprawdzic liczbe czesci dla 497
 #define SIZE 1000
int main(int argc, char **argv)
{
    MIMPI_Init(false);
    int const world_rank = MIMPI_World_rank();
    const char *delay = getenv("DELAY");
    if (delay)
    {
        int res = setenv(WRITE_VAR, delay, true);
        assert(res == 0);
    }

    uint8_t* tab = malloc(SIZE);
    if (world_rank == 0) {
        memset(tab, 42, SIZE);
    }
    else {
        memset(tab, 0, SIZE);
    }

//    printf("przed bcastem\n");
    ASSERT_MIMPI_OK(MIMPI_Bcast(tab, SIZE, 0));

//    for (int i = 0; i < SIZE; i++) {
//        if (tab[i] != 42) {
//            printf("value: %d, index: %d, jestem %d\n", tab[i], i, world_rank);
//        }
//        assert(tab[i] == 42);
//    }


    fflush(stdout);
    int res = unsetenv(WRITE_VAR);
    assert(res == 0);


    printf("Number: %d\n", tab[0]);
    MIMPI_Finalize();
    free(tab);
    return test_success();
}

 */