/*
 * RediFS
 *
 * Redis File System based on FUSE
 * Copyright (C) 2011 Dave van Soest <dave@thebinarykid.nl>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
*/


#ifndef _OPTIONS__H_
#define _OPTIONS__H_


/* ---- Includes ---- */
#include <fuse/fuse_opt.h>

#include "redifs_types.h"


/* ---- Defines ---- */
#define DEFAULT_DIR ""
#define DEFAULT_NAME "redifs1"


/* ================ Options ================ */

struct redifs_settings {
    char* host;
    char* dir;
    redifs_port_t port;
    int create_fs;
    char* name;
};

extern struct redifs_settings* g_settings;


extern struct fuse_opt redifs_opts[];


extern int redifs_opts_proc(void* data, const char* arg, int key, struct fuse_args* outargs);


#endif // _OPTIONS__H_

