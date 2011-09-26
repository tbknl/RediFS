/*
 * RediFS
 *
 * Redis File System based on FUSE
 * Copyright (C) 2011 Dave van Soest <dave@thebinarykid.nl>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
*/


#ifndef _UTIL_H_
#define _UTIL_H_

#include "redifs_types.h"


/* ---- Defines ---- */
#define KEY_NODE_ID_CTR "node_id_ctr"


/* ================ Util functions ================ */

extern node_id_t createUniqueNodeId();
extern node_id_t retrievePathNodeId(const char* path);
extern int checkFileSystemExists();
extern int createFileSystem();
extern long long retrieveNodeInfo(node_id_t nodeId, int index);


#endif // _UTIL_H_

