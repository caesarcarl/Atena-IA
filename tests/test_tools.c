#include "../tools/tools.h"
#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){char*j=NULL;ATENA_TEST_ASSERT(atena_tool_execute("system.info.read","{}",&j)==ATENA_OK);ATENA_TEST_ASSERT(j&&strstr(j,"machine"));free(j);j=NULL;ATENA_TEST_ASSERT(atena_tool_execute("unknown.tool","{}",&j)==ATENA_ERR_NOT_FOUND);puts("test_tools: PASS");return 0;}
