#include <stdint.h>
#include <stdio.h>

#include <algorithm>

#include "malloc_internal.h"

int main () {
  const char *name = "GENERATED_CONSTANTS_H";
  printf("#ifndef %s\n", name);
  printf("#define %s\n", name);
  printf("// Do not edit this file.  This file is automatically generated\n");
  printf("//  from %s\n\n", __FILE__);
  printf("#include \"malloc_internal.h\"\n");
  printf("#include <stdlib.h>\n");
  printf("#include <stdint.h>\n");
  printf("#include <sys/types.h>\n");
  const uint64_t linesize = 64;
  const uint64_t overhead = linesize;
  printf("// In small and medium pages we reserve the first cache line\n");
  printf("// (%ld bytes) for a in-use bitmap.\n", overhead);

  printf("// We obtain hugepages from the operating system via mmap(2).\n");
  printf("// By `hugepage', I mean only mmapped pages.\n");
  printf("// By `page', I mean only a page inside a hugepage.\n");
  printf("// Each hugepage has a bin number.\n");

  printf("// We use a static array to keep track of the bin of each a hugepage.\n");
  printf("//  Bins [0..first_huge_bin_number) give the size of an object.\n");
  printf("//  Larger bin numbers B indicate the object size, coded as\n");
  printf("//     malloc_usable_size(object) = page_size*(bin_of(object)-first_huge_bin_number;\n");

  struct static_bin_t { uint32_t object_size; uint32_t objects_per_page; };
  std::vector<static_bin_t> static_bins;

  printf("static const struct { uint32_t object_size; uint32_t objects_per_page; } static_bin_info[] __attribute__((unused)) = {\n");
  printf("// The first class of small objects have sizes of the form c<<k where c is 4, 5, 6 or 7.\n");
  printf("//   objsize objects_per_page   bin   wastage\n");
  int bin = 0;

  uint64_t prev_size = 0;
  for (uint64_t k = 8; 1; k*=2) {
    for (uint64_t c = 4; c <= 7; c++) {
      uint32_t objsize = (c*k)/4;
      uint32_t n_objects_per_page = (pagesize-overhead)/objsize;
      if (n_objects_per_page <= 12) goto class2;
      prev_size = objsize;
      uint64_t wasted = pagesize - overhead - n_objects_per_page * objsize;
      printf(" {%4u, %3u},  //   %3d     %3ld\n", objsize, n_objects_per_page,  bin++, wasted);
      struct static_bin_t b = {objsize, n_objects_per_page};
      static_bins.push_back(b);
    }
  }
class2:
  uint64_t largest_small;
  printf("// Class 2 small objects are chosen to fit as many in a page as can fit.\n");
  printf("// Class 2 objects are always a multiple of a cache line.\n");
  for(uint32_t n_objects_per_page = 12; n_objects_per_page > 1; n_objects_per_page--) {
    uint32_t objsize = ((pagesize-overhead)/n_objects_per_page) & ~(linesize-1);
    if (objsize <= prev_size) continue;
    prev_size = objsize;
    uint64_t wasted  = pagesize - overhead - n_objects_per_page * objsize;
    largest_small = objsize;
    printf(" {%4u, %3u},  //   %3d       %3ld\n", objsize, n_objects_per_page, bin++, wasted);
    struct static_bin_t b = {objsize, n_objects_per_page};
    static_bins.push_back(b);
  }
  printf("// large objects (page allocated):\n");
  printf("//  So that we can return an accurate malloc_usable_size(), we maintain (in the first page of each largepage chunk) the number of actual pages allocated as an array of short[512].\n");
  uint32_t largest_waste_at_end = log_chunksize - 5;
  printf("//  This introduces fragmentation.  This fragmentation doesn't matter much since it will be demapped. For sizes up to 1<<%d we waste the last potential object.\n", largest_waste_at_end);
  printf("//   for the larger stuff, we reduce the size of the object slightly which introduces some other fragmentation\n");
  int first_large_bin = bin;
  for (uint64_t log_allocsize = 12; log_allocsize < log_chunksize; log_allocsize++) {
    struct static_bin_t b = {1u<<log_allocsize, 1};
    const char *comment = "";
    if (log_allocsize > largest_waste_at_end) {
      b.object_size -= pagesize; 
      comment = " (reserve a page for the list of sizes)";
    }
    printf(" {%u, 1}, //   %3d %s\n", b.object_size, bin++, comment);
    static_bins.push_back(b);
  }
  binnumber_t first_huge_bin = bin;
  printf("// huge objects (chunk allocated) start  at this size.\n");
  printf(" {%ld, 1}};// %3d\n", chunksize, bin++);
  printf("static const size_t largest_small         = %lu;\n", largest_small);
  const size_t largest_large = (1ul<<(log_chunksize-1))-pagesize;
  printf("static const size_t largest_large         = %lu;\n", largest_large);
  printf("static const binnumber_t first_large_bin_number = %u;\n", first_large_bin);
  printf("static const binnumber_t first_huge_bin_number   = %u;\n", first_huge_bin);
  //  printf("static const uint64_t    slot_size               = %u;\n", slot_size);
  

  printf("struct dynamic_small_bin_info {\n");
  printf("  union {\n");
  printf("    struct {\n");
  {
    int count = 0;
    for (int b = 0; b < first_large_bin;  b++ ) {
      printf("      void *b%d[%d];\n", b, static_bins[b].objects_per_page+1);
      count += static_bins[b].objects_per_page+1;
    }
    printf("    };\n");
    printf("    void *b[%d];\n", count);
  }
  printf("  };\n");
  printf("};\n");
  printf("static int dynamic_small_bin_offset(binnumber_t bin) __attribute((pure)) __attribute__((unused)) __attribute__((warn_unused_result));\n");
  printf("static int dynamic_small_bin_offset(binnumber_t bin) {\n");
  printf("  if (0) {\n");
  printf("    switch(bin) {\n");
  {
    int count = 0;
    for (int b = 0; b < first_large_bin;  b++ ) {
      printf("      case %d: return %d;\n", b, count);
      count += static_bins[b].objects_per_page+1;
    }
  }
  printf("    }\n");
  printf("    abort(); // cannot get here.\n");
  printf("  } else {\n");
  printf("    const static int offs[]={");
  {
    int count = 0;
    for (int b = 0; b < first_large_bin;  b++ ) {
      if (b>0) printf(", ");
      printf("%d", count);
      count += static_bins[b].objects_per_page+1;
    }
  }
  printf("};\n");
  printf("    return offs[bin];\n");
  printf("  }\n");
  printf("}\n");

  printf("static binnumber_t size_2_bin(size_t size) __attribute((unused)) __attribute((const));\n");
  printf("static binnumber_t size_2_bin(size_t size) {\n");
  for (binnumber_t b = 0; b < first_huge_bin; b++) {
    printf("  if (size <= %d) return %d;\n", static_bins[b].object_size, b);
  }
  printf("  return %u + ceil(size-%lu, %lu);\n", first_huge_bin-1, largest_large, pagesize);
  printf("}\n");

  printf("static size_t bin_2_size(binnumber_t bin) __attribute((unused)) __attribute((const));\n");
  printf("static size_t bin_2_size(binnumber_t bin) {\n");
  for (binnumber_t b = 0; b < first_huge_bin; b++) {
    printf("  if (bin == %d) return %u;\n", b, static_bins[b].object_size);
  }
  printf("  return (bin-%d)*pagesize + %lu;\n", first_huge_bin-1, largest_large);
  printf("}\n");

  printf("#endif\n");
  return 0;
}