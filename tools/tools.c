#include "tools.h"
#include "../core/util.h"
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static AtenaStatus system_info(char **out_json){
    SYSTEM_INFO si;MEMORYSTATUSEX mem;ZeroMemory(&mem,sizeof(mem));mem.dwLength=sizeof(mem);
    GetNativeSystemInfo(&si);if(!GlobalMemoryStatusEx(&mem))return ATENA_ERR_IO;
    json_object*o=json_object_new_object();if(!o)return ATENA_ERR_NO_MEMORY;
    json_object_object_add(o,"sysname",json_object_new_string("Windows"));json_object_object_add(o,"cpus",json_object_new_int((int)si.dwNumberOfProcessors));
    json_object_object_add(o,"ram_total_bytes",json_object_new_int64((int64_t)mem.ullTotalPhys));
    const char*arch="unknown";if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)arch="x86_64";else if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_ARM64)arch="arm64";
    json_object_object_add(o,"machine",json_object_new_string(arch));*out_json=atena_strdup(json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN));json_object_put(o);return *out_json?ATENA_OK:ATENA_ERR_NO_MEMORY;
}
static AtenaStatus disk_info(char **out_json){
    ULARGE_INTEGER freeb,total,avail;if(!GetDiskFreeSpaceExW(NULL,&avail,&total,&freeb))return ATENA_ERR_IO;json_object*o=json_object_new_object();
    json_object_object_add(o,"path",json_object_new_string("system"));json_object_object_add(o,"available_bytes",json_object_new_int64((int64_t)avail.QuadPart));json_object_object_add(o,"total_bytes",json_object_new_int64((int64_t)total.QuadPart));
    *out_json=atena_strdup(json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN));json_object_put(o);return *out_json?ATENA_OK:ATENA_ERR_NO_MEMORY;
}
#else
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>
static AtenaStatus system_info(char **out_json){struct utsname u;if(uname(&u)!=0)return ATENA_ERR_IO;long pages=sysconf(_SC_PHYS_PAGES),page_size=sysconf(_SC_PAGESIZE),cpus=sysconf(_SC_NPROCESSORS_ONLN);json_object*o=json_object_new_object();if(!o)return ATENA_ERR_NO_MEMORY;json_object_object_add(o,"sysname",json_object_new_string(u.sysname));json_object_object_add(o,"release",json_object_new_string(u.release));json_object_object_add(o,"machine",json_object_new_string(u.machine));json_object_object_add(o,"cpus",json_object_new_int64(cpus));if(pages>0&&page_size>0)json_object_object_add(o,"ram_total_bytes",json_object_new_int64((int64_t)pages*(int64_t)page_size));*out_json=atena_strdup(json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN));json_object_put(o);return *out_json?ATENA_OK:ATENA_ERR_NO_MEMORY;}
static AtenaStatus disk_info(char **out_json){struct statvfs v;if(statvfs("/",&v)!=0)return ATENA_ERR_IO;json_object*o=json_object_new_object();json_object_object_add(o,"path",json_object_new_string("/"));json_object_object_add(o,"available_bytes",json_object_new_int64((int64_t)v.f_bavail*(int64_t)v.f_frsize));json_object_object_add(o,"total_bytes",json_object_new_int64((int64_t)v.f_blocks*(int64_t)v.f_frsize));*out_json=atena_strdup(json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN));json_object_put(o);return *out_json?ATENA_OK:ATENA_ERR_NO_MEMORY;}
#endif
AtenaStatus atena_tool_execute(const char*name,const char*arguments_json,char**out_json){if(!name||!out_json)return ATENA_ERR_INVALID_ARGUMENT;*out_json=NULL;if(arguments_json&&strlen(arguments_json)>4096)return ATENA_ERR_INVALID_ARGUMENT;if(strcmp(name,"system.info.read")==0)return system_info(out_json);if(strcmp(name,"disk.info.read")==0)return disk_info(out_json);return ATENA_ERR_NOT_FOUND;}
AtenaStatus atena_tool_list_json(char**out_json){if(!out_json)return ATENA_ERR_INVALID_ARGUMENT;*out_json=atena_strdup("[{\"name\":\"system.info.read\",\"risk\":\"read\"},{\"name\":\"disk.info.read\",\"risk\":\"read\"}]");return *out_json?ATENA_OK:ATENA_ERR_NO_MEMORY;}
