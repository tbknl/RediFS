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


#endif // _OPERATIONS_H_

