#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define POM_PIPES 90
#define OUT_OF_MPI_BLOCK (-1)
#define META_INFO_SIZE 4
#define MAX_N 16

typedef struct Message {
    void *data;
    int count;
    int source;
    int tag;
    struct Message *next;
} Message;

// Dodawanie nowego elementu na koniec listy
void list_add(Message **head, void const *data, int count, int source, int tag) {
    Message *new_message = malloc(sizeof(Message));
    assert(new_message);

    if (data != NULL) {
        new_message->data = malloc(count);
        assert(new_message->data);
        memcpy(new_message->data, data, count);
    } else {
        new_message->data = NULL;
    }

    new_message->count = count;
    new_message->source = source;
    new_message->tag = tag;
    new_message->next = NULL;

    if (*head == NULL) {
        *head = new_message;
    } else {
        Message *current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_message;
    }
}


typedef struct To_delete {
    Message* message;
    struct To_delete *next;
} To_delete;

To_delete *to_delete_head = NULL;

void list_add_to_delete(To_delete **head, Message* message) {
    To_delete *new_message = malloc(sizeof(To_delete));
    assert(new_message);

    new_message->message = message;
    new_message->next = NULL;

    if (*head == NULL) {
        *head = new_message;
    } else {
        To_delete *current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_message;
    }
}

void list_clear_to_delete(To_delete **head) {
    To_delete *current = *head;
    To_delete *next;
    while (current != NULL) {
        next = current->next;
        free(current->message->data);
        free(current->message);
        free(current);
        current = next;
    }
    *head = NULL;
}


// Znajdowanie pierwszego elementu o wskazanym count, source, tag (lub dowolny tak jak nasz tag == 0)
Message *list_find(Message *head, int count, int source, int tag) {
    Message *current = head;
    while (current != NULL) {
        if ((current->count == count && current->source == source && (current->tag == tag || tag == 0)) ||
            (current->source == source && current->tag < 0)) { // Wiadomość specjalna od konkretnego nadawcy
            return current;
        }
        current = current->next;
    }
    return NULL; // Nie znaleziono elementu
}

