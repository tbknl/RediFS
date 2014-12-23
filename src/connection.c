/*
 * RediFS
 *
 * Redis File System based on FUSE
 * Copyright (C) 2011 Dave van Soest <dave@thebinarykid.nl>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
*/


// ---- Includes:
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <hiredis/hiredis.h>

#include "connection.h"


struct redis_connection_info
{
    const char* host;
    redifs_port_t port;
};


// ---- Shared globals:
char scripts[NUM_SCRIPTS][MAX_SCRIPTHASH_LEN];


// ---- Redis globals:
static struct redis_connection_info redis1_info = { NULL, 0 };
static redisContext* redis1 = NULL;


#define MAX_OPEN_REPLIES 4
static redisReply* openReplies[MAX_OPEN_REPLIES];
#define OPEN_REPLIES_BITMAP_SIZE ((MAX_OPEN_REPLIES - 1) / sizeof(unsigned int)) + 1
static unsigned int openRepliesBitmap[OPEN_REPLIES_BITMAP_SIZE];


static void clearOpenReplies()
{
    int i;
    for (i = 0; i < OPEN_REPLIES_BITMAP_SIZE; ++i)
    {
        openRepliesBitmap[i] = 0;
    }
}


static int getReplyHandle(redisReply* reply)
{
    // TODO: More efficient implementation.
    
    int i;

    for (i = 0; i < MAX_OPEN_REPLIES; ++i)
    {
        if (0 == (openRepliesBitmap[i / sizeof(unsigned int)] & (1 << (i % sizeof(unsigned int)))))
        {
            openRepliesBitmap[i / sizeof(unsigned int)] |= 1 << (i % sizeof(unsigned int));
            openReplies[i] = reply;
            return i + 2;
        }
    }

    assert(0);
    return 0; // Failure.
}


void releaseReplyHandle(int handle)
{
    if (handle == 1)
    {
        return;
    }

    handle -= 2;
    assert(handle >= 0 && handle < MAX_OPEN_REPLIES);

    freeReplyObject(openReplies[handle]);
    openRepliesBitmap[handle / sizeof(unsigned int)] &= ~(1 << (handle % sizeof(unsigned int)));
}


// ---- Util functions:
static char* redifs_lltoa(long long ll, char* a, int len)
{
    int sign = ll < 0;
    ll = sign ? -ll : ll;

    a += len;

    *(--a) = '\0';

    if (ll == 0)
    {
        *(--a) = '0';
        return a;
    }

    do
    {
        *(--a) = '0' + (ll % 10);
        ll /= 10;
    } while (ll != 0);

    if (sign)
    {
        *(--a) = '-';
    }

    return a;
}


static int handleStringReply(redisReply* reply, char** result, int* len)
{
    switch (reply->type)
    {
        case REDIS_REPLY_STRING:
            *result = reply->str;
            if (len != NULL) {
                *len = reply->len;
            }
            return getReplyHandle(reply); // Success.

        case REDIS_REPLY_NIL:
            *result = NULL;
            return 1; // Success.

        default:
            assert(0);
            return 0; // Failure.
    }
}


static int handleStringArrayReply(redisReply* reply, int* result)
{
    switch (reply->type)
    {
        case REDIS_REPLY_ARRAY:
            *result = reply->elements;
            return getReplyHandle(reply); // Success.

        case REDIS_REPLY_NIL:
            *result = 0;
            return 1; // Success.

        default:
            assert(0);
            return 0; // Failure.
    }
}


static int handleIntegerReply(redisReply* reply, long long* result)
{
    switch (reply->type)
    {
        case REDIS_REPLY_INTEGER:
            *result = reply->integer;
            return getReplyHandle(reply); // Success.

        case REDIS_REPLY_STRING:
            *result = atoll(reply->str); // TODO: What if string does not contain an integer?
            return getReplyHandle(reply); // Success.

        default:
            fprintf(stderr, "Expected integer reply, got %d\n", reply->type);
            assert(0);
            return 0; // Failure.
    }
}


void retrieveStringArrayElements(int handle, int offset, int count, char* array[])
{
    redisReply* reply;
    redisReply* strReply;
    int i;

    handle -= 2;
    assert(handle >= 0 && handle < MAX_OPEN_REPLIES);
    assert(openRepliesBitmap[handle / sizeof(unsigned int)] & (1 << (handle % sizeof(unsigned int))));

    reply = openReplies[handle];

    for (i = 0; i < count; ++i)
    {
        strReply = reply->element[i + offset];
        assert(strReply->type == REDIS_REPLY_STRING);
        array[i] = strReply->str;
    }
}


