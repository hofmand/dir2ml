// mlbuilder.cpp : https://tools.ietf.org/html/rfc5854
//

#include "stdafx.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

// https://github.com/ogay/sha2/
#include "fips_sha2/sha2.h"

// http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
extern "C" {
#include "openwall/md5.h"
}

// https://github.com/zeux/pugixml
#include "pugixml/src/pugixml.hpp"

// https://github.com/cxong/tinydir
#include "tinydir/tinydir.h"

// https://github.com/mohaps/TinySHA1
#include "TinySHA1/TinySHA1.hpp"

constexpr wchar_t* APP_NAME = L"mlbuilder";
constexpr wchar_t* VERSION_NO = L"0.3.0";

//////////////////////////////////////////////////////////////////////////
//
// mlbuilder: read a directory structure and create a metalinks file.
//
//////////////////////////////////////////////////////////////////////////
//
// Usage:
//
// mlbuilder --help
// mlbuilder --directory path --base-url url --output outfile [--country code] [--verbose]
// mlbuilder -d directory-path -u base-url -o outfile [-c country-code] [-v]
//
// Example usage:
//
// mlbuilder -d ./MyMirror -u ftp://ftp.example.com -c us -o MyMirror.meta4
//
// Required Arguments:
//
// -h, --help - Show this screen
// -d, --directory directory - The directory path to process
// -u, --base-url base-url - The URL of the source directory
// -o, --output - Output filename(.meta4 or .metalink)
//
// Optional Arguments:
//
// -c, --country country-code - ISO3166-1 alpha-2 two letter country code of the server specified by base-url above
// -v, --verbose - Verbose output
// --no-md5 - Don't calculate MD5
// --no-sha1 - Don't calculate SHA-1
// --no-sha256 - Don't calculate SHA-256
// --no-hash - Don't calculate _any_ hashes
//
//////////////////////////////////////////////////////////////////////////
//
// Example Usage:
//
// mlbuilder -d ./MyMirror -u ftp://ftp.example.com -l us -o MyMirror.meta4
//
//////////////////////////////////////////////////////////////////////////
//
// Example output file:
//
// <?xml version="1.0" encoding="UTF-8"?>
// <metalink xmlns="urn:ietf:params:xml:ns:metalink">
//  <file name="example.ext">
//    <size>14471447</size>
//    <identity>Example</identity>
//    <description>A description of the example file for download.</description>
//    <hash type="sha-256">f0ad929cd259957e160ea442eb80986b5f01...</hash>
//    <url location="us">ftp://ftp.example.com/example.ext</url>
//    <url>http://example.com/example.ext</url>
//    <metaurl mediatype="torrent">http://example.com/example.ext.torrent</metaurl>
//	 </file>
//	</metalink>
//
//////////////////////////////////////////////////////////////////////////

using namespace std;

typedef unsigned int sha256_data_size;
typedef unsigned long md5_data_size;
typedef sha256_data_size max_buf_size; // because sizeof(unsigned int) <= sizeof(unsigned long)

typedef uint_fast16_t process_dir_flags_t;

constexpr process_dir_flags_t FLAG_VERBOSE   = 0x0001;
constexpr process_dir_flags_t FLAG_NO_MD5    = 0x0002;
constexpr process_dir_flags_t FLAG_NO_SHA1   = 0x0004;
constexpr process_dir_flags_t FLAG_NO_SHA256 = 0x0008;

struct ProcessDirContext
{
	ProcessDirContext() : numFiles(0), numBytes(0), dirDepth(0) {}

	size_t numFiles;
	uint_fast64_t numBytes;
	size_t dirDepth;
};