// Usuwanie pierwszego elementu o wskazanym count, source, tag
void list_remove(Message **head, int count, int source, int tag) {
    Message *current = *head;
    Message *previous = NULL;
    while (current != NULL) {
        if (current->count == count && current->source == source && current->tag == tag) {
            if (previous == NULL) {
                // Usuwamy pierwszy element
                *head = current->next;
            } else {
                // Usuwamy element w środku lub na końcu
                previous->next = current->next;
            }
            list_add_to_delete(&to_delete_head, current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

void list_clear(Message **head) {
    Message *current = *head;
    Message *next;
    while (current != NULL) {
        next = current->next;
        free(current->data);
        free(current);
        current = next;
    }
    *head = NULL;
}

Message* received_list = NULL;
bool finished[16] = {false};
bool someone_already_finished = false;
Message* sent_messages = NULL;
bool deadlock_detection = false;
int no_of_barrier = 0;

void pipe_write(int fd, const void *buf, size_t n) {
    int written = 0;
    while (written < n) {
        int bytes_written = chsend(fd, buf + written, n - written);
        ASSERT_SYS_OK(bytes_written);
        written += bytes_written;
    }
}

void pipe_write_without_assert(int fd, const void *buf, size_t n) {
    int written = 0;
    while (written < n) {
        int bytes_written = chsend(fd, buf + written, n - written);
        written += bytes_written;
    }
}

void pipe_read(int fd, void *buf, size_t n) {
    int read = 0;
    while (read < n) {
        int bytes_read = chrecv(fd, buf + read, n - read);
        ASSERT_SYS_OK(bytes_read);
        read += bytes_read;
    }
}

int world_size = -10;
int MIMPI_World_size() {
    if (world_size == -10) {
        world_size = atoi(getenv("MIMPI_WORLD_SIZE")); // Konwertuje string na int
    }
    return world_size;
}

int world_rank = -10;
int MIMPI_World_rank() {
    if (world_rank == -10) {
        world_rank = atoi(getenv("MIMPI_RANK")); // getenv zwraca string
    }
    return world_rank;
}

int check_arguments_correctness(int other_process_rank) {
    if (world_rank == other_process_rank) {
        return MIMPI_ERROR_ATTEMPTED_SELF_OP;
    }
    else if (other_process_rank >= world_size || other_process_rank < 0) {
        return MIMPI_ERROR_NO_SUCH_RANK;
    }
    else {
        return MIMPI_SUCCESS;
    }
}

void notify_iam_out() {
    int is_active[MAX_N];
    pipe_read(60, is_active, sizeof(int) * world_size);
    is_active[world_rank] = 0;
    pipe_write(61, is_active, sizeof(int) * world_size);
}

bool is_in_MPI_block(int nr) {

    if (nr >= world_size) {
        return false;
    }

    int is_active[MAX_N];
    pipe_read(60, is_active, sizeof(int) * world_size);
    bool result = (is_active[nr] == 1);
    pipe_write(61, is_active, sizeof(int) * world_size);
    return result;
}

int MIMPI_get_read_desc(int src, int dest) {
    return src * world_size * 2 + 20 + 2 * POM_PIPES + dest * 2;
}

int MIMPI_get_write_desc(int src, int dest) {
    return MIMPI_get_read_desc(src, dest) + 1;
}

int get_barrier_read_desc(int rank) {
    return 20 + rank * 2;
}

int get_barrier_write_desc(int rank) {
    return get_barrier_read_desc(rank) + 1;
}

// mutexy
int get_deadlock_read_desc(int rank) {
    return 70 + rank * 2;
}

int get_deadlock_write_desc(int rank) {
    return get_deadlock_read_desc(rank) + 1;
}

int get_deadlock_received_read_desc(int rank) {
    return 120 + rank * 2;
}

int get_deadlock_received_write_desc(int rank) {
    return get_deadlock_received_read_desc(rank) + 1;
}

int get_deadlock_counter_read_desc(int rank) {
    return 160 + rank * 2;
}

int get_deadlock_counter_write_desc(int rank) {
    return get_deadlock_counter_read_desc(rank) + 1;
}

void notify_that_message_received(int who_sent_it_to_us, int count, int tag) { // Do deadlock
    int message[3]; // dane odebranej wiadomości
    message[0] = world_rank;
    message[1] = count;
    message[2] = tag;

    int no_of_messages;
    pipe_read(get_deadlock_counter_read_desc(who_sent_it_to_us), &no_of_messages, sizeof(int)); // Też jako mutex
    no_of_messages++;
    pipe_write(get_deadlock_received_write_desc(who_sent_it_to_us), message, sizeof(int) * 3);
    pipe_write(get_deadlock_counter_write_desc(who_sent_it_to_us), &no_of_messages, sizeof(int));
}

Message* odczytane_wiadomosci_deadlock = NULL;
bool finished_deadlock[16] = {false};
bool rodzic_czeka_na_odczytanie_wszystkich = false;
pthread_mutex_t mutex_pipes;
pthread_mutex_t parent_sleeping;
pthread_mutex_t unread_messages_sleeping;

// 111 C
// 113 A
// 115 source
// 117 count
// 119 tag

bool check_for_deadlock_in_receive(int count_arg, int source_arg, int tag_arg) {

    // dodajemy nasze zgłoszenie
    int c[MAX_N];
    int a[MAX_N];
    int source[MAX_N];
    int count[MAX_N];
    int tag[MAX_N];

    pipe_read(110, c, sizeof(int) * world_size);
    pipe_read(112, a, sizeof(int) * world_size);
    pipe_read(114, source, sizeof(int) * world_size);
    pipe_read(116, count, sizeof(int) * world_size);
    pipe_read(118, tag, sizeof(int) * world_size);

    c[world_rank] = -1;
    a[world_rank] = -1;
    source[world_rank] = source_arg;
    count[world_rank] = count_arg;
    tag[world_rank] = tag_arg;

    while (true) {

        // usuwamy z listy wysłanych wiadomości te, które już ktoś odebrał
        int no_of_messages_to_remove;
        pipe_read(get_deadlock_counter_read_desc(world_rank), &no_of_messages_to_remove, sizeof(int));
        pipe_write(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int));
        if (no_of_messages_to_remove > 0) {
            rodzic_czeka_na_odczytanie_wszystkich = true;

            ASSERT_ZERO(pthread_mutex_lock(&unread_messages_sleeping)); // jak są jeszcze jakieś nieodebrane wiadomości to czekamy
        }

        pipe_read(get_deadlock_counter_read_desc(world_rank), &no_of_messages_to_remove, sizeof(int)); // Jako mutex
        // Usuwamy odczytane wiadomości z listy wysłanych
        Message* to_delete = odczytane_wiadomosci_deadlock;
        while (to_delete != NULL) {
            list_remove(&sent_messages, to_delete->count, to_delete->source, to_delete->tag);
            to_delete = to_delete->next;
        }
        list_clear(&odczytane_wiadomosci_deadlock);
        pipe_write(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int));


        // Przetwarzamy zgłoszenia
        for(int i = 0; i < world_size; i++) {
            if (source[i] == world_rank) { // jak ktoś na nas czeka
                Message* message = list_find(sent_messages, count[i], i, tag[i]); // sprawdzamy, czy mamy dla niego wiadomosc

                if (message != NULL) { // mamy dla kogoś wiadomość, więc nic nie robimy
                }
                else { // nie ma wiadomości więc potwierdzamy
                    a[i] = 1; // approved

                    // budzimy
                    void* foo_message = malloc(512);
                    assert(foo_message);
                    memset(foo_message, 0, 512);
                    pipe_write(get_deadlock_write_desc(i), foo_message, 512);
                    free(foo_message);
                }
            }
        }

        // jak my na pewno czekamy, nie byliśmy już wcześniej w cyklu, i ktos jest z nami w cyklu
        if (a[world_rank] == 1 && c[world_rank] != 1 && source[source_arg] == world_rank && a[source_arg] == 1 && c[source_arg] != 1) {
            c[world_rank] = 1; // jestesmy w cyklu
            c[source_arg] = 1; // oznaczamy, że ktoś jest z nami w cyklu
        }

        if (c[world_rank] == 1) { // jak jesteśmy w cyklu, to usuwamy nasz wpis i wychodzimy
            c[world_rank] = -1;
            a[world_rank] = -1;
            source[world_rank] = -1;
            count[world_rank] = -1;
            tag[world_rank] = -1;
            pipe_write(111, c, sizeof(int) * world_size);
            pipe_write(113, a, sizeof(int) * world_size);
            pipe_write(115, source, sizeof(int) * world_size);
            pipe_write(117, count, sizeof(int) * world_size);
            pipe_write(119, tag, sizeof(int) * world_size);

            return true;
        }

        // Oddajemy tablice
        pipe_write(111, c, sizeof(int) * world_size);
        pipe_write(113, a, sizeof(int) * world_size);
        pipe_write(115, source, sizeof(int) * world_size);
        pipe_write(117, count, sizeof(int) * world_size);
        pipe_write(119, tag, sizeof(int) * world_size);


        // śpimy
        while (true) {
            int fragment_tag;
            int who_finished;
            void* foo_message = malloc(512);
            assert(foo_message);
            pipe_read(get_deadlock_read_desc(world_rank), foo_message, 512);
            memcpy(&fragment_tag, foo_message + sizeof(int) * 2, sizeof(int));
            memcpy(&who_finished, foo_message + sizeof(int) * 3, sizeof(int));

            if (fragment_tag < 0) {
                if (fragment_tag == -1) {
                    finished_deadlock[who_finished] = true;
                    if (who_finished == source_arg) { // odczytaliśmy wiadomość, że ten na kogo czekamy skończył więc nie będzie z nim deadlocka
                        free(foo_message);
                        return false;
                    }
                    else if (who_finished != source_arg) { // ktos się skonczyl, ale nie czekamy na niego w tym receive
                        // idziemy spac znowu
                    }
                }
                else if (fragment_tag == -2) { // wątek nas obudził, bo mamy wiadomość
                    // usuwamy nasz wpis i wychodzimy

                    pipe_read(110, c, sizeof(int) * world_size);
                    pipe_read(112, a, sizeof(int) * world_size);
                    pipe_read(114, source, sizeof(int) * world_size);
                    pipe_read(116, count, sizeof(int) * world_size);
                    pipe_read(118, tag, sizeof(int) * world_size);

                    c[world_rank] = -1;
                    a[world_rank] = -1;
                    source[world_rank] = -1;
                    count[world_rank] = -1;
                    tag[world_rank] = -1;

                    pipe_write(111, c, sizeof(int) * world_size);
                    pipe_write(113, a, sizeof(int) * world_size);
                    pipe_write(115, source, sizeof(int) * world_size);
                    pipe_write(117, count, sizeof(int) * world_size);
                    pipe_write(119, tag, sizeof(int) * world_size);

                    free(foo_message);

                    return false;
                }
            }
            else{ // (fragment_tag >= 0) Ktos nas obudził, bo przetworzył naszą wiadomość
                free(foo_message);
                break;
            }

            free(foo_message);
        }

        pipe_read(110, c, sizeof(int) * world_size);
        pipe_read(112, a, sizeof(int) * world_size);
        pipe_read(114, source, sizeof(int) * world_size);
        pipe_read(116, count, sizeof(int) * world_size);
        pipe_read(118, tag, sizeof(int) * world_size);
    }
}

void read_message_from_pom_pipe(int descriptor, void** result_buffer, int* count, int* tag) {

    free(*result_buffer);
    long long int result_buffer_size = 496;
    *result_buffer = malloc(result_buffer_size);
    assert(result_buffer);

    void* buffer = malloc(512);  // Bufor do przechowywania fragmentów i metadanych
    assert(buffer);
    int received = 0;  // Ile bajtów danych już otrzymaliśmy
    int ile = 0, z_ilu = 1, fragment_tag, current_message_size;

    while (ile < z_ilu) {

        // Odbieranie fragmentu
        pipe_read(descriptor, buffer, 512);

        // Rozpakowywanie metadanych z bufora
        memcpy(&ile, buffer + sizeof(int) * 0, sizeof(int));
        memcpy(&z_ilu, buffer + sizeof(int) * 1, sizeof(int));
        memcpy(&fragment_tag, buffer + sizeof(int) * 2, sizeof(int));
        memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));


        // Zapisywanie danych z fragmentu do głównego bufora danych

        memcpy(*result_buffer + received, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
        received += current_message_size;

        if (ile < z_ilu) { // Jak to nie koniec czytania to powiększamy bufor
            result_buffer_size += 496;
            *result_buffer = realloc(*result_buffer, result_buffer_size);
        }
    }

    *count = received;
    *tag = fragment_tag;

    free(buffer);
}



int waiting_count = -10;
int waiting_source = -10;
int waiting_tag = -10;

void *read_messages_from_source(void *src) {

    int source = *((int *)src);

    // Czytamy wiadomosci od konkretnego procesu i zapisujemy je na liscie
    while (true) {

        void* read_message = malloc(496);
        assert(read_message);
        int fragment_tag = 0;
        int count = 0;
        read_message_from_pom_pipe(MIMPI_get_read_desc(source, world_rank), &read_message, &count, &fragment_tag);



        // W tym miejscu mamy już całą jedną wiadomość

        ASSERT_ZERO(pthread_mutex_lock(&mutex_pipes));
        list_add(&received_list, read_message, count, source, fragment_tag);

        // Zapisujemy wszystkie wiadomości, a jak rodzic czeka na daną wiadomość
        // lub wiadomość to dowolna specjalna wiadomość od tego nadawcy to budzimy rodzica
        if ((waiting_count == count && waiting_source == source && (waiting_tag == fragment_tag || waiting_tag == 0)) ||
            (waiting_source == source && fragment_tag < 0)) {
            waiting_count = -10;
            waiting_source = -10;
            waiting_tag = -10;

            if (deadlock_detection == true && fragment_tag >= 0) { // zakładamy że z wiadomościami specjalnymi nie ma zakleszczeń
                void *message = malloc(512);
                assert(message);
                memset(message, 0, 512);
                int tag = -2;
                memcpy(message + sizeof(int) * 2, &tag, sizeof(int));
                pipe_write(get_deadlock_write_desc(world_rank), message, 512);
                free(message);
            }

            ASSERT_ZERO(pthread_mutex_unlock(&parent_sleeping));
        }
        ASSERT_ZERO(pthread_mutex_unlock(&mutex_pipes));

        free(read_message);

        if (fragment_tag == -1) { // Jak nie będzie już więcej wiadomości to kończymy
            break;
        }
    }

    free(src);

    return NULL;
}

void *czytaj_odczytane_wiadomosci_deadlock() {

    // Czytamy wiadomości które ktoś już odebrał i zapisujemy je na listę
    while (true) {
        int message[3];
        pipe_read(get_deadlock_received_read_desc(world_rank), message, sizeof(int) * 3);

        int no_of_messages_to_remove;
        pipe_read(get_deadlock_counter_read_desc(world_rank), &no_of_messages_to_remove, sizeof(int)); // Też jako mutex
        if (message[2] == -1) {
            pipe_write(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int)); // Oddajemy mutexa
            return NULL;
        }
        list_add(&odczytane_wiadomosci_deadlock, NULL, message[1], message[0], message[2]);
        for (int i = 0; i < no_of_messages_to_remove - 1; i++) { // -1 bo juz pierwszą wiadomość odczytaliśmy
            list_add(&odczytane_wiadomosci_deadlock, NULL, message[1], message[0], message[2]);
            pipe_read(get_deadlock_received_read_desc(world_rank), message, sizeof(int) * 3);
            if (message[2] == -1) {
                pipe_write(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int)); // Oddajemy mutexa
                return NULL;
            }
        }
        no_of_messages_to_remove = 0;

        if (rodzic_czeka_na_odczytanie_wszystkich == true) {
            rodzic_czeka_na_odczytanie_wszystkich = false;
            ASSERT_ZERO(pthread_mutex_unlock(&unread_messages_sleeping));
        }
        pipe_write(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int));
    }
}

