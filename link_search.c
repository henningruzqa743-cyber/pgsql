#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

typedef struct LinkData
{
    struct LinkData *prev;
    struct LinkData *next;
    int type;
    int dataSize;
    char *data;
} LinkData;

static LinkData *NewNode(int size, const char *mem)
{
    if (size < 32 || size > 512 || mem == NULL)
        return NULL;
    LinkData *node = malloc(sizeof(*node) + (size_t) size);
    if (node == NULL)
        return NULL;
    node->type = 0;
    node->dataSize = size;
    node->data = (char *) (node + 1);
    memcpy(node->data, mem, (size_t) size);
    return node;
}

LinkData *InitLink(int size, char *mem)
{
    LinkData *head = NewNode(size, mem);
    if (head != NULL)
        head->prev = head->next = head;
    return head;
}

void AppendNode(LinkData *firstNode, int size, char *mem)
{
    if (firstNode == NULL) {
        fputs("AppendNode: null head\n", stderr);
        exit(EXIT_FAILURE);
    }
    LinkData *node = NewNode(size, mem);
    if (node == NULL) {
        fputs("AppendNode: invalid size or allocation failed\n", stderr);
        exit(EXIT_FAILURE);
    }
    node->prev = firstNode->prev;
    node->next = firstNode;
    firstNode->prev->next = node;
    firstNode->prev = node;
}

LinkData *FindNode(LinkData *firstNode, int size, char *mem)
{
    if (firstNode == NULL || mem == NULL || size < 32 || size > 512)
        return NULL;
    LinkData *node = firstNode;
    do {
        if (node->dataSize == size && memcmp(node->data, mem, (size_t) size) == 0)
            return node;
        node = node->next;
    } while (node != firstNode);
    return NULL;
}

void FreeLink(LinkData *head)
{
    if (head == NULL)
        return;
    LinkData *node = head->next;
    while (node != head) {
        LinkData *next = node->next;
        free(node);
        node = next;
    }
    free(head);
}

#ifndef LINK_TEST
#ifdef __linux__
enum { EVENT_COUNT = 4 };

static int OpenEvent(uint32_t type, uint64_t config, int group_fd)
{
    struct perf_event_attr attr = {0};
    attr.size = sizeof(attr);
    attr.type = type;
    attr.config = config;
    attr.disabled = group_fd == -1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED |
                       PERF_FORMAT_TOTAL_TIME_RUNNING;
    return (int) syscall(__NR_perf_event_open, &attr, 0, -1, group_fd, 0);
}

static uint64_t CacheReadEvent(uint64_t result)
{
    return PERF_COUNT_HW_CACHE_L1D |
           ((uint64_t) PERF_COUNT_HW_CACHE_OP_READ << 8) |
           (result << 16);
}

static double Scaled(uint64_t value, uint64_t enabled, uint64_t running)
{
    return running ? (double) value * (double) enabled / (double) running : 0.0;
}

static int Bench(LinkData *head, int repetitions)
{
    int fd[EVENT_COUNT] = {-1, -1, -1, -1};
    fd[0] = OpenEvent(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, -1);
    if (fd[0] == -1)
        goto open_error;
    fd[1] = OpenEvent(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, fd[0]);
    if (fd[1] == -1)
        goto open_error;
    fd[2] = OpenEvent(PERF_TYPE_HW_CACHE,
                      CacheReadEvent(PERF_COUNT_HW_CACHE_RESULT_ACCESS), fd[0]);
    if (fd[2] == -1)
        goto open_error;
    fd[3] = OpenEvent(PERF_TYPE_HW_CACHE,
                      CacheReadEvent(PERF_COUNT_HW_CACHE_RESULT_MISS), fd[0]);
    if (fd[3] == -1)
        goto open_error;

    char *volatile target = head->prev->data;
    int size = head->prev->dataSize;
    volatile uintptr_t found = 0;
    uint64_t values[3 + EVENT_COUNT] = {0};
    if (ioctl(fd[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) == -1 ||
        ioctl(fd[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP) == -1) {
        perror("perf ioctl");
        goto close_error;
    }
    for (int i = 0; i < repetitions; ++i)
        found ^= (uintptr_t) FindNode(head, size, target);
    if (ioctl(fd[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP) == -1 ||
        read(fd[0], values, sizeof(values)) != (ssize_t) sizeof(values) ||
        values[0] != EVENT_COUNT || values[2] == 0) {
        fputs("Failed to read complete perf counter group\n", stderr);
        goto close_error;
    }

    double instructions = Scaled(values[3], values[1], values[2]);
    double cycles = Scaled(values[4], values[1], values[2]);
    double accesses = Scaled(values[5], values[1], values[2]);
    double misses = Scaled(values[6], values[1], values[2]);
    printf("Searches: %d\nInstructions: %.0f\nCycles: %.0f\n", repetitions,
           instructions, cycles);
    printf("Memory accesses (L1D reads): %.0f\n", accesses);
    if (accesses > 0.0)
        printf("L1D read hit rate: %.2f%%\n", 100.0 * (1.0 - misses / accesses));
    else
        puts("L1D read hit rate: unavailable (zero accesses)");
    printf("L1D read misses: %.0f\n", misses);
    printf("Counter running fraction: %.2f%%\n",
           100.0 * (double) values[2] / (double) values[1]);
    (void) found;
    for (int i = 0; i < EVENT_COUNT; ++i)
        close(fd[i]);
    return 0;

open_error:
    perror("perf_event_open");
    fputs("Hardware event unavailable or perf_event permissions denied. "
          "Check perf_event_paranoid and CPU support.\n", stderr);
close_error:
    for (int i = 0; i < EVENT_COUNT; ++i)
        if (fd[i] != -1)
            close(fd[i]);
    return 1;
}
#endif

static int ParsePositive(const char *text, int min, int max)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || *end != '\0' || value < min || value > max)
        return -1;
    return (int) value;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s NODES DATA_BYTES REPETITIONS\n", argv[0]);
        return 2;
    }
    int count = ParsePositive(argv[1], 1, 1000000);
    int size = ParsePositive(argv[2], 32, 512);
    int repetitions = ParsePositive(argv[3], 1, 1000000);
    if (count < 0 || size < 0 || repetitions < 0) {
        fputs("NODES and REPETITIONS must be 1..1000000; DATA_BYTES must be 32..512\n", stderr);
        return 2;
    }
    char data[512] = {0};
    LinkData *head = NULL;
    for (int i = 0; i < count; ++i) {
        memcpy(data, &i, sizeof(i));
        if (i == 0) {
            head = InitLink(size, data);
            if (head == NULL) {
                fputs("InitLink: allocation failed\n", stderr);
                return 1;
            }
        } else {
            AppendNode(head, size, data);
        }
    }
#ifdef __linux__
    int result = Bench(head, repetitions);
#else
    fputs("perf_event_open requires Linux; run this program on a Linux host.\n", stderr);
    int result = 1;
#endif
    FreeLink(head);
    return result;
}
#endif
