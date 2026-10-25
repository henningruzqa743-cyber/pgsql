#include <assert.h>
#include <string.h>

#define LINK_TEST
#include "link_search.c"

int main(void)
{
    char a[32] = {1}, b[32] = {2}, c[32] = {3};
    LinkData *head = InitLink(32, a);
    assert(head != NULL);
    assert(head->prev == head && head->next == head);
    assert(FindNode(head, 32, a) == head);
    assert(FindNode(head, 32, b) == NULL);

    AppendNode(head, 32, b);
    AppendNode(head, 32, c);
    assert(head->prev->data[0] == 3);
    assert(head->next->prev == head);
    assert(head->prev->next == head);
    assert(FindNode(head, 32, b) == head->next);
    assert(FindNode(head, 32, c) == head->prev);
    assert(FindNode(head, 31, b) == NULL);
    c[1] = 4;
    assert(FindNode(head, 32, c) == NULL);
    FreeLink(head);

    char large[512] = {0};
    assert(InitLink(31, large) == NULL);
    head = InitLink(512, large);
    assert(head != NULL);
    for (int i = 1; i <= 10000; ++i) {
        memcpy(large, &i, sizeof(i));
        AppendNode(head, 512, large);
    }
    assert(FindNode(head, 512, large) == head->prev);
    assert(head->prev->next == head);
    FreeLink(head);
    return 0;
}