pthread_t threads[16];
pthread_t czytajacy_odczytane_wiadomosci_deadlock;

void MIMPI_Init(bool enable_deadlock_detection) {
    channels_init();

    if (enable_deadlock_detection) {
        deadlock_detection = true;
    }

    MIMPI_World_rank();
    MIMPI_World_size();

    ASSERT_ZERO(pthread_mutex_init(&mutex_pipes, NULL));
    ASSERT_ZERO(pthread_mutex_init(&parent_sleeping, NULL));
    ASSERT_ZERO(pthread_mutex_lock(&parent_sleeping)); // zmniejszanie licznika do 0
    ASSERT_ZERO(pthread_mutex_init(&unread_messages_sleeping, NULL));
    ASSERT_ZERO(pthread_mutex_lock(&unread_messages_sleeping)); // zmniejszanie licznika do 0

    // Tworzenie po jednym wątku na każdy pipe
    for (int i = 0; i < world_size; i++) {
        if (i != world_rank) {
            int *source = malloc(sizeof(int));
            assert(source);
            *source = i;
            pthread_create(&threads[i], NULL, read_messages_from_source, source);
        }
    }

    // wątek czytający odczytane wiadomosci
    if (deadlock_detection == true) {
        pthread_create(&czytajacy_odczytane_wiadomosci_deadlock, NULL, czytaj_odczytane_wiadomosci_deadlock, NULL);
    }

}

