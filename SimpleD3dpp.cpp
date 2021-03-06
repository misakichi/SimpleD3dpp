/**
 *	@file SmpleD3dpp.cpp
 *	@author	k.misakichi
 *	@brief d3dpreprocess cui tool.
 *	@url https://github.com/misakichi/
 */
//#include <SDKDDKVer.h>
#define _WIN32_WINNT	0x0601
#include <stdio.h>
#include <tchar.h>

#include <d3dcompiler.h>
#include <vector>
#include <map>
#include <string>
#include <stack>
#include <io.h>
#include <Windows.h>
#include <fcntl.h> 
#include <functional>

#pragma comment(lib, "D3dcompiler.lib")

#ifdef _DEBUG
#define DBG_PRINTF(x, ...)	printf(x, __VA_ARGS__)
#else
#define DBG_PRINTF(x, ...)	
#endif

void printUsage()
{
	printf("usage : sd3dpp.exe [options] source\n");
	printf("option:\n");
	printf("    -D<DEFINE>[=<VALUE>]\n");
	printf("    -Include<IncludePath>\n");
	printf("    -Ignore<keyword> : #keyword is ignore D3DPreprocess.\n");

}

static std::stack<std::string> s_currentFile;
static std::string fullPathToDir(const std::string& fullpath) {
	auto pos = fullpath.find_last_of('\\');
	if (pos != std::string::npos) {
		return fullpath.substr(0,pos + 1);
	} else {
		return std::string(".\\");
	}
}
char* textRead(const char* path, size_t* pSize) {
	int fd = -1;
	if (_sopen_s(&fd, path, _O_RDONLY | _O_TEXT, _SH_DENYWR, _S_IREAD) != 0) {
		printf("file open error.");
		return nullptr;
	}
	auto size = _filelengthi64(fd);
	auto buf = (char*)malloc(size + 1);
	_read(fd, buf, (unsigned int)(size));
	buf[size] = 0;
	_close(fd);
	if (pSize)
		*pSize = size;

	return buf;
}
int main(int argc, char* argv[])
{
	if (argc == 1) {
		printUsage();
		return 0;
	}

	std::vector<std::string>			includePaths;
	std::vector<std::string>			ignorePreProcess;
	std::map<std::string, std::string>	defines;

	//parse args
	for (int i = 1; i < argc - 1; i++) {
		if (strncmp(argv[i], "-Include", 8) == 0) {
			std::string value = argv[i] + 8;
			//trim space
			while (value.length()>0 && *value.begin() == ' ') {
				value.erase(value.begin());
			}
			while (value.length()>0 && *(value.end()-1) == ' ') {
				value.erase(value.end()-1);
			}
			//trim
			if (value.length() > 2 && *value.begin() == '\"' && *(value.end() - 1) == '\"') {
				value = value.substr(1, value.length() - 2);
			}
			DBG_PRINTF("AddIncludePath:%s\n", value);
			includePaths.push_back(value);

		} else if (strncmp(argv[i], "-D", 2) == 0) {
			char* def = argv[i] + 2;
			std::string value = "";
			std::string name = def;
			char* eqPoint = strchr(def, '=');
			if (eqPoint) {
				value = eqPoint + 1;
				*eqPoint = 0;
				name = def;
			}
			DBG_PRINTF("AddDefine:%s=%s\n", name, value);
			defines.insert({ name, value });
		} else if (strncmp(argv[i], "-Ignore", 7) == 0) {
			DBG_PRINTF("Add Ignore Preprocess Keyword:#%s\n", (argv[i] + 7));
			ignorePreProcess.push_back(std::string("#") + (argv[i] + 7));
		} else {
			printf("invalid option.\n");
			printf(" -> %s", argv[i]);
			printf("please run sd3pp.exe on non argument to print help.");
			return -1;
		}
	}
	const char* file = argv[argc - 1];
	switch (_access(file, 04)) {
	case ENOENT:
		printf("file name or path not found.");
		return -2;
	case EACCES:
		printf("not allowed read access.");
		return -4;
	case EINVAL:
		printf("error.");
		return -100;
	}
	char fullpath[2048] = {};
	GetFullPathName(file, sizeof(fullpath), fullpath, NULL);

	s_currentFile.push(fullpath);

	class CIncludeProc : public ID3DInclude{
	public:
		CIncludeProc(const std::vector<std::string>& includePath)
			: systemIncludePath_(includePath)
		{
		}

		STDMETHOD(Open) (D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
		{
			std::string includeFilePath;
			HRESULT	result = E_FAIL;
			switch (IncludeType) {
			case D3D_INCLUDE_LOCAL:
				includeFilePath = fullPathToDir(s_currentFile.top()) + pFileName;
				DBG_PRINTF("include open local:%s\n", includeFilePath.c_str());
				if (_access(includeFilePath.c_str(), 04) != 0) {
					DBG_PRINTF("fail.\n");
					return E_FAIL;
				}
				DBG_PRINTF("ok.\n");
				break;
			case D3D_INCLUDE_SYSTEM:
			{
				bool found = false;
				for (auto& dir : systemIncludePath_) {
					includeFilePath = fullPathToDir(s_currentFile.top()) + pFileName;
					DBG_PRINTF("include open system:%s...\n", includeFilePath.c_str());
					if (_access(includeFilePath.c_str(), 04) == 0) {
						DBG_PRINTF("no.\n");
						found = true;
						break;
					}
				}
				if (!found) {
					DBG_PRINTF("fail.\n");
					return E_FAIL;
				}
				DBG_PRINTF("ok.\n");
				break;
			}
			default:
				return E_FAIL;
			}

			size_t size;
			auto buf = textRead(includeFilePath.c_str(), &size);
			*ppData = buf;
			*pBytes = UINT(size);
			DBG_PRINTF("push include:%s\n", includeFilePath.c_str());
			s_currentFile.push(includeFilePath);
			return S_OK;

		}

		STDMETHOD(Close) (LPCVOID pData)
		{
			free(const_cast<void*>(pData));
			DBG_PRINTF("pop include:%s\n", s_currentFile.top().c_str());
			s_currentFile.pop();
			return S_OK;
		}
	private:
		const std::vector<std::string>&	systemIncludePath_;

	};

	DBG_PRINTF("read source:%s\n", fullpath);
	size_t sourceSize;
	auto source = std::string(textRead(fullpath, &sourceSize));

	//remove char
	auto removeCh = [](char rmCh, std::string& rstr) {
		auto it = rstr.begin();
		while (it != rstr.end()) {
			if (*it == rmCh)
				it = rstr.erase(it);
			else
				it++;
		}
		return;
	};
	//remove /*XXX*/
	 auto removeBlockComment = [](std::string& str) {
		 while (1) {
			 auto coStart = str.find("/*");
			 if (coStart == std::string::npos) {
				 return;
			 } else {
				 auto coEnd = str.find("*/", coStart + 2);
				 if (coEnd == std::string::npos) {
					 return;
				 }
				 DBG_PRINTF("remove block comment.\n");
				 str.erase(str.begin() + coStart, str.begin() + coEnd + 2);
			 }
		 }
	};

	removeBlockComment(source);
	//comment out preprocessor keyword for ignore in temporary
	const std::string ignoreCmmentWord("//##c!m!t##");
	for (auto& ignore : ignorePreProcess) {
		DBG_PRINTF("ignorePreProcess(%s) process start!\n", ignore.c_str());
		size_t searchStart = 0;
		do {
			searchStart = source.find(ignore, searchStart);
			if (searchStart == std::string::npos)
				break;
			auto lineStart = source.rfind('\n', searchStart);
			if (lineStart == std::string::npos)
				lineStart = 0;
			else
				lineStart++;
			auto keywordPreStrs = source.substr(lineStart, searchStart - lineStart);

			removeCh(' ', keywordPreStrs);		
			removeCh('\t', keywordPreStrs);
			if (keywordPreStrs.length() == 0) {
				//preprocess keword enabled
				source.insert(searchStart, ignoreCmmentWord);
				searchStart += ignoreCmmentWord.length();
				searchStart += ignore.length();
				DBG_PRINTF("deleted comment out of preprocessor keyword on ignore...\n", ignore.c_str());
			} else {
				searchStart += ignore.length();
			}
		} while (1);
		DBG_PRINTF("ignorePreProcess(%s) process end.\n", ignore.c_str());
	}

	CIncludeProc	incProc(includePaths);
	std::vector<D3D_SHADER_MACRO> definesData; 
	definesData.reserve(defines.size() + 1);
	for (auto& def : defines) {
		definesData.push_back({ def.first.c_str(), def.second.c_str() });
	}
	definesData.push_back({ nullptr,nullptr });

	ID3DBlob* pResult = nullptr;
	ID3DBlob* pError = nullptr;
	auto result = D3DPreprocess(
					source.c_str(),
					sourceSize,
					fullpath,
					&*definesData.begin(),
					&incProc,
					&pResult,
					&pError);

	if (result != S_OK) {
		if (pResult) {
			printf("%s\n", (const char*)pResult->GetBufferPointer());
		}
		if (pError) {
			printf("%s\n", (const char*)pError->GetBufferPointer());
		}
	}
	if (pResult) {
		std::string resultStr((const char*)pResult->GetBufferPointer());
		while (1) {
			auto ignoreCmtWordPos = resultStr.find(ignoreCmmentWord);
			if (ignoreCmtWordPos == std::string::npos)
				break;
			DBG_PRINTF("delete ignore keyword comment out.\n");
			resultStr.erase(resultStr.begin() + ignoreCmtWordPos, resultStr.begin() + ignoreCmtWordPos + ignoreCmmentWord.length());
		}
		printf(resultStr.c_str());
	} else {
		printf("unknown error.\n");
		return -30;
	}
    return 0;
}

