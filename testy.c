



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "mimpi.h"
// TODO barrier remote finished
int main(int argc, char **argv) {
    MIMPI_Init(false);
//    printf("before\n");
    fflush(stdout);

    int world_rank = MIMPI_World_rank();
    int world_size = MIMPI_World_size();

    // Synchronize all processes at the start
    assert(MIMPI_Barrier() == MIMPI_SUCCESS);

    // Let's create a variable delay for each process based on its rank
    sleep(world_rank);

    if (world_rank == world_size - 1) {
        // The last process (highest rank) exits early after some barriers
        for (int i = 0; i < 3; i++) {
            assert(MIMPI_Barrier() == MIMPI_SUCCESS);
        }
        // Exit before other processes hit more barriers
        MIMPI_Finalize();
//        printf("Process %d exiting early\n", world_rank);
        exit(0);
    } else {
        // All other processes go through more barriers
        for (int i = 0; i < 6; i++) {
            MIMPI_Retcode ret = MIMPI_Barrier();
            if (i < 3) {
                // Expect success in the first few barriers
                assert(ret == MIMPI_SUCCESS);
            } else {
                // Expect error after the last process has exited
                assert(ret == MIMPI_ERROR_REMOTE_FINISHED);
            }
        }
    }

    MIMPI_Finalize();
//    printf("after\n");
    return 0;
}






#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "mimpi.h"
#include "examples/mimpi_err.h"

// TODO broadcast remote finished
int main(int argc, char **argv) {
    MIMPI_Init(false);
    int world_rank = MIMPI_World_rank();
    int world_size = MIMPI_World_size();

//    printf("before\n");
    fflush(stdout);

    char data[1000000];
    if (world_rank == 0) {
        memset(data, 42, sizeof(data)); // Only root has the initial value
    }


    if (world_rank == world_size - 1) {
        // The last process (highest rank) exits early after some broadcasts
        for (int i = 0; i < 30; i++) {
            assert(MIMPI_Bcast(data, sizeof(data), 0) == MIMPI_SUCCESS); // Synchronize before exiting
        }
        // Exit before other processes hit more broadcasts
        MIMPI_Finalize();
//        printf("Process %d exiting early\n", world_rank);
        exit(0);
    } else {
        // All other processes go through more broadcasts
        for (int i = 0; i < 6000; i++) {
            MIMPI_Retcode ret = MIMPI_Bcast(data, sizeof(data), 0);
            if (i < 30) {
                // Expect success in the first few broadcasts
                assert(ret == MIMPI_SUCCESS);
                assert(data[0] == 42); // Check if the broadcast worked
                for (int k = 0; k < sizeof(data); k += 789) {
                    assert(data[789] == 42);
                }
            } else {
                // Expect error after the last process has exited
                assert(ret == MIMPI_ERROR_REMOTE_FINISHED);
            }
        }
    }

    MIMPI_Finalize();
//    printf("after\n");
    return 0;
}








#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "mimpi.h"
#include "examples/mimpi_err.h"
#define WRITE_VAR "CHANNELS_WRITE_DELAY"

// TODO reduce remote finished
int main(int argc, char **argv)
{
    size_t data_size = 1;
    if (argc > 1)
    {
        data_size = atoi(argv[1]);
    }

    MIMPI_Init(false);
    int const world_rank = MIMPI_World_rank();

    const char *delay = getenv("DELAY");
    if (delay)
    {
        int res = setenv(WRITE_VAR, delay, true);
        assert(res == 0);
    }

    uint8_t *data = malloc(data_size);
    assert(data);
    memset(data, 1, data_size);
    uint8_t *recv_data = malloc(data_size);



    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);
    assert((MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0)) == MIMPI_SUCCESS);

    if (world_rank == 0) {
        assert(data[0] == 1);
//        printf("Number: %d\n", recv_data[0]);
        for (int i = 1; i < data_size; ++i) {
            assert(recv_data[i] == recv_data[0]);
            fflush(stdout);
        }
    }


    if (world_rank == 0)
    {
        assert(MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0) == MIMPI_ERROR_REMOTE_FINISHED);
//        printf("Number: %d\n", recv_data[0]); // Tutaj teoretycznie mogą być śmieci, ale recv data sie nie zmieni
    }
    else
    {

        if (world_rank != 1) {
            assert((MIMPI_Reduce(data, NULL, data_size, MIMPI_SUM, 0)) == MIMPI_ERROR_REMOTE_FINISHED);
        }
    }

    assert(data[0] == 1);
    for (int i = 1; i < data_size; ++i)
        assert(data[i] == data[0]);
    free(data);
    free(recv_data);

    int res = unsetenv(WRITE_VAR);
    assert(res == 0);

    MIMPI_Finalize();

//    printf("Ok %d\n", world_rank);

    return 0;
}








#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "mimpi.h"
#include "examples/mimpi_err.h"

#define WRITE_VAR "CHANNELS_WRITE_DELAY"

#define LARGE_MESSAGE_SIZE 2147483647 // 2147483647

