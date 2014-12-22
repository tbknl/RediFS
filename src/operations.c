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
#include <unistd.h>
#include <sys/types.h>
#include <linux/limits.h>

#include "operations.h"
#include "options.h"
#include "util.h"
#include "connection.h"


/* ---- Defines ---- */

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
    memset(stbuf, 0, sizeof(struct stat));
    /*
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    //stbuf->st_size = 4096;
    return 0;
    //*/

    int handle;
    int numResults;

    mode_t mode;
    off_t size;
    const char* type;

    handle = redisCommand_SCRIPT_GETATTR(path, &numResults);
    if (!handle)
    {
        return -EIO;
    }

    if (numResults == 3) {
        char* entries[3];
        retrieveStringArrayElements(handle, 0, numResults, entries);
        type = entries[0];
        mode = atoll(entries[1]);
        size = atoll(entries[2]);
        mode = 0755;
    }
    else {
		// TODO!!! Can also be ENOTDIR.
		return -ENOENT;
    }

    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();

    if (0 == strcmp(type, "dir"))
    {
        stbuf->st_mode = mode | S_IFDIR;
        stbuf->st_nlink = 2;
    }
    else
    {
        stbuf->st_mode = mode | S_IFREG;
        stbuf->st_nlink = 1;
        stbuf->st_size = size;
    }

    releaseReplyHandle(handle);

    return 0;
}


// ---- mknod:
int redifs_mknod(const char* path, mode_t mode, dev_t dev)
{
	// TODO: Check mode. For now it is assumed it is a regular file.

	char pathdup1[PATH_MAX];
	char pathdup2[PATH_MAX];
	char* parent_path;
	char* name;
    long long result;
    int handle;

	strcpy(pathdup1, path);
	strcpy(pathdup2, path);
	parent_path = dirname(pathdup1);
	name = basename(pathdup2);

    handle = redisCommand_SCRIPT_FILECREATE(parent_path, name, &result);
    if (!handle)
    {
        return -EIO;
    }

    if (result <= 0) {
        return -EIO; // Unexpected result.
    }

    releaseReplyHandle(handle);
	return 0;

	// TODO?
    /*args[NODE_INFO_MODE] = mode | S_IFDIR;*/
    /*args[NODE_INFO_UID] = 0; // TODO: UID.*/
    /*args[NODE_INFO_GID] = 0; // TODO: GID.*/
    /*args[NODE_INFO_ACCESS_TIME_SEC] = 1; // TODO. */
    /*args[NODE_INFO_ACCESS_TIME_NSEC] = 1; // TODO. */
    /*args[NODE_INFO_MOD_TIME_SEC] = 1; // TODO. */
    /*args[NODE_INFO_MOD_TIME_NSEC] = 1; // TODO. */
}


/* ---- mkdir ---- */
int redifs_mkdir(const char* path, mode_t mode)
{
	char pathdup1[PATH_MAX];
	char pathdup2[PATH_MAX];
	char* parent_path;
	char* name;
    long long result;
    int handle;

	strcpy(pathdup1, path);
	strcpy(pathdup2, path);
	parent_path = dirname(pathdup1);
	name = basename(pathdup2);

    handle = redisCommand_SCRIPT_DIRCREATE(parent_path, name, &result);
    if (!handle)
    {
        return -EIO;
    }

    if (result <= 0) {
        return -EIO; // Unexpected result.
    }

    releaseReplyHandle(handle);
	return 0;

	// TODO?
    /*args[NODE_INFO_MODE] = mode | S_IFDIR;*/
    /*args[NODE_INFO_UID] = 0; // TODO: UID.*/
    /*args[NODE_INFO_GID] = 0; // TODO: GID.*/
    /*args[NODE_INFO_ACCESS_TIME_SEC] = 1; // TODO. */
    /*args[NODE_INFO_ACCESS_TIME_NSEC] = 1; // TODO. */
    /*args[NODE_INFO_MOD_TIME_SEC] = 1; // TODO. */
    /*args[NODE_INFO_MOD_TIME_NSEC] = 1; // TODO. */
}


/* ---- readdir ---- */
int redifs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info* fileInfo)
{
    int result;
    int handle;
    int i;

    handle = redisCommand_SCRIPT_READDIR(path, &result);
    if (!handle)
    {
        return -EIO;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    fprintf(stderr, "readdir result count %d\n", result);
    if (result > 0)
    {
        char* entries[result];
        retrieveStringArrayElements(handle, 0, result, entries);
        for (i = 0; i < result; ++i)
        {
            filler(buf, entries[i], NULL, 0);
            fprintf(stderr, "readdir result %d %s\n", i, entries[i]);
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
    /*
    if (0 != strcmp(path, "/bla"))
    {
        return -ENOENT;
    }

    if ((fileInfo->flags & 3) != O_RDONLY)
    {
        return -EACCES;
    }

    return 0;
    //*/

    long long result;
    int handle;

    handle = redisCommand_SCRIPT_FILEOPEN(path, &result);
    if (!handle)
    {
        return -EIO;
    }

    if (result < 0) {
        return result; // Returned error.
    }
    else if (result == 0) {
        return -EIO; // Unexpected result.
    }

    // NOTE: A result > 0 indicates a file node id.
    
    fileInfo->fh = result;

    releaseReplyHandle(handle);

    return 0;
}


/* ---- read ---- */
int redifs_read(const char* path, char* buf, size_t size, off_t offset,
                       struct fuse_file_info* fileInfo)
{
    long long nodeId = fileInfo->fh;
    char* result;
    int len;
    int handle;

    handle = redisCommand_SCRIPT_FILEIDREADCHUNK(nodeId, offset, size, &result, &len);
    if (!handle)
    {
        return -EIO;
    }

    memcpy(buf, result, len);

    releaseReplyHandle(handle);

    return len;
}


/* ---- write ---- */
int redifs_write(const char* path, const char* buf, size_t size, off_t offset,
                       struct fuse_file_info* fileInfo)
{
    long long nodeId = fileInfo->fh;
    int handle;

    handle = redisCommand_SCRIPT_FILEIDWRITECHUNK(nodeId, offset, size, buf);
    if (!handle)
    {
        return -EIO;
    }

    releaseReplyHandle(handle);

    return size;
}


/* ---- redifs fuse operations ---- */
struct fuse_operations redifs_oper = {
    .getattr = redifs_getattr,
	.mknod = redifs_mknod,
    .mkdir = redifs_mkdir,
    .readdir = redifs_readdir,
    //.chmod = redifs_chmod,
    //.chown = redifs_chown,
    //.utimens = redifs_utimens,
    .open = redifs_open,
    .read = redifs_read,
    .write = redifs_write,
};


#endif // _OPERATIONS_H_

