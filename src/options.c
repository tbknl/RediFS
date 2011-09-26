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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"


/* ================ Usage ================ */

static void usage(const char* progName)
{
    printf(
        "usage: %s mountpoint [[host]:[dir]] [port] [options]\n"
        "\n", progName
    );
}


/* ================ Options ================ */

struct redifs_settings* g_settings;


enum {
    KEY_HELP,
    KEY_CREATE_FS,
    KEY_FS_NAME,
};


struct fuse_opt redifs_opts[] = {
    FUSE_OPT_KEY("-N", KEY_FS_NAME),
    FUSE_OPT_KEY("-C", KEY_CREATE_FS),
    FUSE_OPT_KEY("-h", KEY_HELP),
    FUSE_OPT_KEY("--help", KEY_HELP),
    FUSE_OPT_END
};


int redifs_opts_proc(void* data, const char* arg, int key, struct fuse_args* outargs)
{
    struct redifs_settings* settings = (struct redifs_settings*)data;

    switch (key) {
        case FUSE_OPT_KEY_OPT:
            return 1;

        case FUSE_OPT_KEY_NONOPT:
            if (!settings->host && !settings->dir && strchr(arg, ':'))
            {
                settings->host = strdup(arg);
                char* colon = strchr(settings->host, ':');
                settings->dir = colon + 1;
                *colon = '\0';
                return 0;
            }
            else if (!settings->port && atoi(arg) > 0)
            {
                settings->port = (redifs_port_t)atoi(arg);
                return 0;
            }

            return 1;

        case KEY_HELP:
            usage(outargs->argv[0]);
            exit(1);

        case KEY_CREATE_FS:
            settings->create_fs = 1;
            return 0;

        case KEY_FS_NAME:
            // TODO
            return 0;

        default:
            fprintf(stderr, "internal error\n");
            abort();
    }
}