int send_message_to_pipe(int descriptor, void const *data, int count, int tag, bool check_if_dest_active, int destination) {
    // Dzielimy wiadomość na części rozmiaru ile + z_ilu + tag + current_message_size + wiadomość = 512B
    int max_message_size = 512 - sizeof(int) * META_INFO_SIZE;
    int liczba_czesci = (int)(((long long int)count + ((long long int)max_message_size - 1)) / (long long int)max_message_size); // ceil(A/B) use (A + (B-1)) / B

    int nr_czesci = 1;

    // tworzenie wiadomosci
    for (int i = 0; i < liczba_czesci; i++) {

        // Na wypadek, gdyby odbiorca się zakończył przy wysłaniu długiej wiadomości
        if (check_if_dest_active && is_in_MPI_block(destination) == false) {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }

        int current_message_size = max_message_size;
        if (i == liczba_czesci - 1) { // ostatnia część wiadomości
            current_message_size = count - (liczba_czesci - 1) * max_message_size;
        }

        void *message = malloc(512);
        assert(message);
        memset(message, 0, 512);

        memcpy(message + sizeof(int) * 0, &nr_czesci, sizeof(int));
        memcpy(message + sizeof(int) * 1, &liczba_czesci, sizeof(int));
        memcpy(message + sizeof(int) * 2, &tag, sizeof(int));
        memcpy(message + sizeof(int) * 3, &current_message_size, sizeof(int));
        memcpy(message + sizeof(int) * 4, data + i * max_message_size, current_message_size);

        // Wysyłanie wiadomości
        pipe_write(descriptor, message, 512);
        nr_czesci++;
        free(message);
    }

    return MIMPI_SUCCESS;
}

void* first_message_from_other_barrier = NULL;
int source_1 = -1;
void* second_message_from_other_barrier = NULL;
int source_2 = -1;

void MIMPI_Finalize() {

    notify_iam_out(); // Zabraniamy wysłać do nas wiadomości poprzez MIMPI_SEND

    // wysyłamy wszystkim innym wiadomość z tagiem OUT_OF_MPI_BLOCK (-1) oznaczającą, że skończyliśmy
    for (int i = 0; i < world_size; i++) {
        if (i != world_rank && is_in_MPI_block(i) == true) {
            int message = 0;
            MIMPI_Send(&message, 1, i, OUT_OF_MPI_BLOCK);
        }
    }

    if (deadlock_detection) {
        for (int i = 0; i < world_size; i++) { // Wysyłamy do pipe'ów od deadlock, że skonczyliśmy i nie będzie z nami deadlocka już nigdy
            if (i != world_rank) {
                void *message = malloc(512);
                assert(message);
                int tag = -1;
                memset(message, 0, 512);
                memcpy(message + sizeof(int) * 2, &tag, sizeof(int)); // ze to wiadomość specjalna
                memcpy(message + sizeof(int) * 3, &world_rank, sizeof(int)); // i to kim jestesmy
                pipe_write(get_deadlock_write_desc(i), message, 512);
                free(message);
            }
        }

        int message[3];
        message[0] = 10;
        message[1] = 10;
        message[2] = -1;
        pipe_write(get_deadlock_received_write_desc(world_rank), message, sizeof(int) * 3);
        pthread_join(czytajacy_odczytane_wiadomosci_deadlock, NULL);
    }

    // Wysyłamy wiadomości z tagiem -1 do samych siebie przez wszystkie pipe'y, żeby nasze wątki wiedziały, że mają się zakończyć
    for (int i = 0; i < world_size; i++) {
        if (i != world_rank) {
            void *message = malloc(512);
            assert(message);
            memset(message, 0, 512);
            send_message_to_pipe(MIMPI_get_write_desc(i, world_rank), message, 100, -1, false, i);
            free(message);
        }
    }

    // Czekanie na zakończenie wszystkich wątków
    for (int i = 0; i < world_size; i++) {
        if (i != world_rank) {
            pthread_join(threads[i], NULL);
        }
    }

    list_clear(&received_list);
    list_clear(&sent_messages);
    list_clear(&odczytane_wiadomosci_deadlock);
    list_clear_to_delete(&to_delete_head);

    pthread_mutex_destroy(&mutex_pipes);
    pthread_mutex_destroy(&parent_sleeping);
    pthread_mutex_destroy(&unread_messages_sleeping);

    free(first_message_from_other_barrier);
    free(second_message_from_other_barrier);

    for (int i = 0; i < world_size; i++) {
        for (int j = 0; j < world_size; j++) {
            ASSERT_SYS_OK(close(MIMPI_get_read_desc(i, j)));
            ASSERT_SYS_OK(close(MIMPI_get_write_desc(i, j)));
        }
        ASSERT_SYS_OK(close(get_barrier_read_desc(i)));
    }

    for (int i = get_barrier_read_desc(world_size); i < 60; i++) {
        ASSERT_SYS_OK(close(i));
    }

    for (int i = 0; i < world_size; i++) { // Wysyłamy do pipe'ów od babiery, wiadomość z tagiem -1, że nas nie będzie
        if (i != world_rank) {

            void *message = malloc(512);
            assert(message);
            memset(message, 0, 512);
            int tag = -no_of_barrier - 1;
            memcpy(message + sizeof(int) * 2, &tag, sizeof(int));

            pipe_write_without_assert(get_barrier_write_desc(i), message, 512); // Może zwrócić EPIPE
            free(message);
        }
    }


    for (int i = 0; i < world_size; i++) {
        ASSERT_SYS_OK(close(get_barrier_write_desc(i)));
    }

    for (int i = 60; i < 20 + 2 * POM_PIPES; i++) {
        ASSERT_SYS_OK(close(i));
    }

    channels_finalize();
}