void ProcessDir( wstring const& inputBaseDirName,
                 wstring const& inputDirSuffixName,
                 pugi::xml_node& xmlRootNode,
                 std::wstring const& country,
                 std::wstring const& baseURL,
                 std::wstring const& currentDate,
                 ProcessDirContext& ctx,
                 process_dir_flags_t flags )
{
	bool wantVerbose = ((flags & FLAG_VERBOSE) != 0);

	wstring inputDirName = inputBaseDirName;
	if (!inputDirSuffixName.empty())
		inputDirName += L"/" + inputDirSuffixName;

	++ctx.dirDepth;

	tinydir_dir inputDir;
	if (0 != tinydir_open_sorted(&inputDir, inputDirName.c_str()))
	{
		wcerr << L"Couldn't open input directory \"" << inputDirName
			<< L"\"!" << endl;
		return;
	}

	if(wantVerbose)
		wcout << setw(ctx.dirDepth-1) << setfill(L'|') << L"+" << inputDirName << endl;

	// Reuse the same file buffer to avoid reallocating memory.
	static std::vector<char> fileBuffer;
	constexpr max_buf_size bufferSize = 8192;

	for (int i = 0; i < inputDir.n_files; i++)
	{
		tinydir_file file;
		tinydir_readfile_n(&inputDir, &file, i);
		std::wstring fileName = file.name;
		std::wstring filePathRel = inputDirSuffixName;
		if (!filePathRel.empty())
			filePathRel += L"/";
		filePathRel += file.name;

		if (file.is_dir)
		{
			if (fileName != L"." && fileName != L"..")
			{
				ProcessDir(inputBaseDirName, filePathRel, xmlRootNode,
					country, baseURL, currentDate, ctx, flags);
			}
		}
		else
		{
			++ctx.numFiles;

			// <file name="example.ext">
			wstring filePathAbs;
			{
				wostringstream buf;
				buf << inputDirName << L"/" << file.name;
				filePathAbs = buf.str();
			}

			if (wantVerbose)
				wcout << setw(ctx.dirDepth-1) << setfill(L'|') << L" " << file.name << endl;

			pugi::xml_node xmlFileNode = xmlRootNode.append_child(L"file");
			xmlFileNode.append_attribute(L"name")
				.set_value(filePathRel.c_str());

			// <size>14471447</size>
			ifstream inFile(filePathAbs, ifstream::ate | ifstream::binary);
			{
				auto fileSize = inFile.tellg();
				xmlFileNode.append_child(L"size")
					.append_child(pugi::node_pcdata)
					.set_value(to_wstring(fileSize).c_str());
				ctx.numBytes += fileSize;
			}

			// <identity>Example</identity>
			// TODO ?

			// <description>A description of the example file for download.</description>
			// TODO ?

			// <generator>mlbuilder/0.1.0</generator>
			xmlFileNode.append_child(L"generator")
				.append_child(pugi::node_pcdata)
				.set_value( (std::wstring(L"mlbuild/") + VERSION_NO).c_str());

			// <updated>2010-05-01T12:15:02Z</updated>
			xmlFileNode.append_child(L"updated")
				.append_child(pugi::node_pcdata)
				.set_value(currentDate.c_str());

			// <hash type="md5">05c7d97c0e3a16ced35c2d9e4554f906</hash>
			// <hash type="sha-1">a97fcf6ba9358f8a6f62beee4421863d3e52b080</hash>
			// <hash type="sha-256">f0ad929cd259957e160ea442eb80986b5f01...</hash>
			{
				MD5_CTX ctxMD5;
				MD5_Init(&ctxMD5);

				sha1::SHA1 s1;

				sha256_ctx ctxSHA_256;
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
							if (!(flags & FLAG_NO_MD5))
							{
								MD5_Update(&ctxMD5, reinterpret_cast<const unsigned char*>(&fileBuffer[0]),
									bytesRead);
							}

							if (!(flags & FLAG_NO_SHA1))
							{
								// TinySHA1 is currently the slowest hasher of the three.
								// TODO: find a faster one!
								s1.processBytes(&fileBuffer[0], bytesRead);
							}

							if (!(flags & FLAG_NO_SHA256))
							{
								sha256_update(&ctxSHA_256,
									reinterpret_cast<const unsigned char*>(&fileBuffer[0]),
									bytesRead);
							}
						}
					}
				}

				// MD5
				if (!(flags & FLAG_NO_MD5))
				{
					pugi::xml_node xmlHashNode = xmlFileNode.append_child(L"hash");
					xmlHashNode.append_attribute(L"type").set_value(L"md5");

#define MD5_DIGEST_SIZE (128/8)

					wostringstream buf;
					{
						unsigned char digest[MD5_DIGEST_SIZE];
						memset(digest, 0, MD5_DIGEST_SIZE);
						MD5_Final(digest, &ctxMD5);
						for (size_t i = 0; i < MD5_DIGEST_SIZE; ++i)
							buf << hex << setw(2) << setfill(L'0') << digest[i];
					}

					xmlHashNode.append_child(pugi::node_pcdata)
						.set_value(buf.str().c_str());
				}

				// SHA-1
				if (!(flags & FLAG_NO_SHA1))
				{
					pugi::xml_node xmlHashNode = xmlFileNode.append_child(L"hash");
					xmlHashNode.append_attribute(L"type").set_value(L"sha-1");

					wostringstream buf;
					{
						uint32_t digest[5];
						s1.getDigest(digest);
						for (size_t i = 0; i < 5; ++i)
							buf << hex << setw(8) << setfill(L'0') << digest[i];
					}
					xmlHashNode.append_child(pugi::node_pcdata)
						.set_value(buf.str().c_str());
				}

				// SHA-256
				if (!(flags & FLAG_NO_SHA256))
				{
					pugi::xml_node xmlHashNode = xmlFileNode.append_child(L"hash");
					xmlHashNode.append_attribute(L"type").set_value(L"sha-256");

					wostringstream buf;
					{
						unsigned char digest[SHA256_DIGEST_SIZE];
						memset(digest, 0, SHA256_DIGEST_SIZE);
						sha256_final(&ctxSHA_256, digest);
						for (size_t i = 0; i < SHA256_DIGEST_SIZE; ++i)
							buf << hex << setw(2) << setfill(L'0') << digest[i];
					}

					xmlHashNode.append_child(pugi::node_pcdata)
						.set_value(buf.str().c_str());
				}
			}
			// <url location="us">ftp://ftp.example.com/example.ext</url>
			{
				pugi::xml_node xmlUrlNode = xmlFileNode.append_child(L"url");
				if (!country.empty())
					xmlUrlNode.append_attribute(L"location").set_value(country.c_str());

				wostringstream buf;
				buf << baseURL;
				if (baseURL.back() != L'/')
					buf << L"/";
				buf << filePathRel;


				xmlUrlNode.append_child(pugi::node_pcdata)
					.set_value(buf.str().c_str());
			}
		}

		tinydir_next(&inputDir);
	}

	tinydir_close(&inputDir);

	--ctx.dirDepth;
}

