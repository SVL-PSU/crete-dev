#include <stdint.h>
#include <stdlib.h>
#include <map>

//#define USE_UTHASH

extern "C"{
uint64_t crete_get_dynamic_addr(uint64_t);
}


#define PAGE_ALIGN_BITS     (12)
#define PAGE_SIZE           (1<<PAGE_ALIGN_BITS)
#define PAGE_ADDRESS_MASK   (0xffffffffffffffffLL << PAGE_ALIGN_BITS)
#define PAGE_OFFSET_MASK    (~PAGE_ADDRESS_MASK)

#ifndef USE_UTHASH
uint64_t crete_get_dynamic_addr(uint64_t static_addr)
{
    // <static_page_addr, dynamic_page_addr>
    static std::map<uint64_t, uint64_t> page_addr_map;

    uint64_t static_page_addr = static_addr & PAGE_ADDRESS_MASK;
    uint64_t in_page_offest = static_addr & PAGE_OFFSET_MASK;

    if(page_addr_map.find(static_page_addr) == page_addr_map.end())
    {
        page_addr_map[static_page_addr] = (uint64_t)malloc(PAGE_SIZE);
    }

    return page_addr_map[static_page_addr] + in_page_offest;
}

#else // #ifndef USE_UTHASH
#include "uthash.h"

typedef struct {
    void *static_addr;
    void *dynamic_addr;
    UT_hash_handle hh;
} page_addr_elem_t;

static page_addr_elem_t *page_addr_map = NULL;

static page_addr_elem_t *find_page_addr_elem(void *static_addr)
{
    page_addr_elem_t *s = NULL;

    HASH_FIND_PTR(page_addr_map, &static_addr, s);
    return s;
}

static page_addr_elem_t *add_page_addr_elem(void *static_addr, void *dynamic_addr)
{
    page_addr_elem_t *s = NULL;

    HASH_FIND_PTR(page_addr_map, &static_addr, s);

    if(s == NULL)
    {
        s = (page_addr_elem_t *)malloc(sizeof(page_addr_elem_t));
        s->static_addr = static_addr;
        s->dynamic_addr = dynamic_addr;
        HASH_ADD_PTR(page_addr_map, static_addr, s);
    }

    return s;
}

uint64_t crete_get_dynamic_addr(uint64_t static_addr)
{
    uint64_t static_page_addr = static_addr & PAGE_ADDRESS_MASK;
    uint64_t in_page_offest = static_addr & PAGE_OFFSET_MASK;

    page_addr_elem_t *found_page_elem = find_page_addr_elem((void *)static_page_addr);
    if(!found_page_elem)
    {
        void *dynamic_page_addr = malloc(PAGE_SIZE);
        found_page_elem = add_page_addr_elem((void *)static_page_addr, dynamic_page_addr);
    }

    return (uint64_t)found_page_elem->dynamic_addr + in_page_offest;
}

/*
int test() {
    void *static_addrs = {0xdeadbeaf, 0xdeadbbb};
    void *s1 = 0xdeadbeaf;
    void *d1 = malloc(4096);
    add_page_addr_elem(s1, d1);

    void *s2 = 0xdead;
    void *d2 = malloc(4096);
    add_page_addr_elem(s2, d2);

    {
        void *addr = s1;
        page_addr_elem_t * found = find_page_addr_elem(addr);

        if(found)
        {
            fprintf(stderr, "found: found->static_addr = %p, found->dynamic_addr = %p\n",
                    found->static_addr, found->dynamic_addr);
        } else {
            fprintf(stderr, "NOT found: %p\n", addr);
        }
    }

    {
        void *addr = 0xabcd;
        page_addr_elem_t * found = find_page_addr_elem(addr);

        if(found)
        {
            fprintf(stderr, "found: found->static_addr = %p, found->dynamic_addr = %p\n",
                    found->static_addr, found->dynamic_addr);
        } else {
            fprintf(stderr, "NOT found: %p\n", addr);
        }
    }

    return 0;
}
*/
#endif // #ifndef USE_UTHASH
