#include "port_filesystem.h"
#include "../engine/CommandLineParser.h"
#include "Settings.h"
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
using namespace std;
/*
#ifndef DIR
#define DIR __dirstream_t *
#endif
*/

//char gamepath[512] = "c:\\prenos\\Magic2\\mc2-orig-copy";
char gameFolder[512] = "NETHERW";
char cdFolder[512] = "CD_Files";
char bigGraphicsFolder[512] = "bigGraphics";

#ifndef _MSC_VER
	#include <libgen.h>
	#include <iostream>
	#include <unistd.h>
	#include <stdarg.h>
	#include <sys/stat.h>
    #include <cstdio>
#endif

#ifdef _MSC_VER
std::string utf8_encode(const std::wstring &wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}
#endif

bool firstrun = true;

std::string get_exe_path() {
#ifdef _MSC_VER
	LPWSTR buffer = new WCHAR[MAX_PATH];
	GetModuleFileNameW(NULL, buffer, MAX_PATH);
	std::string locstr = utf8_encode(buffer);
	delete[] buffer;
	std::string::size_type pos = std::string(locstr).find_last_of("\\/");
	std::string strpathx = std::string(locstr).substr(0, pos)/*+"\\system.exe"*/;
	return strpathx;
#else
	std::string strpathx;
	char result[ PATH_MAX ];
	ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
	if (count != -1) {
		result[count] = '\0';
		strpathx = dirname(result);
	}
	return strpathx;
#endif
};

bool file_exists(const char * filename) {
	/*if (FILE * file = fcaseopen(filename, "r")) {
		fclose(file);
		return true;
	}
	return false;*/
	FILE* file;
	if ((file = fcaseopen(filename, "r")) != NULL) {
		fclose(file);
		#ifdef DEBUG_START
				debug_printf("file_exists:true-%s\n", filename);
		#endif //DEBUG_START
		return true;
	}
	#ifdef DEBUG_START
		debug_printf("file_exists:false-%s\n", filename);
	#endif //DEBUG_START
	return false;
}

FILE* mycreate(const char* path, uint32_t  /*flags*/) {
	FILE *fp;
	fp = fcaseopen(path, "wb+");
	#ifdef DEBUG_START
		debug_printf("mycreate:%p\n",fp);
	#endif //DEBUG_START
	return fp;
};

FILE* debug_output = nullptr;

// Only active with --debug-log (or "debug": {"log": true}); written next to the executable,
// truncated once per run and flushed per line so it survives crashes.
void debug_printf(const char* format, ...) {
	if (!g_settings.debugLog)
		return;
	char prbuffer[1024];
	va_list arg;
	va_start(arg, format);
	vsnprintf(prbuffer, sizeof(prbuffer), format, arg);
	va_end(arg);

	if (!debug_output)
	{
		std::string path = get_exe_path() + "/debug.txt";
		debug_output = fopen(path.c_str(), "wt");
		if (!debug_output)
			return;
	}
	fputs(prbuffer, debug_output);
	fflush(debug_output);
	#ifdef DEBUG_PRINT_DEBUG_TO_SCREEN
		printf("%s", prbuffer);
	#endif
}

int32_t myaccess(const char* path, uint32_t  /*flags*/) {
	DIR *dir;
	//char path2[2048] = "\0";
	#ifdef DEBUG_FILEOPS
		debug_printf("myaccess:orig path:%s\n", path);
	#endif //DEBUG_FILEOPS
	//pathfix(path, path2);//only for DOSBOX version
	//#ifdef DEBUG_FILEOPS
	//	debug_printf("myaccess:fix path:%s\n", path2);
	//#endif //DEBUG_FILEOPS
	dir = opendir(path);
	#ifdef DEBUG_FILEOPS
		debug_printf("myaccess:exit:%p %d\n", dir, errno);
	#endif //DEBUG_FILEOPS
	if (dir)
	{
		/* Directory exists. */
		closedir(dir);
		return 1;
	}
	else if (ENOENT == errno)
	{
		return -1;
	}
	return -1;
};

int32_t /*__cdecl*/ mymkdir(const char* path) {
	//char path2[512] = "\0";
	#ifdef DEBUG_FILEOPS
		debug_printf("mymkdir:path: %s\n", path);
	#endif //DEBUG_FILEOPS
	//pathfix(path, path2);//only for DOSBOX version

	//#ifdef DEBUG_FILEOPS
	//	debug_printf("mymkdir:path2: %s\n", path2);
	//#endif //DEBUG_FILEOPS

#ifdef WIN32
	const WCHAR *pwcsName;
	// required size
	int nChars = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
	// allocate it
	pwcsName = new WCHAR[nChars];
	MultiByteToWideChar(CP_ACP, 0, path, -1, (LPWSTR)pwcsName, nChars);
	// use it....

#ifdef DEBUG_FILEOPS
	debug_printf("mymkdir:path3: %s\n", pwcsName);
#endif //DEBUG_FILEOPS
#endif



	int result;
#if defined (WIN32)						/* MS Visual C++ */
	result = _wmkdir(pwcsName);
	delete[] pwcsName;
#else
	result = mkdir(path, 0700);
#endif
	// delete it

#ifdef DEBUG_FILEOPS
	debug_printf("mymkdir:end: %d\n", result);
#endif //DEBUG_FILEOPS
	return result;
};

