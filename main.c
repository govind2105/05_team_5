 
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <pthread.h> 
#include <time.h> 
 
#define ARRAY_SIZE 10 
#define NUM_READERS 5 
#define NUM_WRITERS 2 
#define READER_LOOPS 5 
#define WRITER_LOOPS 3 
 
/* ========== Reader‑Writer Lock with Writer Preference ========== */ 
typedef struct rwlock { 
    int readers;               // number of active readers 
    int writers;               // number of active writers (0 or 1) 
    int waiting_writers;       // writers waiting for the lock 
    pthread_mutex_t mutex;      // protects the structure 
    pthread_cond_t readers_cond; // condition for readers to wait 
    pthread_cond_t writers_cond; // condition for writers to wait 
} rwlock_t; 
 
void rwlock_init(rwlock_t *lock) { 
    lock->readers = 0; 
    lock->writers = 0; 
    lock->waiting_writers = 0; 
    pthread_mutex_init(&lock->mutex, NULL); 
    pthread_cond_init(&lock->readers_cond, NULL); 
    pthread_cond_init(&lock->writers_cond, NULL); 
} 
 
void rwlock_rdlock(rwlock_t *lock) { 
    pthread_mutex_lock(&lock->mutex); 
    // Writer preference: block if a writer is active OR any writer is waiting 
    while (lock->writers > 0 || lock->waiting_writers > 0) { 
        pthread_cond_wait(&lock->readers_cond, &lock->mutex); 
    } 
    lock->readers++; 
    pthread_mutex_unlock(&lock->mutex); 
} 
 
void rwlock_wrlock(rwlock_t *lock) { 
    pthread_mutex_lock(&lock->mutex); 
    lock->waiting_writers++; 
    while (lock->readers > 0 || lock->writers > 0) { 
        pthread_cond_wait(&lock->writers_cond, &lock->mutex); 
    } 
    lock->waiting_writers--; 
    lock->writers++; 
    pthread_mutex_unlock(&lock->mutex); 
} 
 
void rwlock_unlock(rwlock_t *lock) { 
    pthread_mutex_lock(&lock->mutex); 
    if (lock->writers > 0) { 
        // writer releasing 
        lock->writers--; 
    } else { 
        // reader releasing 
        lock->readers--; 
    } 
 
    // If no readers or writers are active, wake up waiting threads 
    if (lock->readers == 0 && lock->writers == 0) { 
        if (lock->waiting_writers > 0) { 
            // Prefer writers 
            pthread_cond_signal(&lock->writers_cond); 
        } else { 
            // No waiting writers, wake all readers 
            pthread_cond_broadcast(&lock->readers_cond); 
        } 
    } 
    pthread_mutex_unlock(&lock->mutex); 
} 
 
void rwlock_destroy(rwlock_t *lock) { 
    pthread_mutex_destroy(&lock->mutex); 
    pthread_cond_destroy(&lock->readers_cond); 
    pthread_cond_destroy(&lock->writers_cond); 
} 
 
/* ========== Shared Array and Thread Functions ========== */ 
int shared_array[ARRAY_SIZE]; 
rwlock_t rwlock; 
 
void init_array() { 
    for (int i = 0; i < ARRAY_SIZE; i++) 
        shared_array[i] = i; 
} 
 
void* reader(void* arg) { 
    int id = *(int*)arg; 
    for (int loop = 0; loop < READER_LOOPS; loop++) { 
        rwlock_rdlock(&rwlock); 
        printf("Reader %d acquired read lock (loop %d)\n", id, loop); 
 
        int sum = 0; 
        for (int i = 0; i < ARRAY_SIZE; i++) { 
            sum += shared_array[i]; 
            usleep(1000); // simulate work 
        } 
        printf("Reader %d read sum = %d\n", id, sum); 
 
        printf("Reader %d releasing read lock\n", id); 
        rwlock_unlock(&rwlock); 
        usleep(rand() % 10000); // random delay 
    } 
    return NULL; 
} 
 
void* writer(void* arg) { 
    int id = *(int*)arg; 
    for (int loop = 0; loop < WRITER_LOOPS; loop++) { 
        rwlock_wrlock(&rwlock); 
        printf("Writer %d acquired write lock (loop %d)\n", id, loop); 
 
        for (int i = 0; i < ARRAY_SIZE; i++) { 
            shared_array[i]++; 
            usleep(2000); // simulate work 
        } 
        printf("Writer %d finished writing\n", id); 
 
        printf("Writer %d releasing write lock\n", id); 
        rwlock_unlock(&rwlock); 
        usleep(rand() % 20000); // random delay 
    } 
    return NULL; 
} 
 
int main() { 
    srand(time(NULL)); 
    init_array(); 
    rwlock_init(&rwlock); 
 
    pthread_t readers[NUM_READERS], writers[NUM_WRITERS]; 
    int reader_ids[NUM_READERS], writer_ids[NUM_WRITERS]; 
 
    for (int i = 0; i < NUM_READERS; i++) { 
        reader_ids[i] = i; 
        pthread_create(&readers[i], NULL, reader, &reader_ids[i]); 
    } 
    for (int i = 0; i < NUM_WRITERS; i++) { 
        writer_ids[i] = i; 
        pthread_create(&writers[i], NULL, writer, &writer_ids[i]); 
    } 
 
    for (int i = 0; i < NUM_READERS; i++) 
        pthread_join(readers[i], NULL); 
    for (int i = 0; i < NUM_WRITERS; i++) 
        pthread_join(writers[i], NULL); 
 
    rwlock_destroy(&rwlock); 
    return 0; 
} 
 
