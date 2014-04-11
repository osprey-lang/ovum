#include "io_directory.h"
#include "io_path.h"

AVES_API BEGIN_NATIVE_FUNCTION(io_Directory_existsInternal)
{
	String *pathName = args[0].common.string;
	CHECKED(Path::ValidatePath(thread, pathName, false));

	WIN32_FILE_ATTRIBUTE_DATA data;

	bool result;
	{ Pinned pn(args + 0);
		CHECKED(io::ReadFileAttributes(thread, pathName, &data, false, result));
	}

	if (result)
		result = data.dwFileAttributes != -1 &&
			((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);

	VM_PushBool(thread, result);
}
END_NATIVE_FUNCTION