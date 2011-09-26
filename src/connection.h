/*
 * RediFS
 *
 * Redis File System based on FUSE
 * Copyright (C) 2011 Dave van Soest <dave@thebinarykid.nl>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
*/


#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include "redifs_types.h"


// ---- Defines:
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 6379


// ---- Prototypes:
extern int openRedisConnection(const char* hostIpAddr, redifs_port_t port);
extern void closeRedisConnection();
extern void releaseReplyHandle(int handle);

extern void retrieveStringArrayElements(int handle, int offset, int count, char* array[]);

extern int redisCommand_HGET(const char* key, const char* field, char** result);
extern int redisCommand_HKEYS(const char* key, int* result);
extern int redisCommand_HSET_INT(const char* key, const char* field, long long value, int* result);
extern int redisCommand_INCR(const char* key, long long* result);
extern int redisCommand_LINDEX(const char* key, long long index, char** result);
extern int redisCommand_LSET_INT(const char* key, long long index, long long value);
extern int redisCommand_SET(const char* key, const char* value);
extern int redisCommand_RPUSH_INT(const char* key, long long values[], long long value_count, int* result);


#endif // _CONNECTION_H_

