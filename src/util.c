/*
 * RediFS
 *
 * Redis File System based on FUSE
 * Copyright (C) 2011 Dave van Soest <dave@thebinarykid.nl>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
*/


/* ---- Includes ---- */
#include <fuse.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "util.h"
#include "options.h"
#include "connection.h"


enum
{
    NODE_INFO_MODE = 0,
};


/* ================ Util functions ================ */

/*
 * Create a new unique node ID.
*/
node_id_t createUniqueNodeId()
{
    node_id_t result = 0;
    char key[1024];
    int redisResult;

    // TODO: Create a list of deleted node IDs and pop one there before creating a new node ID.
    // TODO: Also decrement the node ctr if the latest node ID is deleted.

    snprintf(key, 1024, "%s::%s", g_settings->name, KEY_NODE_ID_CTR);

    redisResult = redisCommand_INCR(key, &result);
    if (!redisResult)
    {
        return -EIO;
    }

    if (result < 1)
    {
        redisResult = redisCommand_SET(key, "-1");
        if (!redisResult)
        {
            return -EIO;
        }

        result = 0;
    }

    return result;
}


/*
 * Retrieve the node ID of the specified path.
*/
node_id_t retrievePathNodeId(const char* path)
{
    char* lpath;
    char* curNodeIdStr;
    node_id_t curNodeId;
    char* curDir;
    char* nextDir;
    char* slash;
    char key[1024];
    int handle;

    if (0 == strcmp(path, "/"))
    {
        return 0; // Root dir node ID.
    }

    lpath = strdup(path);
    curNodeId = 0;
    curDir = lpath + 1;

    while (curDir)
    {
        slash = strchr(curDir, '/');
        if (slash)
        {
            slash[0] = '\0';
            nextDir = slash + 1;
        }
        else
        {
            nextDir = NULL;
        }

        snprintf(key, 1024, "%s::node:%lld", g_settings->name, curNodeId);
        handle = redisCommand_HGET(key, curDir, &curNodeIdStr);
        if (!handle)
        {
            free(lpath);
            return -EIO;
        }

        if (!curNodeIdStr)
        {
            releaseReplyHandle(handle);
            free(lpath);
            return -ENOENT;
        }

        curNodeId = atoll(curNodeIdStr);
        if (curNodeId < 0)
        {
            releaseReplyHandle(handle);
            free(lpath);
            fprintf(stderr, "Error: Invalid node id.\n");
            return -EIO;
        }

        curDir = nextDir;

        releaseReplyHandle(handle);
    }

    free(lpath);

    return curNodeId; // Success.
}


/*
 * Check whether a FS exists on the Redis server.
*/
int checkFileSystemExists()
{
    int result;
    result = retrieveNodeInfo(0, NODE_INFO_MODE);
    return result >= 0 ? 1 : result == -ENOENT ? 0 : result;
}


/*
 * Create a FS on the Redis server.
*/
int createFileSystem()
{
    int redisResult;
    long long args[7];
    char key[1024];
    int result;

    snprintf(key, 1024, "%s::info:0", g_settings->name);

    args[0] = S_IFDIR | 0755;
    args[1] = 0; // TODO: UID.
    args[2] = 0; // TODO: GID.
    args[3] = 1; // TODO: Access time sec.
    args[4] = 1; // TODO: Access time nsec.
    args[5] = 1; // TODO: Modification time sec.
    args[6] = 1; // TODO: Modification time nsec.
    redisResult = redisCommand_RPUSH_INT(key, args, 7, &result);
    if (!redisResult)
    {
        return -EIO;
    }
    else if (result != 1)
    {
        return 0; // Failure.
    }

    return 1; // Success.
}


/*
 * Retrieve node information.
*/
long long retrieveNodeInfo(node_id_t nodeId, int index)
{
    char key[1024];
    char* nodeInfoStr;
    long long nodeInfo;
    int handle;

    snprintf(key, 1024, "%s::info:%d", g_settings->name, index);
    handle = redisCommand_LINDEX(key, index, &nodeInfoStr);
    if (!handle)
    {
        return -EIO;
    }
    else if (!nodeInfoStr)
    {
        releaseReplyHandle(handle);
        return -ENOENT;
    }

    nodeInfo = atoll(nodeInfoStr);

    releaseReplyHandle(handle);

    return nodeInfo;
}