FILE* myopen(const char* path, int pmode, uint32_t flags) {
	#ifdef DEBUG_START
		debug_printf("myopen:open file:%s\n", path);
	#endif //DEBUG_START
	//bool localDrive::FileOpen(DOS_File * * file, const char * name, uint32_t flags) {
	const char * type;
	if ((pmode == 0x222) && (flags == 0x40))type = "rb+";
	else if ((pmode == 0x200) && (flags == 0x40))type = "rb"; // O_BINARY|O_RDONLY: data may be read-only
	else if (pmode == 0x202) type = "rb+";
	else
		exit(1);//error - DOSSetError(DOSERR_ACCESS_CODE_INVALID);
	FILE* fp = nullptr;
	//char path2[512] = "\0";
	//pathfix(path, path2);//only for DOSBOX version
	//#ifdef DEBUG_START
	//	debug_printf("myopen:open file:fixed:%s\n", path2);
	//#endif //DEBUG_START
	//if(file_exists(path2))

	fp= fcaseopen(path, type);
	#ifdef DEBUG_START
		debug_printf("myopen:open end %p\n", fp);
	#endif //DEBUG_START
	return fp;
};
int myclose(FILE* descriptor) {
	return fclose(descriptor);
};
int32_t mylseek(FILE* filedesc, x_DWORD position, char type) {
	return fseek(filedesc, position, type);
};

int32_t myfseek(FILE* filedesc, x_DWORD position, char type) {
	return fseek(filedesc, position, type);
};

long myftell(FILE* decriptor) {
	return ftell(decriptor);
};

int DirExists(const char* path)
{
	struct stat info;

	if (stat(path, &info) != 0)
		return 0;
	else if (info.st_mode & S_IFDIR)
		return 1;
	else
		return -1;
}

FILE* myopent(char* path, char* type) {
	FILE *fp;
	fp= fcaseopen(path, type);
	#ifdef DEBUG_FILEOPS
		debug_printf("myopent:end: %p\n", fp);
	#endif //DEBUG_FILEOPS
	return fp;
};

dirsstruct getListDir(char* dirname)
{
	struct dirent *de;  // Pointer for directory entry 
	dirsstruct directories;
	directories.number = 0;
	// opendir() returns a pointer of DIR type.  
	DIR *dr = opendir(dirname);
	if (dr == NULL)  // opendir returns NULL if couldn't open directory 
	{
		printf("Could not open current directory1 %s\n", dirname);
		return directories;
	}
	// Refer http://pubs.opengroup.org/onlinepubs/7990989775/xsh/readdir.html 
	// for readdir() 
	while ((de = readdir(dr)) != NULL)
	{
		if(de->d_name[0]!='.')
			sprintf(directories.dir[directories.number++],"%s", de->d_name);
		//printf("%s\n", de->d_name);		
	}
	closedir(dr);
	return directories;
}

void FixDir(char* outdirname, char* indirname) {
	std::string pathexe = get_exe_path();
	sprintf(outdirname, "%s/%s", pathexe.c_str(), indirname);
};
/*

dirsstruct getListDirFix(char* indirname)
{
	struct dirent *de;  // Pointer for directory entry 
	dirsstruct directories;
	directories.number = 0;
	// opendir() returns a pointer of DIR type.  
	char path2[512] = "\0";
	char pathexe[512] = "\0";
	get_exe_path(pathexe);
	sprintf(path2,"%s/%s", pathexe,indirname);

	DIR *dr = opendir(path2);
	if (dr == NULL)  // opendir returns NULL if couldn't open directory 
	{
		printf("Could not open current directory2 %s\n", path2);
		return directories;
	}
	// Refer http://pubs.opengroup.org/onlinepubs/7990989775/xsh/readdir.html 
	// for readdir() 
	while ((de = readdir(dr)) != NULL)
	{
		if (de->d_name[0] != '.')
			sprintf(directories.dir[directories.number++], "%s", de->d_name);
		//printf("%s\n", de->d_name);		
	}
	closedir(dr);
	return directories;
}
*/
int dos_getdrive(int* a) {
#ifdef WIN32
	*a = _getdrive();
#else
	*a = 0;
	std::cerr << "STUB: dos_getdrive on Linux" << std::endl;
#endif
	return *a;
};

// DOS _dos_getdiskfree: 0 = success. The game's drives are folders on the host, assumed to have room
// (the old Windows-only version decided on an uninitialised error code).
uint64_t dos_getdiskfree(uint8_t a, int16_t a2) {
	return 0;
}

