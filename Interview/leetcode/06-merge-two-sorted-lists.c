#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct ListNode {
    int val;
    struct ListNode *next;
};

struct ListNode *mergeTwoLists(struct ListNode *list1, struct ListNode *list2) {
    struct ListNode dummy;
    struct ListNode *tail = &dummy;
    while (list1 != NULL && list2 != NULL) {
        if (list1->val < list2->val) {
            tail->next = list1;
            list1 = list1->next;
        } else {
            tail->next = list2;
            list2 = list2->next;
        }
        tail = tail->next;
    }
    // exhaust the one that is not yet fully consumed
    tail->next = list1 ? list1 : list2;
    return dummy.next;
}

struct ListNode *build_array(int nums[], size_t length) {
    struct ListNode dummy;
    struct ListNode *tail = &dummy;
    size_t index = 0;
    while (index < length) {
        tail->next = malloc(sizeof(struct ListNode));
        if (tail->next == NULL) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
        }
        tail = tail->next;
        tail->next = NULL;
        tail->val = nums[index++];
    }
    return dummy.next;
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
    // test case 1
    {
        int arr[] = {1, 3, 5};
        int arr2[] = {2, 4, 6};
        size_t array_size1 = sizeof arr / sizeof arr[0];
        size_t array_size2 = sizeof arr2 / sizeof arr[0];
        struct ListNode *list1 = build_array(arr, array_size1);
        struct ListNode *list2 = build_array(arr2, array_size2);
        struct ListNode *newhead = mergeTwoLists(list1, list2);
        struct ListNode *curr = newhead;
        int sentinel = 1;
        while (curr != NULL) {
            assert(curr->val == sentinel);
            curr = curr->next;
            sentinel++;
        }
        assert(sentinel == 7);
        // lists have been spliced, so one free is enough
        destroy_list(newhead);
    }
    // test case 2
    {
        int arr2[] = {1, 2, 3};
        size_t array_size2 = sizeof arr2 / sizeof arr2[0];
        struct ListNode *list1 = NULL;
        struct ListNode *list2 = build_array(arr2, array_size2);
        struct ListNode *newhead = mergeTwoLists(list1, list2);
        struct ListNode *curr = newhead;
        int sentinel = 1;
        while (curr != NULL) {
            assert(curr->val == sentinel);
            curr = curr->next;
            sentinel++;
        }
        assert(sentinel == 4);
        // lists have been spliced, so one free is enough
        destroy_list(newhead);
    }
    return 0;
}
