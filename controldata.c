/*
 * controldata.c
 * Copyright (c) 2ndQuadrant, 2010-2017
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 */

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "postgres_fe.h"

#include "repmgr.h"
#include "controldata.h"

static ControlFileInfo *get_controlfile(char *DataDir);

DBState
get_db_state(char *data_directory)
{
	ControlFileInfo *control_file_info;
	DBState state;

	control_file_info = get_controlfile(data_directory);

	if (control_file_info->control_file_processed == true)
		state = control_file_info->control_file->state;
	else
		/* if we were unable to parse the control file, assume DB is shut down */
		state = DB_SHUTDOWNED;

	pfree(control_file_info->control_file);
	pfree(control_file_info);
	return state;
}

const char *
describe_db_state(DBState state)
{
	switch (state)
	{
		case DB_STARTUP:
			return _("starting up");
		case DB_SHUTDOWNED:
			return _("shut down");
		case DB_SHUTDOWNED_IN_RECOVERY:
			return _("shut down in recovery");
		case DB_SHUTDOWNING:
			return _("shutting down");
		case DB_IN_CRASH_RECOVERY:
			return _("in crash recovery");
		case DB_IN_ARCHIVE_RECOVERY:
			return _("in archive recovery");
		case DB_IN_PRODUCTION:
			return _("in production");
	}
	return _("unrecognized status code");
}


/*
 * we maintain our own version of get_controlfile() as we need cross-version
 * compatibility, and also don't care if the file isn't readable.
 */
static ControlFileInfo *
get_controlfile(char *DataDir)
{
	ControlFileInfo *control_file_info;
	int			fd;
	char		ControlFilePath[MAXPGPATH];

	control_file_info = palloc0(sizeof(ControlFileInfo));
	control_file_info->control_file_processed = false;
	control_file_info->control_file = palloc0(sizeof(ControlFileData));

	snprintf(ControlFilePath, MAXPGPATH, "%s/global/pg_control", DataDir);

	if ((fd = open(ControlFilePath, O_RDONLY | PG_BINARY, 0)) == -1)
	{
		log_debug("could not open file \"%s\" for reading: %s",
				  ControlFilePath, strerror(errno));
		return control_file_info;
	}

	if (read(fd, control_file_info->control_file, sizeof(ControlFileData)) != sizeof(ControlFileData))
	{
		log_debug("could not read file \"%s\": %s",
				  ControlFilePath, strerror(errno));
		return control_file_info;
	}

	close(fd);

	control_file_info->control_file_processed = true;

	/*
	 * We don't check the CRC here as we're potentially checking a pg_control
	 * file from a different PostgreSQL version to the one repmgr was compiled
	 * against. However we're only interested in the first few fields, which
	 * should be constant across supported versions
	 *
	 * XXX double-check this
	 */

	return control_file_info;
}