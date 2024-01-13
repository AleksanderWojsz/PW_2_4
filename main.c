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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "mimpi.h"


#define NS_PER_1_MS 1##000##000

// based on: https://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds
/* msleep(): Sleep for the requested number of milliseconds. */
static int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

int main(int argc, char **argv)
{
    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();

    int const tag = 17;

    char number = 42;
    if (world_rank == 0)
    {
        msleep(500);
    }
    else if (world_rank == 1)
    {
        ASSERT_MIMPI_RETCODE(MIMPI_Recv(&number, 1, 0, tag), MIMPI_ERROR_REMOTE_FINISHED);
        if (MIMPI_World_size() > 6)
            ASSERT_MIMPI_RETCODE(MIMPI_Send(&number, 1, 0, tag), MIMPI_ERROR_REMOTE_FINISHED);
        ASSERT_MIMPI_RETCODE(MIMPI_Recv(&number, 1, 0, tag), MIMPI_ERROR_REMOTE_FINISHED);
        assert(number == 42);
    }
    else if (world_rank == 4)
    {
        // heurystyczne założenie że chwrite zakończy się z błędem "Zamknięty odpbiorca".
        setenv("CHANNELS_WRITE_DELAY", "1000", true);
        ASSERT_MIMPI_RETCODE(MIMPI_Barrier(), MIMPI_ERROR_REMOTE_FINISHED);
        int res = unsetenv("CHANNELS_WRITE_DELAY");
        assert(res == 0);
    }

    MIMPI_Finalize();
    printf("Done %d\n", world_rank);
    return test_success();
}





// TODO lot_of_messages dziala ale w 5 minut
// extended pipe closed dziala jak dodałem usuwanie delay po barierze


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



// TODO count moze byc duzy (MAX_INT) wiec zadbac o to zeby nie było overflowa
// TODO wiadomości mniejsze niz 512B
// TODO szukanie wiadomosci nie od poczatku

// Wszystkie wiadomości mają rozmiar 512B, jak wiadomość jest za mała, to jest dopełniana zerami

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/pipe.html     - pipe zwraca zawsze dwa najmniejsze wolne deskryptory
// https://pubs.opengroup.org/onlinepubs/009604599/functions/pipe.html
// https://stackoverflow.com/questions/29852077/will-a-process-writing-to-a-pipe-block-if-the-pipe-is-full#comment47830197_29852077 - write do pełnego pipe'a jest blokujący, read z pustego też

/*

./update_public_repo
./test_on_public_repo

 ./mimpirun 2 valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./main

chmod -R 700 *


for i in {1..100}
do
   echo $i
   ./mimpirun 2 ./main
done



 */