// TODO broadcast duza wiadomosc
int main(int argc, char **argv)
{
    MIMPI_Init(false);
    int const world_rank = MIMPI_World_rank();

    uint8_t* data = malloc(LARGE_MESSAGE_SIZE);
    memset(data, world_rank == 0 ? 42 : 7, LARGE_MESSAGE_SIZE);


    const char *delay = getenv("DELAY");
    if (delay) {
        int res = setenv(WRITE_VAR, delay, 1);
        assert(res == 0);
    }
    ASSERT_MIMPI_OK(MIMPI_Bcast(data, LARGE_MESSAGE_SIZE, 0));
    int res = unsetenv(WRITE_VAR);
    assert(res == 0);


    for (int i = 0; i < LARGE_MESSAGE_SIZE; i++) {
//        printf("Sample Data: %d\n", data[i]);
        assert(data[i] == 42);
    }
//    printf("Sample Data: %d\n", data[LARGE_MESSAGE_SIZE - 10]);
    fflush(stdout);

    MIMPI_Finalize();

    free(data);
    return 0;
}





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
// TODO reduce, czy duża wiadomość blokuje jak remote finished
int main(int argc, char **argv)
{
    size_t data_size = 65000;

    MIMPI_Init(false);
    int const world_rank = MIMPI_World_rank();
    const char *delay = getenv("DELAY");
    if (delay)
    {
        int res = setenv(WRITE_VAR, delay, true);
        assert(res == 0);
    }
    uint8_t *data = malloc(data_size);
    assert(data);
    memset(data, 1, data_size);

    uint8_t *recv_data = malloc(data_size);
    assert(recv_data);

    assert(MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0) == MIMPI_SUCCESS);

    if (world_rank == 0)
    {

        assert(MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0) == MIMPI_ERROR_REMOTE_FINISHED);


        for (int i = 1; i < data_size; ++i)
            test_assert(recv_data[i] == recv_data[0]);
        printf("Number: %d\n", recv_data[0]);
        fflush(stdout);
        free(recv_data);
    }
    else
    {
        if (world_rank != 1)
        {
            assert(MIMPI_Reduce(data, NULL, data_size, MIMPI_SUM, 0) == MIMPI_ERROR_REMOTE_FINISHED);
        }
    }




    test_assert(data[0] == 1);
    for (int i = 1; i < data_size; ++i)
        test_assert(data[i] == data[0]);
    free(data);
    int res = unsetenv(WRITE_VAR);
    assert(res == 0);
    free(recv_data);
    MIMPI_Finalize();
    return test_success();
}












#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mimpi.h"
#include "examples/mimpi_err.h"

// TODO nieblokujace duze wiadomosci
int main(int argc, char **argv)
{
    MIMPI_Init(false);

    char* data = malloc(2147483647);

    int const world_rank = MIMPI_World_rank();
    memset(data, 42, 2147483647);

    if (world_rank == 0) {
        MIMPI_Send(data, sizeof(data), 1, 999);
    }
    else if (world_rank == 1)
    {
        sleep(10); // mozna tez bez sleep (wtedy spradza czy wiadomosc przestanie sie wysylac)
    }


    free(data);
    MIMPI_Finalize();
    printf("Skonczylem, jestem %d\n", world_rank);

    return 0;
}





#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "mimpi.h"
#include "examples/test.h"
#include "examples/mimpi_err.h"
// TODO Nie bedzie zakleszczenia bo cel skonczyl
int main(int argc, char **argv)
{
    MIMPI_Init(true);
//    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();
    int partner_rank = (world_rank / 2 * 2) + 1 - world_rank % 2;

    char number = 'a';
    if (world_rank % 2 == 0) {
        ASSERT_MIMPI_OK(MIMPI_Recv(&number, 1, partner_rank, 1));
        assert(MIMPI_Recv(&number, 1, partner_rank, 1) == MIMPI_ERROR_REMOTE_FINISHED);
        assert(MIMPI_Recv(&number, 1, partner_rank, 1) == MIMPI_ERROR_REMOTE_FINISHED);
        assert(MIMPI_Recv(&number, 1, partner_rank, 1) == MIMPI_ERROR_REMOTE_FINISHED);
    }
    else if (world_rank % 2 == 1) {
        ASSERT_MIMPI_OK(MIMPI_Send(&number, 1, partner_rank, 1));
    }
    MIMPI_Finalize();

    return 0;
}




#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "mimpi.h"
#include "examples/test.h"
#include "examples/mimpi_err.h"
// TODO zakleszczenia z barierą
int main(int argc, char **argv)
{
    MIMPI_Init(true);
//    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();
    int partner_rank = (world_rank / 2 * 2) + 1 - world_rank % 2;

    char number = '2';
    if (world_rank % 2 == 0) {
        ASSERT_MIMPI_OK(MIMPI_Recv(&number, 1, partner_rank, 17));
    }
    else if (world_rank % 2 == 1) {
        ASSERT_MIMPI_OK(MIMPI_Send(&number, 1, partner_rank, 17));
    }

    MIMPI_Barrier();

    assert(MIMPI_Recv(&number, 1, partner_rank, 17) == MIMPI_ERROR_DEADLOCK_DETECTED);

    MIMPI_Finalize();

    return 0;
}