// ---- Interface functions:

// Reconnect to Redis server:
static int connectToRedisServer()
{
    // Connect to Redis server:
    redis1 = redisConnect(redis1_info.host, redis1_info.port);
    if (redis1->err)
    {
        fprintf(stderr, "Error: %s\n", redis1->errstr);
        closeRedisConnection();
        return -1; // Failure.
    }

    return 0; // Success.
}


// Connect to Redis server:
int openRedisConnection(const char* host, redifs_port_t port)
{
    int result;

    // Apply default settings if needed:
    redis1_info.host = host && host[0] != '\0' ? host : DEFAULT_HOST;
    redis1_info.port = port ? port : DEFAULT_PORT;

    // Actually open the connection:
    result = connectToRedisServer();

    // Clear open replies:
    clearOpenReplies();

    return result;
}


// Close Redis connection:
void closeRedisConnection()
{
    redisFree(redis1);
    redis1 = NULL;
}


// Execute a Redis command:
redisReply* execRedisCommand2(int numArgs, const char* args[], size_t* argvlen, const int* argiszt)
{
    redisReply* reply;
    int retries;
    int i;

    // Check for Redis server connection:
    if (!redis1)
    {
        if (0 > connectToRedisServer())
        {
            fprintf(stderr, "Error: No connection to the Redis server.\n");
            return NULL;
        }
    }

    // Calculate arg lengths:
    if (argvlen != NULL && argiszt != NULL) {
        for (i = 0; i < numArgs; ++i) {
            if (argiszt[i]) {
                argvlen[i] = strlen(args[i]);
            }
        }
    }

    // Perform Redis command:
    retries = 2;
    while (1)
    {
        reply = redisCommandArgv(redis1, numArgs, args, argvlen);
        if (!reply)
        {
            // Try to reconnect:
            if (redis1->err == REDIS_ERR_EOF && retries > 1)
            {
                closeRedisConnection();
                if (--retries > 0)
                {
                    fprintf(stderr, "Connection to Redis server lost. Trying to reconnect...\n");
                    if (0 == connectToRedisServer())
                    {
                        continue;
                    }
                    else
                    {
                        return NULL;
                    }
                }
            }

            fprintf(stderr, "XY Error: %s %d\n", redis1->errstr, redis1->err);
            return NULL; // Failure.
        }
        else if (reply->type == REDIS_REPLY_ERROR)
        {
            fprintf(stderr, "Error: %s\n", redis1->errstr);
            return NULL; // Failure.
        }

        break;
    }

    return reply; // Success.
}


int loadScripts() {
    const struct { const char* key; int id; } scriptDefs[] = {
        {"getattr", SCRIPT_CHECK}, // TODO!
        {"getattr", SCRIPT_GETATTR},
        {"dir_read", SCRIPT_READDIR},
        {"file_open", SCRIPT_FILEOPEN},
        {"fileid_read_chunk", SCRIPT_FILEIDREADCHUNK},
        {"fileid_write_chunk", SCRIPT_FILEIDWRITECHUNK},
        {"dir_create", SCRIPT_DIRCREATE},
        {"file_create", SCRIPT_FILECREATE},
        {"file_truncate", SCRIPT_FILETRUNCATE},
        {"unlink", SCRIPT_UNLINK},
        {"link", SCRIPT_LINK},
    }; 
    int handle;
    char* scriptHash;
    int len;
    const char* scriptsKey = "scripts";
    int i = 0;

    for (i = 0; i < NUM_SCRIPTS; ++i) {
        handle = redisCommand_HGET(scriptsKey, scriptDefs[i].key, &scriptHash, &len);
        if (!handle)
        {
            return -EIO;
        }

        if (!scriptHash)
        {
            releaseReplyHandle(handle);
            return -ENOENT;
        }

        if (len < MAX_SCRIPTHASH_LEN) {
            strcpy(scripts[scriptDefs[i].id], scriptHash);
        }
        else {
            fprintf(stderr, "Unexpected script hash length %d\n", len);
            return -3456; // TODO: Correct error ode.
        }

        releaseReplyHandle(handle);
    }

    for (i = 0; i < NUM_SCRIPTS; ++i) {
        fprintf(stderr, "Loaded hash for %d: %s\n", i, scripts[i]);
    }

    return 0; // Success.
}


