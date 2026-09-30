#ifndef ATENA_TOOLS_H
#define ATENA_TOOLS_H
#include "atena/status.h"
AtenaStatus atena_tool_execute(const char *name,const char *arguments_json,char **out_json);
AtenaStatus atena_tool_list_json(char **out_json);
#endif
