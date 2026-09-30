#ifndef ATENA_TEST_COMMON_H
#define ATENA_TEST_COMMON_H
#include <stdio.h>
#include <stdlib.h>
#define ATENA_TEST_REQUIRE(expr) do { if(!(expr)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);fflush(stderr);return 1;} } while(0)
#define ATENA_TEST_ASSERT(expr) do { if(!(expr)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);fflush(stderr);abort();} } while(0)
#endif
