/*
 * RediFS
 *
 * Redis File System based on FUSE
 * Copyright (C) 2011 Dave van Soest <dave@thebinarykid.nl>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
*/


#ifndef _OPERATIONS_H_
#define _OPERATIONS_H_


/* ---- Includes ---- */
#include <fuse.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <libgen.h>
#include <errno.h>

#include "operations.h"
#include "options.h"
#include "util.h"
#include "connection.h"


/* ---- Defines ---- */
#define KEY_NODE_ID_CTR "node_id_ctr"

enum
{
    NODE_INFO_MODE = 0,
};


/* ---- Macros ---- */
#define CLEAR_STRUCT(ptr, type) memset(ptr, 0, sizeof(type));


/* ================ FUSE operations ================ */

/* ---- getattr ---- */
int redifs_getattr(const char* path, struct stat* stbuf)
{
    char* lpath;
    char* dirName;
    node_id_t nodeId;
    long long mode;

    lpath = strdup(path);
    dirName = strdup(basename(lpath));
    free(lpath);

    CLEAR_STRUCT(stbuf, struct stat);

    nodeId = retrievePathNodeId(path);
    if (nodeId < 0)
    {
        return -ENOENT;
    }

    mode = retrieveNodeInfo(nodeId, NODE_INFO_MODE);
    if (mode < 0)
    {
        return mode;
    }

    if (mode & S_IFDIR)
    {
        stbuf->st_mode = mode;
        stbuf->st_nlink = 2;
    }
    else
    {
        stbuf->st_mode = mode;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen("blablabla");
    }

    return 0;
}


/* ---- mkdir ---- */
int redifs_mkdir(const char* path, mode_t mode)
{
    node_id_t nodeId;
    node_id_t parentNodeId;
    char* newDirName;
    char key[1024];
    char* lpath;
    int redisResult;

    // Create a new node ID:
    nodeId = createUniqueNodeId();
    if (nodeId == 0)
    {
        fprintf(stderr, "Error: Run out of node IDs.\n");
        return -ENOSPC;
    }
    else if (nodeId < 0)
    {
        return nodeId;
    }

    // Create node info:
    snprintf(key, 1024, "%s::info:%lld", g_settings->name, nodeId);
    redisResult = redisCommand_RPUSH_INT(key, mode | S_IFDIR, NULL);
    if (!redisResult)
    {
        return -EIO;
    }

    // Determine name of new directory:
    lpath = strdup(path);
    newDirName = strdup(basename(lpath));
    free(lpath);

    // Determine parent dir node ID:
    lpath = strdup(path);
    parentNodeId = retrievePathNodeId(dirname(lpath));
    free(lpath);
    if (parentNodeId < 0)
    {
        return -ENOENT;
    }

    // Execute Redis command:
    snprintf(key, 1024, "%s::node:%lld", g_settings->name, parentNodeId);
    redisResult = redisCommand_HSET_INT(key, newDirName, nodeId, NULL);

    free(newDirName);

    if (!redisResult)
    {
        return -EIO;
    }

    return 0;
}


/* ---- readdir ---- */
int redifs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info* fileInfo)
{
    char key[1024];
    node_id_t nodeId;
    int result;
    int handle;
    int i;

    // TODO: Make Redis key safe (remove space and newline chars).

    // Determine dir node ID:
    nodeId = retrievePathNodeId(path);
    if (nodeId < 0)
    {
        return -ENOENT;
    }

    snprintf(key, 1024, "%s::node:%lld", g_settings->name, nodeId);

    handle = redisCommand_HKEYS(key, &result);
    if (!handle)
    {
        return -EIO;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (result > 0)
    {
        char* entries[result];
        retrieveStringArrayElements(handle, 0, result, entries);
        for (i = 0; i < result; ++i)
        {
            filler(buf, entries[i], NULL, 0);
        }
    }

    releaseReplyHandle(handle);

    return 0;
}


/* ---- open ---- */
int redifs_open(const char* path, struct fuse_file_info* fileInfo)
{
    if (0 != strcmp(path, "/bla"))
    {
        return -ENOENT;
    }

    if ((fileInfo->flags & 3) != O_RDONLY)
    {
        return -EACCES;
    }

    return 0;
}


/* ---- read ---- */
int redifs_read(const char* path, char* buf, size_t size, off_t offset,
                       struct fuse_file_info* fileInfo)
{
    size_t len;

    if (0 != strcmp(path, "/bla"))
    {
        return -ENOENT;
    }

    len = strlen("blablabla");
    if (offset < len)
    {
        if (offset + size > len) size = len - offset;
        memcpy(buf, "blablabla" + offset, size);
    }
    else
    {
        size = 0;
    }

    return size;
}


/* ---- redifs fuse operations ---- */
struct fuse_operations redifs_oper = {
    .getattr = redifs_getattr,
    .mkdir = redifs_mkdir,
    .readdir = redifs_readdir,
    .open = redifs_open,
    .read = redifs_read,
};


#endif // _OPERATIONS_H_

