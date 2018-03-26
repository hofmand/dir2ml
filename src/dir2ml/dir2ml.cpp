// dir2ml.cpp : https://tools.ietf.org/html/rfc5854
//

#include "stdafx.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdlib.h>
#include <sys/stat.h>  
#include <vector>

#include "windows.h"

// https://github.com/B-Con/crypto-algorithms
extern "C" {
#include "crypto-algorithms\base64.h"
#include "crypto-algorithms\md5.h"
#include "crypto-algorithms\sha1.h"
#include "crypto-algorithms\sha256.h"
}

// https://github.com/zeux/pugixml
#include "pugixml/src/pugixml.hpp"

// https://github.com/cxong/tinydir
#include "tinydir/tinydir.h"

constexpr wchar_t* APP_NAME = L"dir2ml";
constexpr wchar_t* VERSION_NO = L"0.5.0";

//////////////////////////////////////////////////////////////////////////
//
// dir2ml: read a directory structure and create a metalinks file.
//
//////////////////////////////////////////////////////////////////////////
//
// Example Usage:
//
// dir2ml -d ./MyMirror -u ftp://ftp.example.com -c us -o MyMirror.meta4
//
//////////////////////////////////////////////////////////////////////////
//
// Example output file:
//
// <?xml version="1.0" encoding="UTF-8"?>
// <metalink xmlns="urn:ietf:params:xml:ns:metalink" xmlns:nsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="metalink4.xsd">
//   <file name="example.ext">
//     <size>14471447</size>
//     <generator>dir2ml/0.1.0</generator>
//     <updated>2010-05-01T12:15:02Z</updated>
//     <hash type="sha-256">17bfc4a6058d2d7d82db859c8b0528c6ab48d832fed620ed49fb3385dbf1684d</hash>
//     <url location="us">ftp://ftp.example.com/example.ext</url>
//	 </file>
//   <file name="subdir/example2.ext">
//     <size>14471447</size>
//     <generator>dir2ml/0.1.0</generator>
//     <updated>2010-05-01T12:15:02Z</updated>
//     <hash type="sha-256">f44bcce2a9c2aa3f73ddc853ad98f87cd8e7cee5b5c18719ebb220da3fd4dbc9</hash>
//     <url location="us">ftp://ftp.example.com/subdir/example2.ext</url>
//	 </file>
// </metalink>
//
//////////////////////////////////////////////////////////////////////////

using namespace std;

#if __cplusplus < 201703L && defined _MSC_VER
namespace fs = std::experimental::filesystem::v1;
#else
namespace fs = std::filesystem;
#endif

typedef unsigned int sha256_data_size;
typedef unsigned long md5_data_size;
typedef sha256_data_size max_buf_size; // because sizeof(unsigned int) <= sizeof(unsigned long)

typedef uint_fast16_t process_dir_flags_t;

constexpr process_dir_flags_t FLAG_VERBOSE      = 0x0001;
constexpr process_dir_flags_t FLAG_MD5          = 0x0002;
constexpr process_dir_flags_t FLAG_SHA1         = 0x0004;
constexpr process_dir_flags_t FLAG_SHA256       = 0x0008;
constexpr process_dir_flags_t FLAG_NO_GENERATOR = 0x0010;
constexpr process_dir_flags_t FLAG_NO_DATE      = 0x0020;
constexpr process_dir_flags_t FLAG_NI_URL       = 0x0040;
constexpr process_dir_flags_t FLAG_MAGNET_URL   = 0x0080;
constexpr process_dir_flags_t FLAG_FILE_URL     = 0x0100;
constexpr process_dir_flags_t FLAG_BASE_URL     = 0x0200;

constexpr process_dir_flags_t FLAG_ALL_HASHES = FLAG_MD5 | FLAG_SHA1 | FLAG_SHA256;
constexpr process_dir_flags_t FLAG_SPARSE_OUTPUT = FLAG_NO_GENERATOR | FLAG_NO_DATE;
constexpr process_dir_flags_t FLAG_ALL_URL_TYPES = FLAG_NI_URL | FLAG_MAGNET_URL | FLAG_FILE_URL | FLAG_BASE_URL;

