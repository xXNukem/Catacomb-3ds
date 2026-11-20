#include <dirent.h>
#include "be_filesystem_dir.h"
#include <stddef.h> // NULL is also needed when compiling the unified C++ engine

#ifdef __3DS__
#include "be_filesystem_mkdir.h"
#include "be_filesystem_len_bounds.h"
#include <3ds.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void BEL_Cross_mkdir(const TCHAR *path)
{
	if (BEL_Cross_IsDir(path) || mkdir(path, 0755) == 0)
		return;
	const int err = errno;
	if (err == EEXIST && BEL_Cross_IsDir(path))
		return;

	char message[BE_CROSS_PATH_LEN_BOUND + 96];
	snprintf(message, sizeof(message), "Cannot prepare directory %s: %s",
	         path, strerror(err));
	fprintf(stderr, "%s\n", message);
	errorConf dialog;
	errorInit(&dialog, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
	errorText(&dialog, message);
	errorDisp(&dialog);
	exit(EXIT_FAILURE);
}
#endif

BE_DIR_T BEL_Cross_OpenDir(const char *dirname) { return opendir(dirname); }

char *BEL_Cross_ReadDir(BE_DIR_T dir)
{
	struct dirent *direntry = readdir((DIR *)dir);
	return direntry ? direntry->d_name : NULL;
}

void BEL_Cross_RewindDir(BE_DIR_T dir) { rewinddir((DIR *)dir); }

void BEL_Cross_CloseDir(BE_DIR_T dir) { closedir((DIR *)dir); }