MIMPI_Retcode MIMPI_Send(
        void const *data,
        int count,
        int destination,
        int tag
) {

    if (check_arguments_correctness(destination) != MIMPI_SUCCESS) {
        return check_arguments_correctness(destination);
    }
    else if (is_in_MPI_block(destination) == false) {
        someone_already_finished = true;
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    if (deadlock_detection && tag >= 0) { // Do listy dodajemy tylko wiadomości z tagiem >= 0, bo nikt nigdy nie będzię czekał na wiadomość specjalną
        list_add(&sent_messages, data, count, destination, tag);
    }

    return send_message_to_pipe(MIMPI_get_write_desc(world_rank, destination), data, count, tag, true, destination);
}

MIMPI_Retcode MIMPI_Recv(
        void* data,
        int count,
        int source,
        int tag
) {

    if (check_arguments_correctness(source) != MIMPI_SUCCESS) {
        return check_arguments_correctness(source);
    }

    // Potrzebna jakby ktoś kilka razy zrobił recv od zakończonego nadawcy
    ASSERT_ZERO(pthread_mutex_lock(&mutex_pipes));
    if (finished[source] == true) {
        ASSERT_ZERO(pthread_mutex_unlock(&mutex_pipes));
        someone_already_finished = true;
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    // Sprawdzamy, czy taka wiadomość była odebrana już wcześniej
    // list_find znajdzie też wiadomości z tagiem 0 lub dowolną specjalną od tego nadawcy
    Message *message = list_find(received_list, count, source, tag);
    if (message != NULL) {

        if (message->tag < 0) { // Wiadomość specjalna
            if (message->tag == -1) {
                finished[source] = true;
                list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
                ASSERT_ZERO(pthread_mutex_unlock(&mutex_pipes));
                someone_already_finished = true;
                return MIMPI_ERROR_REMOTE_FINISHED;
            }
        }

        memcpy(data, message->data, message->count); // Przepisanie danych do bufora użytkownika
        list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
        ASSERT_ZERO(pthread_mutex_unlock(&mutex_pipes));

        if (deadlock_detection && is_in_MPI_block(source) == true && tag >= 0) {
            notify_that_message_received(source, count, tag);
        }

        return MIMPI_SUCCESS;
    }

    // Nie znaleźliśmy wiadomości, więc czekamy na nią
    waiting_count = count;
    waiting_source = source;
    waiting_tag = tag;
    ASSERT_ZERO(pthread_mutex_unlock(&mutex_pipes));

    if (deadlock_detection && tag >= 0 && finished_deadlock[source] == false) {
        if (check_for_deadlock_in_receive(count, source, tag)) {
            return MIMPI_ERROR_DEADLOCK_DETECTED;
        }
    }

    ASSERT_ZERO(pthread_mutex_lock(&parent_sleeping)); // idziemy spać

    // W tym miejscu na liście jest już nasza wiadomość, tylko musimy ją znaleźć
    ASSERT_ZERO(pthread_mutex_lock(&mutex_pipes));

    message = list_find(received_list, count, source, tag);
    if (message->tag < 0) { // Wiadomość specjalna
        if (message->tag == -1) {
            finished[source] = true;
            list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
            ASSERT_ZERO(pthread_mutex_unlock(&mutex_pipes));
            someone_already_finished = true;
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }
    memcpy(data, message->data, message->count); // Przepisanie danych do bufora użytkownika
    list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
    ASSERT_ZERO(pthread_mutex_unlock(&mutex_pipes));


    if (deadlock_detection && is_in_MPI_block(source) == true && tag >= 0) {
        notify_that_message_received(source, count, tag);
    }

    return MIMPI_SUCCESS;
}

int find_parent(int root) {
    int i = world_size - root;
    int n = world_size;
    int w = world_rank;

    return ((((w + i) % n) - 1) / 2 + n - i) % n;
}

int read_message_barrier(int read_descriptor, void** result_buffer, int want_to_read_from, int want_to_read_from2) {

    free(*result_buffer);

    // Nie znajdziemy wiadomości z tagiem -1, bo jej nie zapiszemy
    if (second_message_from_other_barrier != NULL && (want_to_read_from == source_2 || want_to_read_from2 == source_2)) {
        *result_buffer = second_message_from_other_barrier;
        second_message_from_other_barrier = NULL;
        source_2 = -1;
        return false;
    }
    else if (first_message_from_other_barrier != NULL && (want_to_read_from == source_1 || want_to_read_from2 == source_1)) {
        *result_buffer = first_message_from_other_barrier;
        first_message_from_other_barrier = NULL;
        source_1 = -1;
        return false;
    }


    long long int result_buffer_size = 0;
    *result_buffer = malloc(result_buffer_size);
    void* buffer = malloc(512);  // Bufor do przechowywania fragmentów i metadanych
    assert(buffer);

    int received = 0, ile = 0, z_ilu = 1, current_message_size, who_sent_it;

    while (ile < z_ilu) {

        // Odbieranie fragmentu
        pipe_read(read_descriptor, buffer, 512);
        memcpy(&who_sent_it, buffer + sizeof(int) * 2, sizeof(int)); // W barierach w miejscu tagu jest nadawca lub coś ujemnego oznaczającego liczbę barier przez które ktoś przeszedł
        if (no_of_barrier == -who_sent_it - 1) {
            someone_already_finished = true;
            continue;
        }
        else if (no_of_barrier == -who_sent_it) {
            free(buffer);
            someone_already_finished = true;
            no_of_barrier--;
            return true;
        }

        memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));

        // W następnej barierze jest inny root, więc obecny root może być w niej korzeniem i wysłać nam wiadomość, więc filtrujemy
        if (who_sent_it != want_to_read_from && who_sent_it != want_to_read_from2) {
            void* message_for_later = malloc(current_message_size);
            assert(message_for_later);
            memcpy(message_for_later, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);

            if (first_message_from_other_barrier == NULL) {
                first_message_from_other_barrier = message_for_later;
                source_1 = who_sent_it;
            }
            else if (second_message_from_other_barrier == NULL) {
                second_message_from_other_barrier = message_for_later;
                source_2 = who_sent_it;
            }

            // znowu czekamy na wiadomość
            continue;
        }


        // Rozpakowywanie metadanych z bufora
        memcpy(&ile, buffer + sizeof(int) * 0, sizeof(int));
        memcpy(&z_ilu, buffer + sizeof(int) * 1, sizeof(int));

        result_buffer_size += current_message_size;
        *result_buffer = realloc(*result_buffer, result_buffer_size);

        // Zapisywanie danych z fragmentu do głównego bufora danych
        memcpy(*result_buffer + received, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
        received += current_message_size;
    }

    free(buffer);
    // nie robimy free result_buffer, bo ten jest potrzebny poza funkcją

    return false;
}



MIMPI_Retcode MIMPI_Bcast(
        void *data,
        int count,
        int root
) {

    if (root >= world_size || root < 0) {
        return MIMPI_ERROR_NO_SUCH_RANK;
    }
    if (world_size == 1) {
        return MIMPI_SUCCESS;
    }
    if (someone_already_finished == true) {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    no_of_barrier++; // To musi być po, bo przecież nie weszliśmy

    int i = world_size - root;
    int n = world_size;
    int w = world_rank;
    int l = 2 * ((w +i) % n) + 1; // lewe dziecko jakby root = 0
    int p = 2 * ((w +i) % n) + 2;
    int l_send = (2 * w + i + 1) % n;
    int p_send = (2 * w + i + 2) % n;
    void* buffer = NULL;

    // Czekamy az wszyscy przyjdą

    if (world_rank == root || l < world_size) { // jesteśmy korzeniem lub mamy dziecko

        if (l < world_size) { // mamy lewe dziecko
            if (read_message_barrier(get_barrier_read_desc(world_rank), &buffer, l_send, p_send)) {
                free(buffer);
                no_of_barrier--;
                return MIMPI_ERROR_REMOTE_FINISHED;
            }
        }
        if (p < world_size) { // mamy prawe dziecko
            if (read_message_barrier(get_barrier_read_desc(world_rank), &buffer, p_send, l_send)) {
                free(buffer);
                no_of_barrier--;
                return MIMPI_ERROR_REMOTE_FINISHED;
            }
        }

        if (world_rank != root) { // budzimy rodzica
            void *wake_up = malloc(512);
            assert(wake_up);
            memset(wake_up, 0, 512);
            send_message_to_pipe(get_barrier_write_desc(find_parent(root)), wake_up, 100, world_rank, false, 10);
            free(wake_up);
        }
    }
    else { // jesteśmy liściem
        void* wake_up = malloc(512);
        assert(wake_up);
        memset(wake_up, 0, 512);
        send_message_to_pipe(get_barrier_write_desc(find_parent(root)), wake_up, 100, world_rank, false, 10);
        free(wake_up);
    }

    // Wysyłamy wiadomość od roota
    if (world_rank == root || l < world_size) { // jesteśmy korzeniem lub rodzicem

        if (world_rank != root) {
            if (read_message_barrier(get_barrier_read_desc(world_rank), &buffer, find_parent(root), find_parent(root))) {
                free(buffer);
                no_of_barrier--;
                return MIMPI_ERROR_REMOTE_FINISHED;
            }
            memcpy(data, buffer, count);
        }

        if (l < world_size) {
            send_message_to_pipe(get_barrier_write_desc(l_send), data, count, world_rank, false, 10);
        }
        if (p < world_size) {
            send_message_to_pipe(get_barrier_write_desc(p_send), data, count, world_rank, false, 10);
        }
    }
    else { // jestesmy lisciem
        if (read_message_barrier(get_barrier_read_desc(world_rank), &buffer, find_parent(root), find_parent(root))) {
            free(buffer);
            no_of_barrier--;
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
        memcpy(data, buffer, count);
    }

    free(buffer);

    return MIMPI_SUCCESS;
}

// Korzystamy tylko z synchronizującego efektu Bcast
MIMPI_Retcode MIMPI_Barrier() {
    void* foo_data = malloc(512);
    assert(foo_data);
    memset(foo_data, 0, 512);

    MIMPI_Retcode ret = MIMPI_Bcast(foo_data, 1, 0);
    free(foo_data);
    return ret;
}

// tablice są typu uint8_t
void reduce(void* reduced_array, void const* array_1, void const* array_2, int count, MIMPI_Op op) {

    uint8_t* reduced = (uint8_t*)reduced_array;
    uint8_t* arr1 = (uint8_t*)array_1;
    uint8_t* arr2 = (uint8_t*)array_2;

    for (int i = 0; i < count; i++) {
        if (op == MIMPI_SUM) {
            reduced[i] = arr1[i] + arr2[i];
        }
        else if (op == MIMPI_PROD) {
            reduced[i] = arr1[i] * arr2[i];
        }
        else if (op == MIMPI_MIN) {
            if (arr1[i] < arr2[i]) {
                reduced[i] = arr1[i];
            }
            else {
                reduced[i] = arr2[i];
            }
        }
        else if (op == MIMPI_MAX) {
            if (arr1[i] > arr2[i]) {
                reduced[i] = arr1[i];
            }
            else {
                reduced[i] = arr2[i];
            }
        }
    }
}

int source_1_size = 0;
int source_2_size = 0;
int remaining_fragments_1 = 0;
int remaining_fragments_2 = 0;

int read_message_reduce(int read_descriptor, void** result_buffer, int want_to_read_from, int want_to_read_from2) {

    free(*result_buffer);

    // Nie znajdziemy wiadomości z tagiem -1, bo jej nie zapiszemy
    if (second_message_from_other_barrier != NULL && (want_to_read_from == source_2 || want_to_read_from2 == source_2)) {
        *result_buffer = second_message_from_other_barrier;
        second_message_from_other_barrier = NULL;
        source_2 = -1;
        source_2_size = 0;
        return false;
    }
    else if (first_message_from_other_barrier != NULL && (want_to_read_from == source_1 || want_to_read_from2 == source_1)) {
        *result_buffer = first_message_from_other_barrier;
        first_message_from_other_barrier = NULL;
        source_1 = -1;
        source_1_size = 0;
        return false;
    }


    long long int result_buffer_size = 0;
    *result_buffer = malloc(result_buffer_size);
    void* buffer = malloc(512);  // Bufor do przechowywania fragmentów i metadanych
    assert(buffer);

    int received = 0, ile = 0, z_ilu = 1, current_message_size, who_sent_it;

    while (ile < z_ilu || remaining_fragments_1 > 0 || remaining_fragments_2 > 0) {

        // Odbieranie fragmentu
        pipe_read(read_descriptor, buffer, 512);
        memcpy(&who_sent_it, buffer + sizeof(int) * 2, sizeof(int)); // W barierach w miejscu tagu jest nadawca lub coś ujemnego oznaczającego liczbę barier przez które ktoś przeszedł
        if (no_of_barrier == -who_sent_it - 1) {
            someone_already_finished = true;
            continue;
        }
        else if (no_of_barrier == -who_sent_it) {
            free(buffer);
            someone_already_finished = true;
            no_of_barrier--;
            return true;
        }

        memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));

        // W następnej barierze jest inny root, więc obecny root może być w niej korzeniem i wysłać nam wiadomość, więc filtrujemy
        if (who_sent_it != want_to_read_from && who_sent_it != want_to_read_from2) {
            if (who_sent_it == source_1) { // Wiadomość od tego nadawcy już jest więc po prostu ją wydłużamy
                first_message_from_other_barrier = realloc(first_message_from_other_barrier, source_1_size + current_message_size);
                memcpy(first_message_from_other_barrier + source_1_size, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
                source_1_size += current_message_size;
                remaining_fragments_1--;
            } else if (who_sent_it == source_2) {
                second_message_from_other_barrier = realloc(second_message_from_other_barrier, source_2_size + current_message_size);
                memcpy(second_message_from_other_barrier + source_2_size, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
                source_2_size += current_message_size;
                remaining_fragments_2--;
            } else { // nie ma jeszcze takiej wiadomości
                void* message_for_later = malloc(current_message_size);
                assert(message_for_later);
                memcpy(message_for_later, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
                if (first_message_from_other_barrier == NULL) {
                    first_message_from_other_barrier = message_for_later;
                    source_1 = who_sent_it;
                    source_1_size = current_message_size;
                    memcpy(&remaining_fragments_1, buffer + sizeof(int) * 1, sizeof(int));
                    remaining_fragments_1--;
                }
                else if (second_message_from_other_barrier == NULL) {
                    second_message_from_other_barrier = message_for_later;
                    source_2 = who_sent_it;
                    source_2_size = current_message_size;
                    memcpy(&remaining_fragments_2, buffer + sizeof(int) * 1, sizeof(int));
                    remaining_fragments_2--;
                }
            }

            // znowu czekamy na wiadomość
            continue;
        }


        // Rozpakowywanie metadanych z bufora
        memcpy(&ile, buffer + sizeof(int) * 0, sizeof(int));
        memcpy(&z_ilu, buffer + sizeof(int) * 1, sizeof(int));

        result_buffer_size += current_message_size;
        *result_buffer = realloc(*result_buffer, result_buffer_size);

        // Zapisywanie danych z fragmentu do głównego bufora danych
        memcpy(*result_buffer + received, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
        received += current_message_size;
    }

    free(buffer);
    // nie robimy free result_buffer, bo ten jest potrzebny poza funkcją

    return false;
}

MIMPI_Retcode MIMPI_Reduce(
        void const *send_data,
        void *recv_data,
        int count,
        MIMPI_Op op,
        int root
) {

    if (root >= world_size || root < 0) {
        return MIMPI_ERROR_NO_SUCH_RANK;
    }

    if (world_size == 1) {
        memcpy(recv_data, send_data, count);
        return MIMPI_SUCCESS;
    }

    // Wywołujemy barierę, żeby sprawdzić, czy wszyscy przyjdą, i nie zakleszczymy się na wysyłaniu dużej tablicy
    // Moja złożoność dla count = 4000 zaoszczędza 7 czasów działania zwykłej bariery, więc możemy sobie pozwolić na dołożenie jednej
    if (count > 4000) {
        void* foo_data = malloc(512);
        assert(foo_data);
        memset(foo_data, 0, 512);

        MIMPI_Retcode ret = MIMPI_Bcast(foo_data, 1, root);
        free(foo_data);

        if (ret != MIMPI_SUCCESS) {
            return ret;
        }
    }

    if (someone_already_finished == true) {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    no_of_barrier++; // To musi być po, bo przecież nie weszliśmy

    void* array_1 = malloc(count);
    assert(array_1);
    void* array_2 = malloc(count);
    assert(array_2);
    void* reduced_array = malloc(count);
    assert(reduced_array);

    memcpy(reduced_array, send_data, count);

    int i = world_size - root;
    int n = world_size;
    int w = world_rank;

    int l = 2 * ((w +i) % n) + 1; // lewe dziecko jakby root = 0
    int p = 2 * ((w +i) % n) + 2;
    int l_send = (2 * w + i + 1) % n;
    int p_send = (2 * w + i + 2) % n;

    // Wysyłany tag oznacza numer nadawcy, żeby czytający wiedział, od kogo jest dana porcja z pipe'a

    if (world_rank == root || l < world_size) { // jesteśmy korzeniem lub mamy dziecko

        if (l < world_size && p >= world_size) { // mamy tylko lewe dziecko
            if (read_message_reduce(get_barrier_read_desc(world_rank), &array_1, l_send, l_send)) {
                free(array_1);
                free(array_2);
                free(reduced_array);
                no_of_barrier--;
                return MIMPI_ERROR_REMOTE_FINISHED;
            }

            reduce(reduced_array, send_data, array_1, count, op);
        }
        else if (l < world_size && p < world_size) { // mamy dwójkę dzieci

            // Odczytana część może być od dowolnego z dzieci
            int max_message_size = 512 - sizeof(int) * META_INFO_SIZE;
            int liczba_czesci = (int)(((long long int)count + ((long long int)max_message_size - 1)) / (long long int)max_message_size); // ceil(A/B) use (A + (B-1)) / B
            void* buffer = malloc(512);  // Bufor do przechowywania fragmentów i metadanych
            assert(buffer);
            int ile = 0, z_ilu = 1, ranga_nadawcy, current_message_size;
            int received_1 = 0;
            int received_2 = 0;
            int pierwsza_ranga;


            int liczba_czesci_1 = liczba_czesci;
            int liczba_czesci_2 = liczba_czesci;
            // Sprawdzamy, czy taka wiadomość już była odebrana wcześniej
            // Nie znajdziemy wiadomości z tagiem -1, bo jej nie zapiszemy
            if (second_message_from_other_barrier != NULL && (l_send == source_2 || p_send == source_2)) {
                free(array_2);
                array_2 = second_message_from_other_barrier;
                second_message_from_other_barrier = NULL;
                source_2 = -1;
                source_2_size = 0;
                liczba_czesci_2 = 0;
            }
            if (first_message_from_other_barrier != NULL && (l_send == source_1 || p_send == source_1)) {

                if (liczba_czesci_2 ==  0) {
                    free(array_1);
                    array_1 = first_message_from_other_barrier;
                } else {
                    free(array_2);
                    array_2 = first_message_from_other_barrier;
                }
                first_message_from_other_barrier = NULL;
                source_1 = -1;
                source_1_size = 0;
                liczba_czesci_1 = 0;
            }

            for (int j = 0; j < liczba_czesci_1 + liczba_czesci_2; j++) {

                pipe_read(get_barrier_read_desc(world_rank), buffer, 512); // Nie uzywamy read_message_from_pipe bo tutaj mozemy miec fragmenty w dowolnej kolejnosci, wiec robimy fragment po fragmencie
                // Rozpakowywanie metadanych z bufora
                memcpy(&ile, buffer + sizeof(int) * 0, sizeof(int));
                memcpy(&z_ilu, buffer + sizeof(int) * 1, sizeof(int));
                memcpy(&ranga_nadawcy, buffer + sizeof(int) * 2, sizeof(int));
                memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));

                while (ranga_nadawcy < 0) {
                    if (no_of_barrier == -ranga_nadawcy - 1) {
                        someone_already_finished = true;
                    }
                    else if (no_of_barrier == -ranga_nadawcy) {
                        free(buffer);
                        free(array_1);
                        free(array_2);
                        free(reduced_array);
                        someone_already_finished = true;
                        no_of_barrier--;
                        return MIMPI_ERROR_REMOTE_FINISHED;
                    }

                    // Rozpakowywanie metadanych z bufora
                    pipe_read(get_barrier_read_desc(world_rank), buffer, 512);
                    memcpy(&ile, buffer + sizeof(int) * 0, sizeof(int));
                    memcpy(&z_ilu, buffer + sizeof(int) * 1, sizeof(int));
                    memcpy(&ranga_nadawcy, buffer + sizeof(int) * 2, sizeof(int));
                    memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));
                }


                if (j == 0) {
                    pierwsza_ranga = ranga_nadawcy;
                }

                if (ranga_nadawcy == pierwsza_ranga) {
                    memcpy(array_1 + received_1, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
                    received_1 += current_message_size;
                } else {
                    memcpy(array_2 + received_2, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
                    received_2 += current_message_size;
                }
            }

            free(buffer);

            reduce(reduced_array, send_data, array_1, count, op);
            reduce(reduced_array, reduced_array, array_2, count, op);
        }

        if (world_rank == root) { // jesteśmy korzeniem
            // zapisujemy sobie wynik
            memcpy(recv_data, reduced_array, count);
        } else {
            send_message_to_pipe(get_barrier_write_desc(find_parent(root)), reduced_array, count, world_rank, false, 10); // wysyłamy wiadomość rodzicowi
        }
    }
    else { // jesteśmy liściem
        send_message_to_pipe(get_barrier_write_desc(find_parent(root)), send_data, count, world_rank, false, 10); // wysyłamy naszą tablicę rodzicowi
    }

    free(array_1);
    free(array_2);
    free(reduced_array);


    // Czekamy aż root obudzi wszystkich
    void* wake_up = malloc(512);
    assert(wake_up);
    memset(wake_up, 0, 512);
    void* foo_buffer = malloc(512);
    assert(foo_buffer);
    memset(foo_buffer, 0, 512);

    // Wysyłamy wiadomość od roota
    if (world_rank == root || l < world_size) { // jesteśmy korzeniem lub rodzicem

        if (world_rank != root) {
            if (read_message_reduce(get_barrier_read_desc(world_rank), &foo_buffer, find_parent(root), find_parent(root))) {
                free(wake_up);
                free(foo_buffer);
                no_of_barrier--;
                return MIMPI_ERROR_REMOTE_FINISHED;
            }
        }

        if (l < world_size) {
            send_message_to_pipe(get_barrier_write_desc(l_send), wake_up, 100, world_rank, false, 10);
        }
        if (p < world_size) {
            send_message_to_pipe(get_barrier_write_desc(p_send), wake_up, 100, world_rank, false, 10);
        }
    }
    else { // jestesmy lisciem
        if (read_message_reduce(get_barrier_read_desc(world_rank), &foo_buffer, find_parent(root), find_parent(root))) {
            free(wake_up);
            free(foo_buffer);
            no_of_barrier--;
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }

    free(foo_buffer);
    free(wake_up);

    return MIMPI_SUCCESS;
}