constexpr process_dir_flags_t FLAG_DEFAULT = FLAG_SHA256;

constexpr size_t PROGRESS_MARKER_BYTES = 1000000; // 1MB (not MiB)

struct ProcessDirContext
{
	ProcessDirContext() : numFiles(0), numBytes(0), dirDepth(0) {}

	size_t numFiles;
	uint_fast64_t numBytes;
	size_t dirDepth;
};

enum PATHTYPE { PATH_IS_FILE, PATH_IS_DIR };
typedef map<wstring, PATHTYPE> PATH_NAME_TYPE_MAP;

void ProcessDir( wstring const& inputBaseDirName,
                 wstring const& inputDirSuffixName,
                 pugi::xml_node& xmlRootNode,
                 wstring const& country,
                 wstring const& baseURL,
                 wstring const& baseUrlType,
                 wstring const& fileUrlBase,
                 wstring const& currentDate,
                 ProcessDirContext& ctx,
                 process_dir_flags_t flags )
{
	bool wantVerbose = ((flags & FLAG_VERBOSE) != 0);

	wstring inputDirName = inputBaseDirName;
	if (!inputDirSuffixName.empty())
		inputDirName += L"/" + inputDirSuffixName;

	++ctx.dirDepth;

	PATH_NAME_TYPE_MAP pathContents;
	{
		tinydir_dir inputDir;
		if (0 != tinydir_open(&inputDir, inputDirName.c_str()))
		{
			wcerr << L"Couldn't open input directory \"" << inputDirName
				<< L"\"!" << endl;
			return;
		}

		while(inputDir.has_next)
		{
			tinydir_file file;
			tinydir_readfile(&inputDir, &file);

			wstring fileName = file.name;
			if (fileName != L"." && fileName != L"..")
				pathContents[file.name] = file.is_dir ? PATH_IS_DIR : PATH_IS_FILE;

			tinydir_next(&inputDir);
		}

		tinydir_close(&inputDir);
	}

	if (wantVerbose)
		wcout << setw(ctx.dirDepth - 1) << setfill(L'|') << L"+" << inputDirName << endl;

	// Reuse the same file buffer to avoid reallocating memory.
	static vector<char> fileBuffer;
	constexpr max_buf_size bufferSize = 8192;

	uint_fast64_t oldNumBytes = 0;

	for(auto it = pathContents.begin(); it != pathContents.end(); ++it)
	{
		wstring fileName = it->first;
		wstring filePathRel = inputDirSuffixName;
		if (!filePathRel.empty())
			filePathRel += L"/";
		filePathRel += fileName;

		if (it->second == PATH_IS_DIR)
		{
			ProcessDir(inputBaseDirName, filePathRel, xmlRootNode,
				country, baseURL, baseUrlType, fileUrlBase, currentDate, ctx, flags);
		}
		else
		{
			++ctx.numFiles;

			// <file name="example.ext">
			wstring filePathAbs;
			{
				wostringstream buf;
				buf << inputDirName << L"/" << fileName;
				filePathAbs = buf.str();
			}

			if (wantVerbose)
				wcout << setw(ctx.dirDepth-1) << setfill(L'|') << L" " << fileName;

			pugi::xml_node xmlFileNode = xmlRootNode.append_child(L"file");
			xmlFileNode.append_attribute(L"name")
				.set_value(filePathRel.c_str());

			// <size>14471447</size>
			uint_fast64_t fileSize(0);
			ifstream inFile(filePathAbs, ifstream::ate | ifstream::binary);
			{
#ifdef WIN32
				// Use _wstat64 to make 32-bit windows build report the
				// correct file size, because ifstream::tellg() is limited
				// to 32 bits in 32-bit builds.
				{
					struct __stat64 fileInfo;
					switch (_wstat64(filePathAbs.c_str(), &fileInfo))
					{
					case 0:
						fileSize = fileInfo.st_size;
						break;
					case ENOENT:
						wcout << L"File \"" << filePathAbs << "\" not found!" << endl;
						return;
					default:
						wcout << L"Undefined error reading file \"" << filePathAbs << L"\"!" << endl;
						return;
					}
				}
#else
				// Not sure how to do the above in other operating systems
				fileSize = inFile.tellg();
#endif
				xmlFileNode.append_child(L"size")
					.append_child(pugi::node_pcdata)
					.set_value(to_wstring(fileSize).c_str());
				ctx.numBytes += fileSize;
			}

			if (!(flags & FLAG_NO_GENERATOR))
			{
				// <generator>dir2ml/0.1.0</generator>
				xmlFileNode.append_child(L"generator")
					.append_child(pugi::node_pcdata)
					.set_value((wstring(APP_NAME) + L"/" + VERSION_NO).c_str());
			}

			if (!(flags & FLAG_NO_DATE))
			{
				// <updated>2010-05-01T12:15:02Z</updated>
				xmlFileNode.append_child(L"updated")
					.append_child(pugi::node_pcdata)
					.set_value(currentDate.c_str());
			}

			// <hash type="md5">05c7d97c0e3a16ced35c2d9e4554f906</hash>
			// <hash type="sha-1">a97fcf6ba9358f8a6f62beee4421863d3e52b080</hash>
			// <hash type="sha-256">f0ad929cd259957e160ea442eb80986b5f01...</hash>
			if (flags & FLAG_ALL_HASHES)
			{
				MD5_CTX ctxMD5;
				md5_init(&ctxMD5);

				SHA1_CTX ctxSHA_1;
				sha1_init(&ctxSHA_1);

				SHA256_CTX ctxSHA_256;
				sha256_init(&ctxSHA_256);

				inFile.seekg(0, ios::beg);
				{
					while (!inFile.eof())
					{
						fileBuffer.resize(bufferSize);
						inFile.read(&fileBuffer[0], bufferSize);
						max_buf_size bytesRead = static_cast<max_buf_size>(inFile.gcount());
						if (bytesRead > 0)
						{
							if (flags & FLAG_MD5)
							{
								md5_update(&ctxMD5, reinterpret_cast<const uint8_t*>(&fileBuffer[0]),
									bytesRead);
							}

							if (flags & FLAG_SHA1)
							{
								sha1_update(&ctxSHA_1,
									reinterpret_cast<const uint8_t*>(&fileBuffer[0]),
									bytesRead);
							}

							if (flags & FLAG_SHA256)
							{
								sha256_update(&ctxSHA_256,
									reinterpret_cast<const uint8_t*>(&fileBuffer[0]),
									bytesRead);
							}

							// Every 1MB let's output a '.' just to prove we're still alive
							auto numTicks = ((oldNumBytes + bytesRead) / PROGRESS_MARKER_BYTES) - (oldNumBytes / PROGRESS_MARKER_BYTES);
							if(numTicks != 0)
								wcout << wstring(static_cast<size_t>(numTicks), L'.');
						} // end if (bytesRead > 0)

						oldNumBytes += bytesRead;
					} // end while (!inFile.eof())
				} // end scope

				// MD5
				wstring md5HashStr;
				static vector<uint8_t> md5Digest(MD5_BLOCK_SIZE);
				fill(md5Digest.begin(), md5Digest.end(), 0);
				if (flags & FLAG_MD5)
				{
					pugi::xml_node xmlHashNode = xmlFileNode.append_child(L"hash");
					xmlHashNode.append_attribute(L"type").set_value(L"md5");

					wostringstream buf;
					{
						md5_final(&ctxMD5, &md5Digest[0]);
						for (size_t i = 0; i < MD5_BLOCK_SIZE; ++i)
							buf << hex << setw(2) << setfill(L'0') << md5Digest[i];
					}

					md5HashStr = buf.str();
					xmlHashNode.append_child(pugi::node_pcdata)
						.set_value(md5HashStr.c_str());
				} // end MD5

				// SHA-1
				wstring sha1HashStr;
				static vector<uint8_t> sha1Digest(SHA1_BLOCK_SIZE);
				fill(sha1Digest.begin(), sha1Digest.end(), 0);
				if (flags & FLAG_SHA1)
				{
					pugi::xml_node xmlHashNode = xmlFileNode.append_child(L"hash");
					xmlHashNode.append_attribute(L"type").set_value(L"sha-1");

					wostringstream buf;
					{
						sha1_final(&ctxSHA_1, &sha1Digest[0]);
						for (size_t i = 0; i < SHA1_BLOCK_SIZE; ++i)
							buf << hex << setw(2) << setfill(L'0') << sha1Digest[i];
					}

					sha1HashStr = buf.str();
					xmlHashNode.append_child(pugi::node_pcdata)
						.set_value(sha1HashStr.c_str());
				} // end SHA-1

				// SHA-256
				wstring sha256HashStr;
				static vector<uint8_t> sha256Digest(SHA256_BLOCK_SIZE);
				fill(sha256Digest.begin(), sha256Digest.end(), 0);
				if (flags & FLAG_SHA256)
				{
					pugi::xml_node xmlHashNode = xmlFileNode.append_child(L"hash");
					xmlHashNode.append_attribute(L"type").set_value(L"sha-256");

					wostringstream buf;
					{
						sha256_final(&ctxSHA_256, &sha256Digest[0]);
						for (size_t i = 0; i < SHA256_BLOCK_SIZE; ++i)
							buf << hex << setw(2) << setfill(L'0') << sha256Digest[i];
					}

					sha256HashStr = buf.str();
					xmlHashNode.append_child(pugi::node_pcdata)
						.set_value(sha256HashStr.c_str());
				} // end SHA-256

				// https://tools.ietf.org/html/rfc6920
				// Example URL: ni:///sha-256;UyaQV-Ev4rdLoHyJJWCi11OHfrYv9E1aGQAlMO2X_-Q
				if ((flags & FLAG_NI_URL) && (flags & FLAG_SHA256))
				{
					// Construct the URL in MBCS
					auto bufSize = base64_encode(&sha256Digest[0], nullptr, sha256Digest.size(), 0);
					string url8(bufSize, ' ');
					base64_encode(&sha256Digest[0],
						reinterpret_cast<uint8_t*>(&url8[0]), sha256Digest.size(), 0);
					url8.erase(url8.find_last_not_of("=")+1); // trim trailing "=" padding characters
					url8.insert(0, "ni://sha-256;");

					// Convert to Unicode for pugixml
					wstring_convert<codecvt_utf8<wchar_t> > conv;
					wstring url16 = conv.from_bytes(url8);
						
					pugi::xml_node xmlUrlNode = xmlFileNode.append_child(L"url");
					xmlUrlNode.append_attribute(L"type").set_value(L"ni");
					xmlUrlNode.append_child(pugi::node_pcdata)
						.set_value(url16.c_str());
				}

				// Magnet links
				if ((flags & FLAG_MAGNET_URL) && (flags & FLAG_SHA256))
				{
					wostringstream buf;
					buf << L"magnet:?xt=urn:sha256:" << sha256HashStr;

					pugi::xml_node xmlUrlNode = xmlFileNode.append_child(L"url");
					xmlUrlNode.append_attribute(L"type").set_value(L"magnet");
					xmlUrlNode.append_child(pugi::node_pcdata)
						.set_value(buf.str().c_str());
				}
			} // end if any hashes enabled

			// <url location="us">ftp://ftp.example.com/example.ext</url>
			if(flags & FLAG_BASE_URL)
			{
				pugi::xml_node xmlUrlNode = xmlFileNode.append_child(L"url");
				if (!country.empty())
					xmlUrlNode.append_attribute(L"location").set_value(country.c_str());
				xmlUrlNode.append_attribute(L"type").set_value(baseUrlType.c_str());

				wostringstream buf;
				buf << baseURL;
				if (baseURL.back() != L'/')
					buf << L"/";
				buf << filePathRel;

				xmlUrlNode.append_child(pugi::node_pcdata)
					.set_value(buf.str().c_str());
			} // end <url>..</url>

			// <url location="us">ftp://ftp.example.com/example.ext</url>
			if(flags & FLAG_FILE_URL)
			{
				pugi::xml_node xmlUrlNode = xmlFileNode.append_child(L"url");
				xmlUrlNode.append_attribute(L"type").set_value(L"file");

				wostringstream buf;
				buf << fileUrlBase;
				if (fileUrlBase.back() != L'/')
					buf << L"/";
				buf << filePathRel;

				xmlUrlNode.append_child(pugi::node_pcdata)
					.set_value(buf.str().c_str());
			} // end <url>..</url>

			if(wantVerbose)
				wcout << endl;
		} // end file
	} // end files in this directory

	--ctx.dirDepth;
}

