#include "io_directory.h"
#include "io_path.h"

AVES_API NATIVE_FUNCTION(io_Directory_existsInternal)
{
	String *pathName = args[0].common.string;
	Path::ValidatePath(thread, pathName, false);

	WIN32_FILE_ATTRIBUTE_DATA data;

	bool r;
	{ Pinned pn(args + 0);
		r = io::ReadFileAttributes(thread, pathName, &data, false);
	}

	if (r)
		r = data.dwFileAttributes != -1 &&
			((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);

	VM_PushBool(thread, r);
}