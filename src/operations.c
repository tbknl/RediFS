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
    NODE_INFO_UID,
    NODE_INFO_GID,
    NODE_INFO_ACCESS_TIME_SEC,
    NODE_INFO_ACCESS_TIME_NSEC,
    NODE_INFO_MOD_TIME_SEC,
    NODE_INFO_MOD_TIME_NSEC,
    NODE_INFO_COUNT
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


// ---- mknod:
int redifs_mknod(const char* path, mode_t mode, dev_t dev)
{
    node_id_t nodeId;
    node_id_t parentNodeId;
    long long args[NODE_INFO_COUNT];
    char* newFileName;
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
    args[NODE_INFO_MODE] = mode;
    args[NODE_INFO_UID] = 0; // TODO: UID.
    args[NODE_INFO_GID] = 0; // TODO: GID.
    args[NODE_INFO_ACCESS_TIME_SEC] = 1; // TODO. 
    args[NODE_INFO_ACCESS_TIME_NSEC] = 1; // TODO. 
    args[NODE_INFO_MOD_TIME_SEC] = 1; // TODO. 
    args[NODE_INFO_MOD_TIME_NSEC] = 1; // TODO. 
    redisResult = redisCommand_RPUSH_INT(key, args, NODE_INFO_COUNT, NULL);
    if (!redisResult)
    {
        return -EIO;
    }

    // Determine name of new file:
    lpath = strdup(path);
    newFileName = strdup(basename(lpath));
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
    redisResult = redisCommand_HSET_INT(key, newFileName, nodeId, NULL);

    free(newFileName);

    if (!redisResult)
    {
        return -EIO;
    }

    return 0;
}


/* ---- mkdir ---- */
int redifs_mkdir(const char* path, mode_t mode)
{
    node_id_t nodeId;
    node_id_t parentNodeId;
    long long args[NODE_INFO_COUNT];
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
    args[NODE_INFO_MODE] = mode | S_IFDIR;
    args[NODE_INFO_UID] = 0; // TODO: UID.
    args[NODE_INFO_GID] = 0; // TODO: GID.
    args[NODE_INFO_ACCESS_TIME_SEC] = 1; // TODO. 
    args[NODE_INFO_ACCESS_TIME_NSEC] = 1; // TODO. 
    args[NODE_INFO_MOD_TIME_SEC] = 1; // TODO. 
    args[NODE_INFO_MOD_TIME_NSEC] = 1; // TODO. 
    redisResult = redisCommand_RPUSH_INT(key, args, NODE_INFO_COUNT, NULL);
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


// ---- chmod:
int redifs_chmod(const char* path, mode_t mode)
{
    char key[1024];
    node_id_t nodeId;
    int redisResult;

    // Retrieve the node ID:
    nodeId = retrievePathNodeId(path);
    if (nodeId < 0)
    {
        return -ENOENT;
    }

    // Update node info:
    snprintf(key, 1024, "%s::info:%lld", g_settings->name, nodeId);
    redisResult = redisCommand_LSET_INT(key, NODE_INFO_MODE, mode | S_IFDIR);
    if (!redisResult)
    {
        return -EIO;
    }

    return 0; // Success.
}


// ---- chown:
int redifs_chown(const char* path, uid_t uid, gid_t gid)
{
    char key[1024];
    node_id_t nodeId;
    int redisResult;

    // Retrieve the node ID:
    nodeId = retrievePathNodeId(path);
    if (nodeId < 0)
    {
        return -ENOENT;
    }

    snprintf(key, 1024, "%s::info:%lld", g_settings->name, nodeId);

    // Update UID:
    redisResult = redisCommand_LSET_INT(key, NODE_INFO_UID, uid);
    if (!redisResult)
    {
        return -EIO;
    }

    // Update GID:
    redisResult = redisCommand_LSET_INT(key, NODE_INFO_GID, gid);
    if (!redisResult)
    {
        return -EIO;
    }

    return 0; // Success.
}


// ---- utimens:
int redifs_utimens(const char* path, const struct timespec tv[2])
{
    char key[1024];
    node_id_t nodeId;
    int redisResult;

    // Retrieve the node ID:
    nodeId = retrievePathNodeId(path);
    if (nodeId < 0)
    {
        return -ENOENT;
    }

    snprintf(key, 1024, "%s::info:%lld", g_settings->name, nodeId);

    // Update access time sec:
    redisResult = redisCommand_LSET_INT(key, NODE_INFO_ACCESS_TIME_SEC, tv[0].tv_sec);
    if (!redisResult)
    {
        return -EIO;
    }

    // Update access time usec:
    redisResult = redisCommand_LSET_INT(key, NODE_INFO_ACCESS_TIME_NSEC, tv[0].tv_nsec);
    if (!redisResult)
    {
        return -EIO;
    }

    // Update modification time sec:
    redisResult = redisCommand_LSET_INT(key, NODE_INFO_MOD_TIME_SEC, tv[1].tv_sec);
    if (!redisResult)
    {
        return -EIO;
    }

    // Update modification time usec:
    redisResult = redisCommand_LSET_INT(key, NODE_INFO_MOD_TIME_NSEC, tv[1].tv_nsec);
    if (!redisResult)
    {
        return -EIO;
    }

    return 0; // Success.
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
    .mknod = redifs_mknod,
    .mkdir = redifs_mkdir,
    .readdir = redifs_readdir,
    .chmod = redifs_chmod,
    .chown = redifs_chown,
    .utimens = redifs_utimens,
    .open = redifs_open,
    .read = redifs_read,
};


#endif // _OPERATIONS_H_

