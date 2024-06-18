#ifndef MAR_ALLOC_H
#define MAR_ALLOC_H

#include <unistd.h>
#include <pthread.h>
#include <string.h>


typedef char ALIGN[16];

union header {
    struct {
        size_t size;
        unsigned isFree;
        union header* next;
    } h;
    ALIGN stub;
};

typedef union header header_t;

header_t* head,* tail;
pthread_mutex_t globalMallocLock;

header_t* getFreeBlock (size_t size) {
    
    header_t* currentBlock = head;

    while (currentBlock) {

        if (currentBlock->h.isFree && currentBlock->h.size >= size) return currentBlock;

        currentBlock->h.next;
    }

    return NULL;
}

void* marAlloc (size_t size) {

    size_t totalSize;
    void* memoryBlock;
    header_t* header;

    if (!size) return NULL;

    pthread_mutex_lock (&globalMallocLock);
    header = getFreeBlock (size);

    if (header) {
        
        header->h.isFree = 0;
        pthread_mutex_unlock (&globalMallocLock);

        return (void*) (header + 1);
    }

    totalSize = sizeof (header_t) + size;
    memoryBlock = sbrk (totalSize);

    if (memoryBlock == (void*) -1) {

        pthread_mutex_unlock (&globalMallocLock);
        return NULL;
    }

    header = memoryBlock;
    header->h.size = size;
    header->h.isFree = 0;
    header->h.next = NULL;

    if (!head)  head = header;
    if (tail)   tail->h.next = header;
    tail = header;

    pthread_mutex_unlock (&globalMallocLock);
    return (void*) (header + 1);
}

void marFree (void* memoryBlock) {

    header_t* header,* tmpHeader;
    void* brk;

    if (!memoryBlock) return;

    pthread_mutex_lock (&globalMallocLock);
    header = (header_t*) memoryBlock - 1;

    brk = sbrk (0);

    if ((char*) memoryBlock + header->h.size == brk) {

        if (head == tail) { head = tail = NULL; }
        else {

            tmpHeader = head;
            while (tmpHeader) {

                if (tmpHeader->h.next == tail) {

                    tmpHeader->h.next = NULL;
                    tail = tmpHeader;
                }

                tmpHeader = tmpHeader->h.next;
            }
        }

        sbrk (0 - sizeof (header_t) - header->h.size);
        pthread_mutex_unlock (&globalMallocLock);
        return;
    }

    header->h.isFree = 1;
    pthread_mutex_unlock (&globalMallocLock);
}

void* marCalloc (size_t num, size_t nsize) {

    size_t size;
    void* memoryBlock;

    if (!num || !nsize) return NULL;
    size = num * nsize;

    if (nsize != size / num) return NULL;
    memoryBlock = marAlloc (size);

    if (!memoryBlock) return NULL;

    memset (memoryBlock, 0, size);
    return memoryBlock;
}

void* marRealloc (void* memoryBlock, size_t size) {

    header_t* header;

    void* ret;

    if (!memoryBlock || !size) return marAlloc (size);
    header = (header_t*) memoryBlock - 1;

    if (header->h.size >= size) return memoryBlock;
    ret = marAlloc (size);

    if (ret) { memcpy (ret, memoryBlock, header->h.size); marFree (memoryBlock); }

    return ret;
}

#endif