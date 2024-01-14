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


#define steps 200000 // 60000 // 200000
#define tagSpace 12
#define spaceSize 12

unsigned char myRand1(unsigned long long state)
{
    state = state * 1664525 + 1013904223;
    return state >> 20;
}

unsigned char myRand2(unsigned long long state)
{
    state = state * 25214903917 + 11;
    return state >> 20;
}

unsigned char myRand3(unsigned long long state)
{
    state = state * 16843009 + 826366247;
    return state >> 20;
}

char *getData(int tag, int size)
{
    char *res = (char *)malloc(size * sizeof(char));
    assert(res != NULL);

    for (int i = 0; i < size; i++)
    {
        res[i] = (myRand1(i) ^ myRand2(tag) ^ myRand3(size));
    }
    return res;
}

void validateData(int tag, int size, char *data)
{
    for (int i = 0; i < size; i++)
    {
        fflush(stdout);
        assert(data[i] == (char)(myRand1(i) ^ myRand2(tag) ^ myRand3(size)));
    }
}

static unsigned long long st = 2137;
unsigned char myRand4()
{
    st = st * 214013 + 2531011;
    return st >> 20;
}

int main(int argc, char **argv)
{
    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();
    int const world_size = MIMPI_World_size();

    st ^= world_rank;

    for (int i = 0; i < steps; i++)
    {
        fflush(stdout);
        if (myRand4() % 4 || i < 20000)
        {
            int tar = myRand4() % (world_size - 1);
            tar += (tar >= world_rank) ? 1 : 0;

            int size = (myRand4() % spaceSize) + 1;
            int tag = (myRand4() % tagSpace) + 1;

            char *data = getData((tag ^ myRand2(world_rank)), size);
            ASSERT_MIMPI_OK(MIMPI_Send(data, size, tar, tag));

            free(data);
        }
        else
        {
            int rec = myRand4() % (world_size - 1);
            rec += (rec >= world_rank) ? 1 : 0;

            int size = (myRand4() % spaceSize) + 1;
            int tag = myRand4() % (tagSpace + 1);

            char *data = (char *)malloc(size * sizeof(char));
            assert(data != NULL);
            ASSERT_MIMPI_OK(MIMPI_Recv(data, size, rec, tag));

            if (tag != 0)
                validateData((tag ^ myRand2(rec)), size, data);
            free(data);
        }
    }
    MIMPI_Barrier();
    fflush(stdout);
    MIMPI_Finalize();
    return test_success();
}
// TODO na start 15,1s

// TODO lot_of_messages dziala ale w 5 minut
// extended pipe closed dziala jak dodałem usuwanie delay po barierze
// big_message.sh valgrind czas
// obstruction.sh valgrind czas
// deadlock1.sh valgrind czas








// TODO count moze byc duzy (MAX_INT) wiec zadbac o to zeby nie było overflowa
// TODO wiadomości mniejsze niz 512B
// TODO szukanie wiadomosci nie od poczatku

// Wszystkie wiadomości mają rozmiar 512B, jak wiadomość jest za mała, to jest dopełniana zerami

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
VALGRIND=1 ./run_test 51 2 examples_build/big_message





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