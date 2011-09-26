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


/* ---- redifs fuse operations ---- */
extern struct fuse_operations redifs_oper;


/* ---- Defines ---- */
#define KEY_NODE_ID_CTR "node_id_ctr"


/* --- Prototypes ---- */
extern int redifs_getattr(const char* path, struct stat* stbuf);
extern int redifs_mkdir(const char* path, mode_t mode);
extern int redifs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info* fileInfo);
extern int redifs_open(const char* path, struct fuse_file_info* fileInfo);
extern int redifs_read(const char* path, char* buf, size_t size, off_t offset,
                       struct fuse_file_info* fileInfo);


#endif // _OPERATIONS_H_

