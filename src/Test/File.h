#pragma once

#include <string>
#include <fstream>

#undef fopen
#undef fwrite
#undef fread
#undef fseek
#undef fprintf
#undef ftell
#undef fclose

#define O_CREAT         CELL_FS_O_CREAT
#define O_EXCL          CELL_FS_O_EXCL
#define O_TRUNC         CELL_FS_O_TRUNC
#define O_APPEND        CELL_FS_O_APPEND
#define O_ACCMODE       CELL_FS_O_ACCMODE
#define O_RDONLY        CELL_FS_O_RDONLY
#define O_RDWR          CELL_FS_O_RDWR
#define O_WRONLY        CELL_FS_O_WRONLY
#define O_BE			0x3000
#define O_LE			0x2000

enum cell_fs_error
{
	cell_err,
	cell_ok,
};

_STD_BEGIN

// cell fs wrapper
class filesystem
{
public:
	filesystem() = default;
	filesystem(const char* name, const char* mode);

	// operators
	filesystem& operator<<(const char* buf);
	filesystem& operator<<(int buf);
	filesystem& operator<<(const float buf);

	operator bool();

	bool is_open();
	bool is_good();

	// open functions
	int open(const char* name, const char* mode);

	// write functions
	int write(const void* buf, size_t size, size_t count = 1);

	// read functions
	int read(void* ptr, size_t size, size_t count = 1);

	template<class T> int read(T* ptr, size_t count = 1)
	{
		return fsread(ptr, sizeof(T), count, this);
	}

	template<class T> T read()
	{
		T temp;
		fsread(&temp, sizeof(T), 1, this);

		return temp;
	}

	// read with offset functions
	int read_off(int offset, void* ptr, size_t size, size_t count = 1);

	// write string function
	int print(const char* fmt, ...);

	bool get_line(string& line, char delimeter = '\n', int position = 0);
	filesystem& getline(string& str);

	int seek(int offset, int whence = SEEK_CUR);
	int tell();
	int close();

	size_t size();

	char name[100];
	int fd;
	int offset;
	bool eof;
	unsigned int mode;
	unsigned int file_size;
};

_STD_END

int __sflags(const char* mode, int* optr);
std::filesystem* fsopen(const char* name, const char* mode);
size_t fswrite(const void* buf, size_t size, size_t count, std::filesystem* fp);
size_t fsread(void* ptr, size_t size, size_t count, std::filesystem* stream);
size_t fsread_off(int offset, void* ptr, size_t size, size_t count, std::filesystem* stream);
int fsseek(std::filesystem* stream, long int offset, int origin);
int fsprintf(std::filesystem* fp, const char* fmt, ...);
int fstell(std::filesystem* stream);
int fsclose(std::filesystem* stream);
