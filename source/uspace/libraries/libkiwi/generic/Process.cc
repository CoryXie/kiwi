/* Kiwi process class
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Process class.
 */

#include <kernel/errors.h>
#include <kernel/process.h>

#include <kiwi/Process.h>

#include <stdlib.h>
#include <string.h>

using namespace kiwi;

extern char **environ;

/* FIXME. */
#define PATH_MAX 4096

/** Internal creation function.
 * @param args		NULL-terminated argument array.
 * @param env		NULL-terminated environment variable array.
 * @param inherit	Whether the new process should inherit handles.
 * @param usepath	Whether to use the PATH environment variable. */
void Process::_Init(char **args, char **env, bool inherit, bool usepath) {
	char buf[PATH_MAX];
	const char *path;
	char *cur, *next;
	size_t len;

	if(usepath && !strchr(args[0], '/')) {
		if(!(path = getenv("PATH"))) {
			path = "/system/binaries";
		}

		for(cur = (char *)path; cur; cur = next) {
			if(!(next = strchr(cur, ':'))) {
				next = cur + strlen(cur);
			}

			if(next == cur) {
				buf[0] = '.';
				cur--;
			} else {
				if((next - cur) >= (PATH_MAX - 3)) {
					m_init_status = ERR_PARAM_INVAL;
					return;
				}

				memcpy(buf, cur, (size_t)(next - cur));
			}

			buf[next - cur] = '/';
			len = strlen(args[0]);
			if(len + (next - cur) >= (PATH_MAX - 2)) {
				m_init_status = ERR_PARAM_INVAL;
				return;
			}

			memcpy(&buf[next - cur + 1], args[0], len + 1);

			if((m_handle = process_create(buf, args, (env) ? env : environ, inherit)) >= 0) {
				return;
			} else if(m_handle != -ERR_NOT_FOUND) {
				m_init_status = -m_handle;
				return;
			}

			if(*next == 0) {
				break;
			}
			next++;
		}

		m_init_status = ERR_NOT_FOUND;
		return;
	} else {
		if((m_handle = process_create(args[0], args, (env) ? env : environ, inherit)) < 0) {
			m_init_status = -m_handle;
		}
	}
}

/** Create a new process.
 * @param args		NULL-terminated argument array.
 * @param env		NULL-terminated environment variable array.
 * @param inherit	Whether the new process should inherit handles.
 * @param usepath	Whether to use the PATH environment variable. */
Process::Process(char **args, char **env, bool inherit, bool usepath) : m_init_status(0)  {
	_Init(args, env, inherit, usepath);
}

/** Create a new process.
 * @param cmdline	Command line string.
 * @param env		NULL-terminated environment variable array.
 * @param inherit	Whether the new process should inherit handles.
 * @param usepath	Whether to use the PATH environment variable. */
Process::Process(const char *cmdline, char **env, bool inherit, bool usepath) : m_init_status(0)  {
	char **args = NULL, **tmp, *tok, *dup, *orig;
	size_t count = 0;

	/* Duplicate the command line string so we can modify it. */
	if(!(orig = strdup(cmdline))) {
		m_init_status = ERR_NO_MEMORY;
		return;
	}
	dup = orig;

	/* Loop through each token of the command line and place them into an
	 * array. */
	while((tok = strsep(&dup, " "))) {
		if(!tok[0]) {
			continue;
		}

		if(!(tmp = reinterpret_cast<char **>(realloc(args, (count + 2) * sizeof(char *))))) {
			free(orig);
			free(args);
			m_init_status = ERR_NO_MEMORY;
			return;
		}
		args = tmp;

		/* Duplicating the token is not necessary, and not doing so
		 * makes it easier to handle failure - just free the array and
		 * duplicated string. */
		args[count++] = tok;
		args[count] = NULL;
	}

	if(!count) {
		m_init_status = ERR_PARAM_INVAL;
		return;
	}

	_Init(args, env, inherit, usepath);
	free(args);
	free(orig);
}

/** Open an existing process.
 * @param id		ID of the process to open. */
Process::Process(identifier_t id) : m_init_status(0) {
	if((m_handle = process_open(id)) < 0) {
		m_init_status = -m_handle;
	}
}

/** Check whether initialisation was successful.
 * @param status	Optional pointer to integer to store error code in if
 *			not successful.
 * @return		True if successful, false if not. */
bool Process::Initialised(int *status) const {
	if(m_init_status != 0) {
		if(status) {
			*status = m_init_status;
		}
		return false;
	} else {
		return true;
	}
}

/** Wait for the process to die.
 * @param timeout	Timeout in microseconds.
 * @return		0 on success, error code on failure. */
int Process::WaitTerminate(timeout_t timeout) const {
	return Wait(PROCESS_EVENT_DEATH, timeout);
}

/** Get the ID of the process.
 * @return		ID of the process. */
identifier_t Process::GetID(void) const {
	return process_id(m_handle);
}

/** Get the ID of the current process.
 * @return		ID of the current process. */
identifier_t Process::GetCurrentID(void) {
	return process_id(-1);
}