// Redis HGET command:
int redisCommand_HGET(const char* key, const char* field, char** result, int* len) {
    redisReply* reply;
    const char* args[] = { "HGET", key, field };

    reply = execRedisCommand2(3, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleStringReply(reply, result, len);
}


// Redis SCRIPT GETATTR:
int redisCommand_SCRIPT_GETATTR(const char* path, int* result)
{
    redisReply* reply;
    const char* args[] = { "EVALSHA", scripts[SCRIPT_GETATTR], "0", path };

    reply = execRedisCommand2(4, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleStringArrayReply(reply, result);
}


// Redis SCRIPT READDIR:
int redisCommand_SCRIPT_READDIR(const char* path, int* result)
{
    redisReply* reply;
    const char* args[] = { "EVALSHA", scripts[SCRIPT_READDIR], "0", path };

    reply = execRedisCommand2(4, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleStringArrayReply(reply, result);
}


// Redis SCRIPT FILEOPEN:
int redisCommand_SCRIPT_FILEOPEN(const char* path, long long* result)
{
    redisReply* reply;
    const char* args[] = { "EVALSHA", scripts[SCRIPT_FILEOPEN], "0", path };

    reply = execRedisCommand2(4, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleIntegerReply(reply, result);
}


int redisCommand_SCRIPT_FILEIDREADCHUNK(long long nodeId, int offset, int size, char** result, int* len)
{
    redisReply* reply;
    char nodeId_str[64];
    char offset_str[64];
    char size_str[64];
    const char* args[] = { "EVALSHA", scripts[SCRIPT_FILEIDREADCHUNK], "0", nodeId_str, offset_str, size_str };
    snprintf(nodeId_str, sizeof(nodeId_str), "%lld", nodeId);
    snprintf(offset_str, sizeof(offset_str), "%d", offset);
    snprintf(size_str, sizeof(size_str), "%d", size);

    reply = execRedisCommand2(6, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleStringReply(reply, result, len);
}


int redisCommand_SCRIPT_FILEIDWRITECHUNK(long long nodeId, int offset, int size, const char* buf)
{
    redisReply* reply;
    char nodeId_str[64];
    char offset_str[64];
    int handle;
    long long success;
    size_t argvlen[6] = { 0, 0, 0, 0, 0, size };
    int argiszt[6] = { 1, 1, 1, 1, 1, 0 }; // Zero-terminated args.
    const char* args[] = { "EVALSHA", scripts[SCRIPT_FILEIDWRITECHUNK], "0", nodeId_str, offset_str, buf };

    snprintf(nodeId_str, sizeof(nodeId_str), "%lld", nodeId);
    snprintf(offset_str, sizeof(offset_str), "%d", offset);

    reply = execRedisCommand2(6, args, argvlen, argiszt);
    if (!reply)
    {
        return 0; // Failure.
    }

    handle = handleIntegerReply(reply, &success);
    if (!success) {
        return 0; // Failure.
    }
    return handle;
}


// Redis SCRIPT DIRCREATE:
int redisCommand_SCRIPT_DIRCREATE(const char* path, const char* name, long long* result)
{
    redisReply* reply;
    const char* args[] = { "EVALSHA", scripts[SCRIPT_DIRCREATE], "0", path, name };

    reply = execRedisCommand2(5, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleIntegerReply(reply, result);
}


// Redis SCRIPT FILECREATE:
int redisCommand_SCRIPT_FILECREATE(const char* path, const char* name, long long* result)
{
    redisReply* reply;
    const char* args[] = { "EVALSHA", scripts[SCRIPT_FILECREATE], "0", path, name };

    reply = execRedisCommand2(5, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleIntegerReply(reply, result);
}


// Redis SCRIPT FILETRUNCATE:
int redisCommand_SCRIPT_FILETRUNCATE(const char* path, long long length, long long* result)
{
    redisReply* reply;
    char length_str[64];
    const char* args[] = { "EVALSHA", scripts[SCRIPT_FILETRUNCATE], "0", path, length_str };

    snprintf(length_str, sizeof(length_str), "%lld", length);

    reply = execRedisCommand2(5, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleIntegerReply(reply, result);
}


// Redis SCRIPT UNLINK:
int redisCommand_SCRIPT_UNLINK(const char* path, long long* result)
{
    redisReply* reply;
    const char* args[] = { "EVALSHA", scripts[SCRIPT_UNLINK], "0", path };

    reply = execRedisCommand2(4, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleIntegerReply(reply, result);
}


// Redis SCRIPT LINK:
int redisCommand_SCRIPT_LINK(const char* from_path, const char* to_path, long long* result) {
    redisReply* reply;
    const char* args[] = { "EVALSHA", scripts[SCRIPT_LINK], "0", from_path, to_path };

    reply = execRedisCommand2(5, args, NULL, NULL);
    if (!reply)
    {
        return 0; // Failure.
    }

    return handleIntegerReply(reply, result);
}


