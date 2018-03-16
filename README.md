# metalink-builder
A tool to create metalink ([RFC5854](https://tools.ietf.org/html/rfc5854)) files (`.meta4`/`.metalink`) from a directory structure on local storage, complete with multiple cryptographic hashes.

This tool, `mlbuilder`, will create a single UTF-8 metalink file from a supplied directory and base URL, with one `file` record per file, sorted [ASCIIbetically](https://en.wiktionary.org/wiki/ASCIIbetical), directories first, depth-first. The metalink file is an XML format so it can be rendered in a variety of ways using ordinary tools such as XML transformation utilities operating on XML stylesheets (`.xslt`). See an [example](#example-output-file) of this file format below.

Later, when the original storage location of the files goes offline, the `.meta4` file can be used to identify and locate those files by hash on other servers or by P2P, and to reconstruct the original file structures.

Try it! Run `mlbuilder.exe` on your own computer's download directory, open the `.meta4` file it generates, and search on your favorite search engine for some of the hashes. *You might have the best luck with MD5 hashes of `.tar.gz` files but as metalinks become more ubiquitous, it will become easier to locate other lost files.*

Another use-case is to use `mlbuilder` to periodically [fingerprint](https://www.technologyreview.com/s/402961/fingerprinting-your-files/) your hard drive onto a USB flash drive *(use `--sparse-output` for that purpose unless your diff tool can ignore XML tags, and perhaps also disable all hash algorithms except SHA-256 in order to improve speed)*, then if your hard drive starts to crash or if you're hit by ransomware, you can use any ordinary diff tool to compare two `.meta4` files and easily determine which files have changed and need to be restored from [backup](https://www.backblaze.com/blog/the-3-2-1-backup-strategy/). (*You have a backup, right?*)

---

## Table of Contents
* [Usage](#usage)
* [Example Output File](#example-output-file)
* [Limitations](#limitations)
* [Future Plans](#future-plans)
* [Built With](#built-with)
* [Authors](#authors)
* [License](#license)

---

## Install

1. Clone this repository:

   `git clone https://github.com/hofmand/metalink-builder.git`

   or use your web browser to download the `.zip` file, then unpack it to the directory of your choice.

2. Compile `metalink-builder\src\metalink-tools.sln`

3. `mlbuilder.exe` will be located in `metalink-builder\src\x64\Release` or `metalink-builder\src\x64\Debug`

## Usage

**`mlbuilder --help`**

**`mlbuilder --directory`** *path* [**`--base-url`** *url*] **`--output`** *outfile* [**`--country`** *code*] [**`--verbose`**]

**`mlbuilder -d`** *directory-path* [**`-u`** *base-url*] **`-o`** *outfile* [**`-c`** *country-code*] [**`-v`**]

### Example usage:

`mlbuilder -d ./MyMirror -u ftp://ftp.example.com -c us -o MyMirror.meta4`

### Required Arguments:

**`-d`**, **`--directory`** *directory* - The directory path to process

**`-o`**, **`--output`** *outfile* - Output filename (`.meta4` or `.metalink`)

### Optional Arguments:

**`-c`**, **`--country`** *country-code* - [ISO3166-1 alpha-2](https://datahub.io/core/country-list) two letter country code of the server specified by *base-url* above

**`-h`**, **`--help`** - Show this screen

**`--version`** - Show version information

**`-s`**, **`--show-statistics`** - Show statistics at the end of processing

**`-u`**, **`--base-url`** *base-url* - The URL of the source directory. If this is omitted, the directory specified by `--directory` will be used, prepended by `file://`.

   Note: on Windows, backslashes (`\`) in the *base-url* will be replaced by forward slashes (`/`).

**`-v`**, **`--verbose`** - Verbose output

**`--no-md5`** - Don't calculate MD5

**`--no-sha1`** - Don't calculate SHA-1

**`--no-sha256`** - Don't calculate SHA-256

**`--no-hash`** - Don't calculate *any* hashes

**`--sparse-output`** - combines `--no-generator` and `--no-date` to simplify diffs

**`--no-generator`** - Don't output `<generator>..</generator>`

**`--no-date`** - Don't output `<updated>..</updated>`

## Example Output File
```xml
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink" xmlns:nsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="metalink4.xsd">
  <file name="example.ext">
    <size>14471447</size>
    <generator>mlbuilder/0.1.0</generator>
    <updated>2010-05-01T12:15:02Z</updated>
    <hash type="sha-256">17bfc4a6058d2d7d82db859c8b0528c6ab48d832fed620ed49fb3385dbf1684d</hash>
    <url location="us">ftp://ftp.example.com/example.ext</url>
  </file>
  <file name="subdir/example2.ext">
    <size>14471447</size>
    <generator>mlbuilder/0.1.0</generator>
    <updated>2010-05-01T12:15:02Z</updated>
    <hash type="sha-256">f44bcce2a9c2aa3f73ddc853ad98f87cd8e7cee5b5c18719ebb220da3fd4dbc9</hash>
    <url location="us">ftp://ftp.example.com/subdir/example2.ext</url>
  </file>
</metalink>
```

## Limitations
* **The SHA-1 and SHA-256 implementations do not correctly process large files >2GB.** I am working on this. Until this is fixed, if `mlbuilder` encounters a large file and either SHA-1 or SHA-256 processing is enabled, `mlbuilder` will fail and `exit(1)` without writing to *outfile*. The MD5 algorithm has no issues with large files.
* Windows only but the code uses only standard C/C++ (no MFC/.NET, etc.) so compiling for other operating systems should not be an issue.
* Single threaded so `mlbuilder` is CPU-bound, especially when the storage device is fast.
* `mlbuilder.exe` may run out of memory when processing a directory containing millions of files because the XML file isn't written until the very end. If you run into this problem, please  [open an issue](https://github.com/hofmand/metalink-builder/issues).
* Only one *base-url* can be specified.
* Only MD5, SHA-1, and SHA-256 hashes are currently supported.

## Future Plans
* Create `.magnet` links for each file
* Support multiple `country-code`/`base-url` pairs
* Deep-inspect archive files? (`.zip`, `.iso`, etc.)
* Output the `.meta4` file directly to a `.zip` / `.rar` / `.7z` container to save storage space.

## Built With
* Microsoft Visual Studio 2015; targeting x64, Unicode

## Authors
Derek Hofmann

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.