int wmain( int argc, wchar_t **argv )
{
	wstring inputDirName, baseURL, fileUrlBase, country, outFileName;

	bool invalidArgs(false);
	bool wantHelp(false), wantStatistics(false), wantVerbose(false);

	wcout << APP_NAME << L" " << VERSION_NO << L"\n" << endl;

	process_dir_flags_t flags(FLAG_DEFAULT);

	// Process arguments (stage 1)
	for(int a = 1; a < argc; ++a)
	{
		bool validArg(false);

		wstring argText = argv[a];
		if(argText == L"-d" || argText == L"--directory")
		{
			if (a < argc - 1)
			{
				++a;
				inputDirName = argv[a];
				validArg = true;
			}
		}
		else if (argText == L"-u" || argText == L"--base-url")
		{
			if (a < argc - 1)
			{
				++a;
				baseURL = argv[a];
				flags |= FLAG_BASE_URL;
				validArg = true;
			}
		}
		else if (argText == L"-o" || argText == L"--output")
		{
			if (a < argc - 1)
			{
				++a;
				outFileName = argv[a];
				validArg = true;
			}
		}
		else if (argText == L"-c" || argText == L"--country")
		{
			if (a < argc - 1)
			{
				++a;
				country = argv[a];
				validArg = true;
			}
		}
		else if (argText == L"-h" || argText == L"--help")
		{
			wantHelp = true;
			validArg = true;
		}
		else if (argText == L"-s" || argText == L"--show-statistics")
		{
			wantStatistics = true;
			validArg = true;
		}
		else if (argText == L"-v" || argText == L"--verbose")
		{
			wantVerbose = true;
			flags |= FLAG_VERBOSE;
			validArg = true;
		}
		else if (argText == L"--version")
		{
			validArg = true;
			return EXIT_SUCCESS;
		}
		else if (argText == L"--hash-type")
		{
			flags &= ~FLAG_ALL_HASHES;

			if (a < argc - 1)
			{
				++a;
				std::wstringstream ss(argv[a]);
				while (ss.good())
				{
					wstring substr;
					getline(ss, substr, L',');
					if (substr == L"md5")
					{
						flags |= FLAG_MD5;
						validArg = true;
					}
					else if (substr == L"sha1")
					{
						flags |= FLAG_SHA1;
						validArg = true;
					}
					else if (substr == L"sha256")
					{
						flags |= FLAG_SHA256;
						validArg = true;
					}
					else
					{
						validArg = false;
						break;
					}
				}
			}
		}
		else if (argText == L"--sparse-output")
		{
			flags |= FLAG_SPARSE_OUTPUT;
			validArg = true;
		}
		else if (argText == L"--no-generator")
		{
			flags |= FLAG_NO_GENERATOR;
			validArg = true;
		}
		else if (argText == L"--no-date")
		{
			flags |= FLAG_NO_DATE;
			validArg = true;
		}
		else if (argText == L"--ni-url")
		{
			flags |= FLAG_NI_URL;
			validArg = true;
		}
		else if (argText == L"--magnet-url")
		{
			flags |= FLAG_MAGNET_URL;
			validArg = true;
		}
		else if (argText == L"-f" || argText == L"--file-url")
		{
			flags |= FLAG_FILE_URL;
			validArg = true;
		}

		if (!validArg)
		{
			wcerr << L"Invalid argument: \"" << argv[a] << L"\""
				<< endl;
			invalidArgs = true;
		}
	}

	// Process arguments (stage 2)
	wstring baseUrlType;
	if (!wantHelp && !invalidArgs)
	{
		// Input directory
		if (inputDirName.empty())
		{
			wcerr << L"Missing input directory!" << endl;
			invalidArgs = true;
		}

		// At least one URL type must be supplied
		if ((flags & FLAG_ALL_URL_TYPES) == 0)
		{
			wcerr << L"Missing -u/--base-url, -f/--file-url, --ni-url, or --magnet-url!" << endl;
			invalidArgs = true;
		}

		// Base URL
		if (flags & FLAG_BASE_URL)
		{
			auto i = baseURL.find(L"://"); // e.g. ftp://www.example.com
			if (i != wstring::npos)
			{
				baseUrlType = baseURL.substr(0, i);
			}
			else
			{
				wcerr << L"Invalid -u/--base-url!" << endl;
				invalidArgs = true;
			}
		} // end scope

		if (flags & FLAG_FILE_URL)
		{
			// RFC1738 says, "A file URL takes the form: file://<host>/<path>"

			// Input:  //server/share/file.ext
			// Output: file://server/share/file.ext
			//
			// Input:  \\server\share\file.ext
			// Output: file://server/share/file.ext
			//
			// Input:  C:\WINDOWS\clock.avi
			// Output: file:///c:/WINDOWS/clock.avi
			//
			// Input:  file:///c:/WINDOWS/clock.avi
			// Output: file:///c:/WINDOWS/clock.avi

			// Append missing trailing "/" so fs::canonical() doesn't think it's a file
			fileUrlBase = inputDirName;
			if (fileUrlBase.back() != L'/' && fileUrlBase.back() != L'\\')
				fileUrlBase += L"/";

			// Get the full path of base-url
			error_code ec;
			fs::path canonicalPath = fs::canonical(fileUrlBase, ec);
			if (ec)
			{
				wcerr << L"Can't get canonical path from \"" << inputDirName
					<< L"\"! ";
				cerr << ec.message() << endl;
				return EXIT_FAILURE;
			}

			// Prepend 'file://'
			wstring canonicalName = canonicalPath;
			{
				// root-name can be "C:" or "//myserver"
				wstring rootName = canonicalPath.root_name().wstring();
				if (rootName.size() == 2 && rootName[1] == L':')
				{
					canonicalName[0] = tolower(canonicalName[0]);
					canonicalName.insert(0, L"///");
				}
			}
			fileUrlBase = fs::path(L"file:").append(canonicalName);
			replace(fileUrlBase.begin(), fileUrlBase.end(), L'\\', L'/');
		} // end FLAG_FILE

		// Country code
		if (!country.empty() && country.size() != 2)
		{
			wcerr << "Invalid country code \"" << country << "\"!" << endl;
			invalidArgs = true;
		}

		// Output filename
		if (outFileName.empty())
		{
			wcerr << L"Missing output filename!" << endl;
			invalidArgs = true;
		}
	}

	if (invalidArgs && !wantHelp)
		wcout << L"\nTry:\t" << APP_NAME << " --help" << endl;

	if (wantHelp)
	{
		wcout << L"Usage:\n"
			<< L"\n"
			<< L" " << APP_NAME << " --help\n"
			<< L" " << APP_NAME << " --directory path [--base-url url] --output outfile [--country code] [--verbose]\n"
			<< L" " << APP_NAME << " -d directory-path -u base-url -o outfile [-c country-code] [-v]\n"
			<< L"\n"
			<< L"Example usage:\n"
			<< L"\n"
			<< L" " << APP_NAME << " -d ./MyMirror -u ftp://ftp.example.com -c us --sha256 -o MyMirror.meta4\n"
			<< L"\n"
			<< L"Required Arguments:\n"
			<< L"\n"
			<< L" -d, --directory directory - The directory path to process\n"
			<< L" -o, --output outfile - Output filename(.meta4 or .metalink)\n"
			<< L"\n"
			<< L"At least one of -u/--base-url, -f/--file-url, --ni-url, or --magnet-url must be supplied.\n"
			<< L"\n"
			<< L"Optional Arguments:\n"
			<< L"\n"
			<< L" -c, --country country-code - ISO3166-1 alpha-2 two letter country code of the server specified by base-url above\n"
			<< L" -h, --help - Show this screen\n"
			<< L" -s, --show-statistics - Show statistics at the end of processing\n"
			<< L" -u, --base-url base-url - The base/root URL of an online directory containing the files. For example, ftp://ftp.example.com. " << APP_NAME << " will append the relative path of each file to the base-url.\n"
			<< L" -f, --file - Add a local source for the file, using the directory specified by `--directory` prepended by `file://`. This is useful for fingerprinting a directory or hard drive."
			<< L"   Note: on Windows, backslashes (\\) in the base-url will be replaced by forward slashes (/).\n"
			<< L" -v, --verbose - Verbose output to stdout\n"
			<< L" --hash-type hash-list - Calculate and output hash-list (comma-separated). Available hashes are md5, sha1, and sha256. If none are specified, sha256 is used.\n"
			<< L" --sparse-output - combines --no-generator and --no-date to simplify diffs\n"
			<< L" --no-generator - the name of the tool used to generate the .meta4 file\n"
			<< L" --no-date - Don't output the date the .meta4 file was generated\n"
			<< L" --ni-url - Output Named Information (RFC6920) links (experimental)\n"
			<< L" --magnet-url - Output magnet links (experimental)" << endl;
	}

	if (invalidArgs)
		return EXIT_FAILURE;
	else if (wantHelp)
		return EXIT_SUCCESS;

	// <?xml version="1.0" encoding="UTF-8"?>
	pugi::xml_document xmlDoc;
	pugi::xml_node xmlDeclNode = xmlDoc.append_child(pugi::node_declaration);
	xmlDeclNode.set_name(L"xml");
	xmlDeclNode.append_attribute(L"version")
		.set_value(L"1.0");

	// <metalink xmlns="urn:ietf:params:xml:ns:metalink">
	pugi::xml_node xmlRootNode = xmlDoc.append_child(L"metalink");
	xmlRootNode.append_attribute(L"xmlns")
		.set_value(L"urn:ietf:params:xml:ns:metalink");
	xmlRootNode.append_attribute(L"xmlns:nsi")
		.set_value(L"http://www.w3.org/2001/XMLSchema-instance");
	xmlRootNode.append_attribute(L"xsi:noNamespaceSchemaLocation")
		.set_value(L"metalink4.xsd");

	wstring currentDate;
	{
		time_t now;
		time(&now);
		wchar_t buf[sizeof L"2011-10-08T07:07:09Z"];
		struct tm newTime;
		gmtime_s(&newTime, &now);
		wcsftime(buf, sizeof buf, L"%FT%TZ", &newTime);
		currentDate = buf;
	}

	auto startTick = GetTickCount64();

	ProcessDirContext ctx;
	ProcessDir(inputDirName, L"", xmlRootNode, country, baseURL, baseUrlType, fileUrlBase, currentDate, ctx, flags);

	auto endTick = GetTickCount64();
	if(wantStatistics)
	{
		double numSeconds = (endTick - startTick) / 1000.;
		double Mbps = ((ctx.numBytes / 1e6) * 8) / numSeconds;
		wcout << L"\n" << APP_NAME << L" processed " << ctx.numBytes << L" bytes"
			<< L" in " << ctx.numFiles << L" files"
			<< L" in " << numSeconds << L" seconds"
			<< L" (" << Mbps << L" Mbps)." << endl;
	}

	if (!xmlDoc.save_file(outFileName.c_str(), PUGIXML_TEXT("\t"),
		pugi::format_default | pugi::format_write_bom
		| pugi::format_save_file_text,
		pugi::encoding_utf8))
	{
		wcerr << L"Unable to write to file \"" << outFileName << L"\"!";
		return EXIT_FAILURE;
	}

    return EXIT_SUCCESS;
}

