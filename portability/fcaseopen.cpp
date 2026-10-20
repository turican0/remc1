#include "fcaseopen.h"

#include <ctype.h>

static bool our_iequals(const std::string& a, const std::string& b) {
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
			return false;
	return true;
}

std::vector<std::string> GetTokensFromPath(const std::string &path) {
    size_t pos = 0;
    size_t start = 0;
    std::vector<std::string> tokens;

    while ((pos = path.find('/', start)) != std::string::npos) {
        if (pos != start)
            tokens.push_back(path.substr(start, pos - start));
        start = pos + 1;
    }
    tokens.push_back(path.substr(start, path.length() - start));

    return tokens;
}

#if !defined(_WIN32)
#include <filesystem>

std::string casepath(const std::string &inPath)
{
    // the path with the case of every existing leading component fixed; the components after the first
    // missing one are kept as written (a file or folder about to be created)
    // DOS code uses '\' separators; normalise first
    std::string path = inPath;
    for (auto& c : path)
        if (c == '\\')
            c = '/';

    std::vector<std::string> tokens = GetTokensFromPath(path);

    // NOTE: the first case-insensitive match is taken
    std::string result {""};
    if (!path.empty() && path[0] == '/')
        result = "/";
    else if (path.size() >= 2 && path[0] == '.' && path[1] == '/')
        result = "./";

    size_t start_i = 0;
    if (!tokens.empty() && tokens[0] == ".")
        start_i = 1;

    for (size_t i = start_i; i < tokens.size(); ++i) {
        std::string token = tokens[i];
        std::string current = result + token;

        if (!std::filesystem::exists(current)) {
            std::string dir = result;
            if (dir.empty())
                dir = ".";
            else if (dir.size() > 1 && dir.back() == '/')
                dir.pop_back();

            std::error_code ec;
            for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
                std::string test = entry.path().filename().string();
                if (our_iequals(token, test)) {
                    current = result + test;
                    break;
                }
            }
        }

        result = current + ((i != tokens.size()-1) ? "/" : "");
    }

    return result;
}
#endif

FILE* fcaseopen(char const* path, char const* mode)
{
#if defined(_WIN32)
    return fopen(path, mode);
#else
    FILE* f = fopen(path, mode);
    if (!f)
    {
        std::string r = casepath(path);
        f = fopen(r.c_str(), mode);
    }
    return f;
#endif
}
