Simple buddy allocator.

A buddy allocator allocates based on halved partitions.
The allocator will halve the partitions as much as possible,
then give the final halved partition to the allocatee.

Moreover, allocations within free'd partitions can allow for
coalescing of partitions (i.e. merging partitions).

This leads to minimal external fragmentation, but results in
internal fragmentation.

In order to use this implementation; in a single translation unit,
define BUDDY_ALLOC_IMPLEMENTATION then include "buddy.h".
In all other files, include "buddy.h" normally.

Note: This doesn't actually allocate any memory. The buffer simply
has a specified size and allocations consist of offset + size.
This is to be integrated with actual memory (e.g. a GPU buffer).

Example Usage;
```
BuddyBuffer* buf = buddy_buf_new(1024);

BuddyAllocation alloc;
if (!buddy_buf_alloc(buf, 30, &alloc)) {
  printf("allocation failed!");
  return;
}

printf("offset: %zu, size: %zu", alloc.offset, alloc.size);

buddy_buf_free(alloc); // this is optional
buddy_buf_destroy(buf);
```

This is licensed under the MIT License.