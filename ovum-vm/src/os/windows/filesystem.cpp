#include "filesystem.h"

namespace ovum
{

namespace os
{

	FileStatus _FileStatusFromError(DWORD error)
	{
		switch (error)
		{
		case ERROR_HANDLE_EOF:
			return FILE_EOF;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			return FILE_NOT_FOUND;
		case ERROR_ACCESS_DENIED:
			return FILE_ACCESS_DENIED;
		case ERROR_FILE_EXISTS:
		case ERROR_ALREADY_EXISTS:
			return FILE_ALREADY_EXISTS;
		default:
			return FILE_IO_ERROR;
		}
	}

} // namespace os

} // namespace ovum