#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "mimpi.h"
#include "examples/test.h"
#include "examples/mimpi_err.h"

// TODO czy duzo wiadomosci zakleszcza sprawdzanie zakleszczen
// lista odebranych wiadomosci jest za duza

#define NUMBER 100000
int main(int argc, char **argv)
{
//    MIMPI_Init(false);
    MIMPI_Init(true);

    int const world_rank = MIMPI_World_rank();
    int partner_rank = (world_rank / 2 * 2) + 1 - world_rank % 2;


    char number = '2';

    for (int i = 0; i < NUMBER; i++) {
        if (world_rank % 2 == 0) {
            ASSERT_MIMPI_OK(MIMPI_Recv(&number, 1, partner_rank, 17));
        }
        else if (world_rank % 2 == 1) {
            ASSERT_MIMPI_OK(MIMPI_Send(&number, 1, partner_rank, 17));
        }
    }

    MIMPI_Finalize();

    return 0;
}



// TODO z forum

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



int main(int argc, char **argv)
{
    MIMPI_Init(true);
    int rank = MIMPI_World_rank();
    char buf = '1';
    if (rank == 0) {
        assert(MIMPI_Recv(&buf, sizeof(buf), 1, 2137) == MIMPI_ERROR_DEADLOCK_DETECTED); // zakleszczenie
        assert(MIMPI_Send(&buf, sizeof(buf), 1, 2137) == MIMPI_SUCCESS); // sukces czy zakleszczenie?
    }
    if (rank == 1) {
        assert(MIMPI_Recv(&buf, sizeof(buf), 0, 2137) == MIMPI_ERROR_DEADLOCK_DETECTED); // zakleszczenie
        assert(MIMPI_Recv(&buf, sizeof(buf), 0, 2137) == MIMPI_SUCCESS); // sukces czy zakleszczenie?
    }
    MIMPI_Finalize();

}


int main(int argc, char **argv)
{
    MIMPI_Init(true);
    int rank = MIMPI_World_rank();
    char buf = '1';
    if (rank == 0) {
        assert(MIMPI_Recv(&buf, sizeof(buf), 1, 2137) == MIMPI_ERROR_DEADLOCK_DETECTED); // zakleszczenie
    }
    if (rank == 1) {
        assert(MIMPI_Recv(&buf, sizeof(buf), 0, 2137) == MIMPI_ERROR_DEADLOCK_DETECTED); // zakleszczenie
    }
    assert(MIMPI_Barrier() == MIMPI_SUCCESS); // sukces czy zakleszczenie?
    MIMPI_Finalize();

    return test_success();
}


int main(int argc, char **argv)
{
    MIMPI_Init(true);
    int rank = MIMPI_World_rank();
    char buf = '1';
    if (rank == 0) {
        MIMPI_Retcode status = MIMPI_Recv(&buf, sizeof(buf), 1, 2137); // zakleszczenie
        if (status == MIMPI_ERROR_DEADLOCK_DETECTED) {
            MIMPI_Finalize(); // wychodzimy z bloku MPI bezpośrednio po zakleszczeniu
        }
        else {
            assert(false);
        }
    }
    if (rank == 1) {
        assert(MIMPI_Recv(&buf, sizeof(buf), 0, 2137) == MIMPI_ERROR_DEADLOCK_DETECTED); // zakleszczenie
        MIMPI_Finalize();
    }
    if (rank == 2) {
        assert(MIMPI_Recv(&buf, sizeof(buf), 0, 2137) == MIMPI_ERROR_REMOTE_FINISHED); // nadawca zakończył działanie czy zakleszczenie?
        MIMPI_Finalize();
    }

    return test_success();
}





// TODO max int reduce
#define WRITE_VAR "CHANNELS_WRITE_DELAY"
int main(int argc, char **argv)
{
    size_t data_size = 2147483647;


    MIMPI_Init(false);
    int const world_rank = MIMPI_World_rank();

    const char *delay = getenv("DELAY");
    if (delay)
    {
        int res = setenv(WRITE_VAR, delay, true);
        assert(res == 0);
    }

    uint8_t *data = malloc(data_size);
    assert(data);
    memset(data, 1, data_size);

    if (world_rank == 0)
    {
        printf("Data size: %ld\n", data_size);
        uint8_t *recv_data = malloc(data_size);
        assert(recv_data);
        ASSERT_MIMPI_OK(MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0));
        for (int i = 1; i < data_size; ++i)
            test_assert(recv_data[i] == recv_data[0]);
        printf("Number: %d\n", recv_data[0]);
        fflush(stdout);
        free(recv_data);
    }
    else
    {
        ASSERT_MIMPI_OK(MIMPI_Reduce(data, NULL, data_size, MIMPI_SUM, 0));
    }
    test_assert(data[0] == 1);
    for (int i = 1; i < data_size; ++i)
        test_assert(data[i] == data[0]);
    free(data);

    int res = unsetenv(WRITE_VAR);
    assert(res == 0);

    MIMPI_Finalize();
    return test_success();
}

