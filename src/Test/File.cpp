#include "file.h"

#include <libpsutil.h>

#include <cell/cell_fs.h>

#include <errno.h>

_X_STD_BEGIN
filesystem::filesystem(const char* name, const char* mode)
{
	open(name, mode);
}

filesystem& filesystem::operator<<(const char* buf)
{
	print(buf);
	return *this;
}

filesystem& filesystem::operator<<(int buf)
{
	write(&buf, sizeof(int), 1);
	return *this;
}

filesystem& filesystem::operator<<(const float buf)
{
	write(&buf, sizeof(float), 1);
	return *this;
}

filesystem::operator bool()
{
	return (tell() >= size());
}

bool filesystem::is_open()
{
	return fd != 0;
}

bool filesystem::is_good()
{
	if (tell() >= size())
		return false;
	return true;
}

int filesystem::open(const char* name, const char* mode)
{
	filesystem* file = fsopen(name, mode);
	if (file)
	{
		file->file_size = file->size();
		memcpy(this, file, sizeof(filesystem));

		_sys_free(file);

		return cell_ok;
	}
	else
		return cell_err;
}

int filesystem::write(const void* buf, size_t size, size_t count)
{
	return fswrite(buf, size, count, this);
}

int filesystem::read(void* ptr, size_t size, size_t count)
{
	return fsread(ptr, size, count, this);
}

int filesystem::read_off(int offset, void* ptr, size_t size, size_t count)
{
	return fsread_off(offset, ptr, size, count, this);
}

int filesystem::seek(int offset, int whence)
{
	return fsseek(this, offset, whence);
}

int filesystem::tell()
{
	return fstell(this);
}

bool filesystem::get_line(string& line, char delimeter, int position)
{
	int file_size = size();
	string temp;
	char character;
	while (true)
	{
		if (eof)
		{
			seek(0, SEEK_SET);
			eof = false;
			return false;
		}

		character = read<char>();
		int seek_pos = tell();

		temp += character;
		line = temp;

		if (seek_pos >= file_size)
		{
			eof = true;
			break;
		}

		if (character == delimeter)
			break;
	}

	return true;
}

filesystem& filesystem::getline(string& str)
{
	char ch;
	str.clear();
	while (read(&ch, 1) && ch != '\n')
		str.push_back(ch);
	return *this;
}

int filesystem::print(const char* fmt, ...)
{
	static char buffer[2048];
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsprintf(buffer, fmt, ap);
	va_end(ap);

	fswrite(buffer, strlen(buffer), 1, this);
	return (ret);
}

int filesystem::close()
{
	return fsclose(this);
}

size_t filesystem::size()
{
	CellFsStat stat;
	if ((cellFsStat(name, &stat)) == CELL_FS_SUCCEEDED)
	{
		return static_cast<int64_t>(stat.st_size);
	}

	return 0;
}

_X_STD_END

int __sflags(const char* mode, int* optr)
{
	int ret, m, o;
	switch (*mode++)
	{
	case 'r':	/* open for reading */
		ret = 2;
		m = O_RDONLY;
		o = 0;
		break;
	case 'w':	/* open for writing */
		ret = 1;
		m = O_WRONLY;
		o = O_CREAT | O_TRUNC;
		break;
	case 'a':	/* open for appending */
		ret = 1;
		m = O_WRONLY;
		o = O_CREAT | O_APPEND;
		break;
	case 'B':
		o = O_BE;
		break;
	case 'L':
		o = O_LE;
		break;
	default:	/* illegal mode */
		return (0);
	}
	/* [rwa]\+ or [rwa]b\+ means read and write */
	if (*mode == '+' || (*mode == 'b' && mode[1] == '+'))
	{
		ret = 1;
		m = O_RDWR;
	}
	*optr = m | o;
	return ret;
}

std::filesystem* fsopen(const char* name, const char* mode)
{
	std::filesystem* fp = (std::filesystem*)_sys_malloc(sizeof(std::filesystem));

	printf("%s: %X", __PRETTY_FUNCTION__, fp);
	int f;
	int flags, oflags;
	if ((flags = __sflags(mode, &oflags)) == 0)
		return nullptr;
	if ((cellFsOpen(name, oflags, &fp->fd, 0, 0)) != CELL_FS_SUCCEEDED)
		return nullptr;

	if (oflags & O_APPEND)
	{
		uint64_t offset;
		cellFsLseek(fp->fd, 0, SEEK_END, &offset);
	}
	strcpy(fp->name, name);
	return fp;
}
size_t fswrite(const void* buf, size_t size, size_t count, std::filesystem* fp)
{
	int err = 0;

	if (!fp->fd) return;

	uint64_t file_bytes_written;
	if ((err = cellFsWrite(fp->fd, buf, size * count, &file_bytes_written)) == CELL_FS_SUCCEEDED)
		return file_bytes_written;
	else
		return NULL;
}
size_t fsread(void* ptr, size_t size, size_t count, std::filesystem* stream)
{
	int err = 0;

	if (!stream->fd) return;

	uint64_t file_bytes_read;
	if ((err = cellFsRead(stream->fd, ptr, size * count, &file_bytes_read)) == CELL_FS_SUCCEEDED)
	{
		if (stream->mode & O_BE)
			ptr = libpsutil::memory::byte_swap(ptr);
		stream->offset = fstell(stream);
		return file_bytes_read;
	}
	else
		return NULL;

}
size_t fsread_off(int offset, void* ptr, size_t size, size_t count, std::filesystem* stream)
{
	int err = 0;

	if (!stream->fd) return;

	uint64_t file_bytes_read;
	if ((err = cellFsReadWithOffset(stream->fd, offset, ptr, size * count, &file_bytes_read)) == CELL_FS_SUCCEEDED)
	{
		stream->offset = fstell(stream);
		return file_bytes_read;
	}
	else
		return NULL;

}
int fsseek(std::filesystem* stream, long int offset, int origin)
{
	int err = 0;

	if (!stream->fd) return;

	uint64_t _offset;
	if ((err = cellFsLseek(stream->fd, offset, origin, (uint64_t*)&_offset)) == CELL_FS_SUCCEEDED)
	{
		stream->offset = _offset;
		return 0;
	}
	else
		return -1;
}
int fsprintf(std::filesystem* fp, const char* fmt, ...)
{
	static char buffer[2048];
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsprintf(buffer, fmt, ap);
	va_end(ap);

	fswrite(buffer, strlen(buffer), 1, fp);
	return (ret);
}
int fstell(std::filesystem* stream)
{
	int err = 0;

	if (!stream->fd) return 0;

	uint64_t seek_position;
	if ((err = cellFsLseek(stream->fd, 0, SEEK_CUR, &seek_position)) == CELL_FS_SUCCEEDED)
		return seek_position;
	else
		return -1;
}
int fsclose(std::filesystem* stream)
{
	int err = 0;

	if (!stream->fd) return 0;


	if ((err = cellFsClose(stream->fd)) == CELL_FS_SUCCEEDED)
	{
		return err;
	}
	else
	{
		return 0;
	}
}