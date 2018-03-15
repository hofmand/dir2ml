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

constexpr wchar_t* VERSION_NO = L"0.2.0";

//////////////////////////////////////////////////////////////////////////
//
// mlbuilder: read a directory structure and create a metalinks file.
//
//////////////////////////////////////////////////////////////////////////
//
// Arguments:
//
// [-c country] (ISO3166-1 2 two letter country code)
// -d directory
// -h (help)
// -o output filename (.meta4)
// -u base URL
// [-v] (verbose output)
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

enum PROCESS_DIR_FLAGS : process_dir_flags_t
{
	FLAG_WANT_VERBOSE = 0x0001
};

void ProcessDir( wstring const& inputBaseDirName,
                 wstring const& inputDirSuffixName,
                 pugi::xml_node& xmlRootNode,
                 std::wstring const& country,
                 std::wstring const& baseURL,
                 std::wstring const& currentDate,
                 process_dir_flags_t flags )
{
	bool wantVerbose = ((flags & FLAG_WANT_VERBOSE) != 0);

	wstring inputDirName = inputBaseDirName;
	if (!inputDirSuffixName.empty())
		inputDirName += L"/" + inputDirSuffixName;

	tinydir_dir inputDir;
	if (0 != tinydir_open_sorted(&inputDir, inputDirName.c_str()))
	{
		wcerr << L"Couldn't open input directory \"" << inputDirName
			<< L"\"!" << endl;
		return;
	}

	if(!wantVerbose)
		wcout << L"DIR: " << inputDirName << endl;

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
					country, baseURL, currentDate, flags);
			}
		}
		else
		{
			// <file name="example.ext">
			wstring filePathAbs;
			{
				wostringstream buf;
				buf << inputDirName << L"/" << file.name;
				filePathAbs = buf.str();
			}

			if (wantVerbose)
				wcout << L"  " << filePathAbs << endl;

			pugi::xml_node xmlFileNode = xmlRootNode.append_child(L"file");
			xmlFileNode.append_attribute(L"name")
				.set_value(filePathRel.c_str());

			// <size>14471447</size>
			ifstream inFile(filePathAbs, ifstream::ate | ifstream::binary);
			{
				xmlFileNode.append_child(L"size")
					.append_child(pugi::node_pcdata)
					.set_value(to_wstring(inFile.tellg()).c_str());
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
							MD5_Update(&ctxMD5, reinterpret_cast<const unsigned char*>(&fileBuffer[0]),
								bytesRead);

							s1.processBytes(&fileBuffer[0], bytesRead);

							sha256_update(&ctxSHA_256,
								reinterpret_cast<const unsigned char*>(&fileBuffer[0]),
								bytesRead);
						}
					}
				}

				// MD5
				// SHA-256
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
}

int wmain( int argc, wchar_t **argv )
{
	wstring inputDirName, baseURL, country, outFileName;

	bool invalidArgs(false);
	bool wantHelp(false);
	bool wantVerbose(false);

	wcout << L"mlbuilder v" << VERSION_NO << L"\n" << endl;

	for(int a = 1; a < argc; ++a)
	{
		if (wcslen(argv[a]) == 2 && argv[a][0] == L'-')
		{
			switch (argv[a][1])
			{
			case L'd':
				if (a < argc - 1)
				{
					++a;
					inputDirName = argv[a];
				}
				break;
			case L'u':
				if (a < argc - 1)
				{
					++a;
					baseURL = argv[a];
				}
				break;
			case L'c':
				if (a < argc - 1)
				{
					++a;
					country = argv[a];
				}
				break;
			case L'o':
				if (a < argc - 1)
				{
					++a;
					outFileName = argv[a];
				}
				break;
			case L'h':
				wantHelp = true;
				break;
			case L'v':
				wantVerbose = true;
				break;
			default:
				wcerr << L"Invalid argument: \"" << argv[a] << L"\""
					<< endl;
				invalidArgs = true;
				goto invalidargs;
			}
		}
		else
		{
			wcerr << L"Invalid argument: \"" << argv[a] << L"\"" << endl;
			invalidArgs = true;
		}
	}

	if (!wantHelp)
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

invalidargs:
	if (invalidArgs || wantHelp)
	{
		if (invalidArgs)
			wcout << L"\n";

		wcout << L"Arguments:\n"
			<< L"\t[-c country] (ISO3166-1 2 two letter country code)\n"
			<< L"\t-d directory\n"
			<< L"\t-h (help)\n"
			<< L"\t-o output filename (.meta4)\n"
			<< L"\t-u base URL\n"
			<< L"\t[-v] (want verbose)" << endl;

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
	process_dir_flags_t flags(0);
	if (wantVerbose)
		flags |= FLAG_WANT_VERBOSE;

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

	ProcessDir(inputDirName, L"", xmlRootNode, country, baseURL, currentDate, flags);

	auto endTick = GetTickCount64();
	if (wantVerbose)
	{
		wcout << L"Metalinks builder completed in "
			<< ((endTick - startTick) / 1000.) << L" seconds." << endl;
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

