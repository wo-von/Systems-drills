#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct ListNode {
    int val;
    struct ListNode *next;
};

struct ListNode *reverseList(struct ListNode *head) {
    struct ListNode *prev = NULL;
    struct ListNode *curr = head;
    struct ListNode *next;
    while (curr != NULL) {
        next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}

struct ListNode *build_array(int nums[], size_t length) {
    struct ListNode *addr[length + 1];
    for (size_t i = 0; i < length; i++) {
        struct ListNode *node = malloc(sizeof(struct ListNode));
        if (node == NULL) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
        }
        addr[i] = node;
    }
    addr[length] = NULL;
    for (size_t i = 0; i < length; i++) {
        addr[i]->val = nums[i];
        addr[i]->next = addr[i + 1];
    }
    struct ListNode *head = addr[0];
    return head;
}

void destroy_list(struct ListNode *head) {
    struct ListNode *curr = head;
    struct ListNode *next;
    while (curr != NULL) {
        next = curr->next;
        free(curr);
        curr = next;
    }
}

int main(void) {
    {
        int arr[] = {1, 2, 3, 4, 5, 6};
        size_t array_size = sizeof arr / sizeof arr[0];
        struct ListNode *test = build_array(arr, array_size);
        struct ListNode *newhead = reverseList(test);
        struct ListNode *curr = newhead;
        int sentinel = array_size;
        while (curr != NULL) {
            assert(curr->val == sentinel);
            curr = curr->next;
            sentinel--;
        }
        assert(sentinel == 0);
        destroy_list(newhead);
    }
    {
        int arr[] = {1, 2};
        size_t array_size = sizeof arr / sizeof arr[0];
        struct ListNode *test = build_array(arr, array_size);
        struct ListNode *newhead = reverseList(test);
        struct ListNode *curr = newhead;
        int sentinel = array_size;
        while (curr != NULL) {
            assert(curr->val == sentinel);
            curr = curr->next;
            sentinel--;
        }
        assert(sentinel == 0);
        destroy_list(newhead);
    }
    {
        struct ListNode *newhead = reverseList(NULL);
        struct ListNode *curr = newhead;
        int sentinel = 0;
        while (curr != NULL) {
            assert(curr->val == sentinel);
            curr = curr->next;
            sentinel--;
        }
        assert(sentinel == 0);
        destroy_list(newhead);
    }
    return 0;
}
