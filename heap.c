#include "types.h"
#include "mem.h"
#include "heap.h"

/* kheap and kmalloc are the kernel versions */

extern uint32_t _suserheap;
extern uint32_t _euserheap;
extern uint32_t _skernelheap;
extern uint32_t _ekernelheap;

/*-------------------------- Simple heap, by mgyenik -----------------------------*/

/* This struct can be made larger to enlarge the minimum block used for allocation. Decreases overhead for defragmentation,
   but increases how much memory silly little things use. */
typedef struct heap_node_struct{
    struct heap_node_struct* next_node;
}heapNode;

#define HEAP_BLOCK_SIZE sizeof(heapNode*) /* Total size (in bytes) of one heapNode for use in allocing mem. */

typedef struct heap_list_struct{
    heapNode* head;
    heapNode* tail;
}heapList;

static heapList k_heapList = {  NULL ,
                                NULL };

static heapList u_heapList = {  NULL ,
                                NULL };

void init_uheap(void){
    u_heapList.head = (heapNode *)&_suserheap;                /* Set heap to first address in heap area. */
    u_heapList.tail  = (heapNode *)&_euserheap;                /* This is the end of the heap. We don't frack with mem after this address. */
    heapNode* curr_node = u_heapList.head;          /* Pointer to heap block we are setting up. */
    while(curr_node < u_heapList.tail){
        curr_node->next_node = (heapNode*) ((uint32_t) curr_node + HEAP_BLOCK_SIZE);  /* Hack to ensure that HEAP_BLOCK_SIZE is added as bytes instead of words */
        curr_node = curr_node->next_node;
    }
    u_heapList.tail->next_node = NULL;              /* Clear next node of tail */
}

void init_kheap(void){
    k_heapList.head = (heapNode *)&_skernelheap;                /* Set heap to first address in heap area. */
    k_heapList.tail  = (heapNode *)&_ekernelheap;                /* This is the end of the heap. We don't frack with mem after this address. */
    heapNode* curr_node = k_heapList.head;          /* Pointer to heap block we are setting up. */
    while(curr_node < k_heapList.tail){
        curr_node->next_node = (heapNode*) ((uint32_t) curr_node + HEAP_BLOCK_SIZE);  /* Hack to ensure that HEAP_BLOCK_SIZE is added as bytes instead of words */
        curr_node = curr_node->next_node;
    }
    k_heapList.tail->next_node = NULL;              /* Clear next node of tail */
}

void* malloc(int size, uint16_t aligned){
    /* TODO: Ensure memory block returned is contiguous */
    /* TODO: Return errors; Corollary: Add error checking to everything using malloc */
    int needed_blocks = size/(HEAP_BLOCK_SIZE)+ 1;
    heapNode* prev_node = NULL;
    heapNode* break_node = u_heapList.head;         /* Node at which we jump to after the break */
    if (aligned) {
        /* Start at an aligned memory location */
        while ((uint32_t) break_node % size) {
            prev_node = break_node;
            break_node = break_node->next_node;
        }
    }
    heapNode* ret_node = break_node;
    /* Find which node will be new head, then return old head, which is first mem location in newly alloc'd mem */
    for(int i = 0; i < needed_blocks; i++){
        break_node = break_node->next_node;
    }

    if (prev_node != NULL) {
        /* Connect list, skipping returned region */
        prev_node->next_node = break_node;
    }
    else {
        /* Move head */
        u_heapList.head = break_node;
    }
    return ret_node;
}

void* kmalloc(int size){
    /* TODO: Ensure memory block returned is contiguous */
    /* TODO: Return errors; Corollary: Add error checking to everything using malloc */
    int needed_blocks = size/(HEAP_BLOCK_SIZE)+ 1;
    heapNode* break_node = k_heapList.head;         /* Node at which we jump to after the break */
    heapNode* ret_node = NULL;
    /* Find which node will be new head, then return old head, which is first mem location in newly alloc'd mem */
    for(int i = 0; i < needed_blocks; i++){
        break_node = break_node->next_node;
    }
    ret_node = k_heapList.head;                     /* Head now points to alloc'd mem. Get ready to return that mem. */
    k_heapList.head = break_node;                   /* Move head forward to break node. Heap has grown upwards. */
    return ret_node;
}

void* kmalloc_test(void){
    void* stuff = kmalloc(20);
    return stuff;
}