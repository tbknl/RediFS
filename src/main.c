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
#include <stddef.h>
#include <libgen.h>
#include <errno.h>

#include "util.h"
#include "options.h"
#include "connection.h"
#include "operations.h"


// ---- Main function:
int main(int argc, char* argv[])
{
    int result;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct redifs_settings settings = {
        .host = NULL,
        .dir = NULL,
        .port = 0,
        .create_fs = 0,
        .name = DEFAULT_NAME,
    };

    // Parse command line options:
    if (-1 == fuse_opt_parse(&args, &settings, redifs_opts, redifs_opts_proc))
    {
        exit(1);
    }

    // Set global settings:
    g_settings = &settings;

    // Connect to Redis:
    if (-1 == openRedisConnection(settings.host, settings.port))
    {
        fprintf(stderr, "Error: Cannot connect to Redis server.\n");
        exit(1);
    }

	// TODO: Handle filesystem name as cli argument. Set default FS name.

	/* TODO
    // Check whether a FS exists:
    result = checkFileSystemExists();
    if (result == 0)
    {
        if (settings.create_fs)
        {
            fprintf(stderr,
                "No file system named '%s' exists on the Redis server.\n"
                "Creating new file system '%s'.\n", g_settings->name, g_settings->name
            );

            result = createFileSystem();
            if (result == 0)
            {
                fprintf(stderr, "Error: Could not create file system.\n");
                exit(1);
            }
            else if (result < 0)
            {
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "Error: No file system named '%s' exists on the Redis server.\n", g_settings->name);
            exit(1);
        }
    }
    else if (result == -EIO)
    {
        fprintf(stderr, "Error: I/O error occurred.\n");
        exit(1);
    }
    else if (result < 1)
    {
        fprintf(stderr, "Error: Unexpected error occurred: %d.\n", -result);
        exit(1);
    }
	*/

	// Load the scripts table:
	if (0 > loadScripts()) {
        fprintf(stderr, "Error: Could not load scripts.\n");
		exit(1);
	}
	//exit(2);

    // Start FUSE main:
    result = fuse_main(args.argc, args.argv, &redifs_oper, NULL);

    // Close Redis connection:
    closeRedisConnection();

    // Clean up FUSE stuff:
    fuse_opt_free_args(&args);
    if (settings.host) free(settings.host);

    return result;
}