void AdvReadfile(const char* path, uint8_t* buffer) {
	std::string pathexe = get_exe_path();
	std::string path2 = pathexe + "/" + std::string(path);
	/*
	FILE* file0;
	fopen_s(&file0, path2, (char*)"wb");
	char x = 1;
	fwrite(&x, 1, 1, file0);
	myclose(file0);
	*/
	FILE* file;
	//fopen_s(&file, (char*)"c:\\prenos\\remc2\\biggraphics\\out_rlt-n-out.data", (char*)"rb");
	file= fcaseopen(path2.c_str(), (char*)"rb");
	fseek(file, 0L, SEEK_END);
	long szdata = ftell(file);
	fseek(file, 0L, SEEK_SET);
	fread(buffer, szdata, 1, file);
	myclose(file);
};

bool ExistGraphicsfile(const char* path) {

	FILE* file;

	if ((file = fopen(path, "r")) != NULL) {
		fclose(file);
#ifdef DEBUG_START
		debug_printf("ffile_exists:true-%s\n", path);
#endif //DEBUG_START
		return true;
	}
#ifdef DEBUG_START
	debug_printf("ffile_exists:false-%s\n", path);
#endif //DEBUG_START
	return false;
}

void ReadGraphicsfile(const char* path, uint8_t* buffer, long size) 
{
	FILE* file;
	file = fcaseopen(path, (char*)"rb");
	if (file != NULL)
	{
		if (size == -1)
		{
			fseek(file, 0L, SEEK_END);
			size = ftell(file);
			fseek(file, 0L, SEEK_SET);
		}
		fread(buffer, size, 1, file);
		myclose(file);
	}
};

std::string getExistingDataPath(std::filesystem::path path) 
{
	std::vector<std::string> file_locations;
#ifdef __linux__
	auto env_home_dir = std::getenv("HOME");
	auto env_xdg_data_home_dir = std::getenv("XDG_DATA_HOME");
	std::filesystem::path home_dir;
	std::filesystem::path xdg_data_home_dir;
	if (env_home_dir) home_dir = env_home_dir;
	if (env_xdg_data_home_dir) xdg_data_home_dir = env_xdg_data_home_dir;

	if (std::filesystem::exists(xdg_data_home_dir)) {
		file_locations.emplace_back(xdg_data_home_dir / "remc1" / path);
	}
	if (std::filesystem::exists(home_dir)) {
		file_locations.emplace_back(home_dir / ".local" / "share" / "remc1" / path);
	}
#else //__linux__
	auto home_drive = std::getenv("HOMEDRIVE");
	auto home_path =  std::getenv("HOMEPATH");
	if (home_drive && home_path) {
		std::filesystem::path home_dir(std::string(home_drive) + "/" + std::string(home_path));
		file_locations.push_back((home_dir / "remc1" / path).string());
	}
#endif //__linux__
	file_locations.push_back(get_exe_path() + "/" + path.string());

	std::string file_found;

	// first location at which the file can be found is chosen
	for (const std::string &file_location: file_locations) {
#ifdef __linux__
		std::string caseInsensitivePath = casepath(file_location);
		if (std::filesystem::exists(caseInsensitivePath)) {
			file_found = std::string(caseInsensitivePath);
			break;
		}
#else //__linux__
		if (std::filesystem::exists(file_location)) {
			file_found = file_location;
			break;
		}
#endif //__linux__
	}
	if (CommandLineParams.DoShowDebugMessages1())
		std::cout << "Data file found: " << file_found << "\n";
	return file_found;
}

std::string GetSubDirectoryPath(const char* subDirectory)
{
	std::string path = getExistingDataPath(subDirectory);
	return path.c_str();
}

std::string GetSubDirectoryPath(const char* gamepath, const char* subDirectory)
{
	std::string path = getExistingDataPath(
		std::filesystem::path(gamepath) / std::filesystem::path(subDirectory)
	);
	return path.c_str();
}

std::string GetSubDirectoryFilePath(const char* subDirectory, const char* fileName)
{
	std::string subDirPath = GetSubDirectoryPath(subDirectory);
	return subDirPath + "/" + std::string(fileName);
}

std::string GetSubDirectoryFile(const char* gamepath, const char* subDirectory, const char* fileName)
{
	std::string subDirPath = GetSubDirectoryPath(gamepath, subDirectory);
	return subDirPath + "/" + std::string(fileName);
}

std::string GetSaveGameFile(const char* gamepath, int16_t index)
{
	std::string subDirPath = GetSubDirectoryPath(gamepath, "SAVE");
	char buffer[MAX_PATH];
	sprintf(buffer, "%s/SAVE%d.GAM", subDirPath.c_str(), index);
	return std::string(buffer);
}

int GetDirectory(char* directory, const char* filePath)
{
	string str = string(filePath);
	size_t found = str.find_last_of("/\\");

	if (found)
	{
		directory = strcpy(directory, str.substr(0, found).c_str());
		return 0;
	}
	return - 1;
}
