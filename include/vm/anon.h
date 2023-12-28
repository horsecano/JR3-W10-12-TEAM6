#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "filesys/off_t.h"
struct page;
enum vm_type;

struct anon_page
{
    /* TODO
    1. 스왑 공간의 주소를 저장해야하지 않을까?
    */
   off_t swap_offset;

};

void vm_anon_init(void);
bool anon_initializer(struct page* page, enum vm_type type, void* kva);

#endif