int wmain( int argc, wchar_t **argv )
{
	wstring inputDirName, baseURL, country, outFileName;

	bool invalidArgs(false);
	bool wantHelp(false), wantVerbose(false);

	wcout << APP_NAME << L" " << VERSION_NO << L"\n" << endl;

	process_dir_flags_t flags(0);

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
		else if (argText == L"-o" || argText == L"--output")
		{
			if (a < argc - 1)
			{
				++a;
				outFileName = argv[a];
				validArg = true;
			}
		}
		else if (argText == L"-h" || argText == L"--help")
		{
			wantHelp = true;
			validArg = true;
		}
		else if (argText == L"-v" || argText == L"--verbose")
		{
			wantVerbose = true;
			flags |= FLAG_VERBOSE;
			validArg = true;
		}
		else if (argText == L"--no-md5")
		{
			flags |= FLAG_NO_MD5;
			validArg = true;
		}
		else if (argText == L"--no-sha1")
		{
			flags |= FLAG_NO_SHA1;
			validArg = true;
		}
		else if (argText == L"--no-sha256")
		{
			flags |= FLAG_NO_SHA256;
			validArg = true;
		}
		else if (argText == L"--no-hash")
		{
			flags |= FLAG_NO_MD5 | FLAG_NO_SHA1 | FLAG_NO_SHA256;
			validArg = true;
		}

		if (!validArg)
		{
			wcerr << L"Invalid argument: \"" << argv[a] << L"\""
				<< endl;
			invalidArgs = true;
		}
	}

	if (!wantHelp && !invalidArgs)
	{
		if (inputDirName.empty())
		{
			wcerr << L"Missing input directory!" << endl;
			invalidArgs = true;
		}

		if (baseURL.empty())
		{
			wcerr << L"Missing base URL!" << endl;
			invalidArgs = true;
		}

		if (!country.empty() && country.size() != 2)
		{
			wcerr << "Invalid country code \"" << country << "\"!" << endl;
			invalidArgs = true;
		}

		if (outFileName.empty())
		{
			wcerr << L"Missing output filename!" << endl;
			invalidArgs = true;
		}
	}

	if (invalidArgs || wantHelp)
	{
		if (invalidArgs)
			wcout << L"\n";

		wcout << L"Usage:\n"
			<< L"\n"
			<< L" mlbuilder --help\n"
			<< L" mlbuilder --directory path --base-url url --output outfile [--country code] [--verbose]\n"
			<< L" mlbuilder -d directory-path -u base-url -o outfile [-c country-code] [-v]\n"
			<< L"\n"
			<< L"Example usage:\n"
			<< L"\n"
			<< L" mlbuilder -d ./MyMirror -u ftp://ftp.example.com -c us -o MyMirror.meta4\n"
			<< L"\n"
			<< L"Required Arguments:\n"
			<< L"\n"
			<< L" -h, --help - Show this screen\n"
			<< L" -d, --directory directory - The directory path to process\n"
			<< L" -u, --base-url base-url - The URL of the source directory\n"
			<< L" -o, --output - Output filename(.meta4 or .metalink)\n"
			<< L"\n"
			<< L"Optional Arguments:\n"
			<< L"\n"
			<< L" -c, --country country-code - ISO3166-1 alpha-2 two letter country code of the server specified by base-url above\n"
			<< L" -v, --verbose - Verbose output\n"
			<< L" --no-md5 - Don't calculate MD5\n"
			<< L" --no-sha1 - Don't calculate SHA-1\n"
			<< L" --no-sha256 - Don't calculate SHA-256\n"
			<< L" --no-hash - Don't calculate _any_ hashes" << endl;

		return invalidArgs ? EXIT_FAILURE : EXIT_SUCCESS;
	}

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

	replace(inputDirName.begin(), inputDirName.end(), L'\\', L'/');

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
	ProcessDir(inputDirName, L"", xmlRootNode, country, baseURL, currentDate, ctx, flags);

	auto endTick = GetTickCount64();
	if (wantVerbose)
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

