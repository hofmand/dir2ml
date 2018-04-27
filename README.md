# dir2ml
A command line tool to create metalink ([RFC5854](https://tools.ietf.org/html/rfc5854)) files (`.meta4`/`.metalink`) from a directory structure on local storage, complete with multiple cryptographic hash options. The latest binary release for Windows x86 and x64 can be found [here](https://github.com/hofmand/dir2ml/releases).

This tool, `dir2ml`, will create a single UTF-8 metalink file from a supplied directory and base URL, with one `file` record per file, sorted case-sensitive [ASCIIbetically](https://en.wiktionary.org/wiki/ASCIIbetical) by path to simplify diffs. The metalink file is an XML format so it can be rendered in a variety of ways using ordinary tools such as XML transformation utilities operating on XML stylesheets (`.xslt`). See an [example](#example-output-file) of this file format below.

Later, when the original storage location of the files goes offline, the `.meta4` file can be used to identify and locate those files by hash on other servers or by P2P, and to reconstruct the original file structures.

Try it! Run `dir2ml.exe` on your own computer's download directory, open the `.meta4` file it generates, and search on your favorite search engine for some of the hashes. *You might have the best luck with MD5 hashes of `.tar.gz` files but as metalinks become more ubiquitous, it will become easier to locate other lost files.*

Another use-case is to use `dir2ml` to periodically [fingerprint](https://www.technologyreview.com/s/402961/fingerprinting-your-files/) your hard drive onto a USB flash drive *(I recommend you use `--sparse-output --file-url` for that purpose)*, then if your hard drive starts to crash or if you're hit by ransomware, you can use any ordinary diff tool to compare two `.meta4` files and easily determine which files have changed and need to be restored from [backup](https://www.backblaze.com/blog/the-3-2-1-backup-strategy/). (*You have a backup, right?*)

Also included are a schema file (`metalink4.xsd` copied from [here](https://github.com/antbryan/www/blob/master/schema/4.0/metalink4.xsd)) and a stylesheet (`dfxml2meta4.xslt`) that converts from a Digital Forensics XML file to `.meta4`.

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

   `git clone --recursive https://github.com/hofmand/metalink-builder.git`

   or use your web browser to download the `.zip` file, then unpack it to the directory of your choice.

2. Compile `metalink-builder\src\metalink-tools.sln`

3. `dir2ml.exe` will be located in `metalink-builder\src\x64\Release` or `metalink-builder\src\x64\Debug`

## Usage

### Show Help

**`dir2ml --help`**

### Minimal Parameters

**`dir2ml -d`** *path* **`-o`** *outfile* { `-f` | `--file-url` | `-u` | `--base-url` | `--ni-url` }

### Example usage:

`dir2ml -d ./MyMirror -u ftp://ftp.example.com -o MyMirror.meta4`

### Required Arguments:

**`-d`**, **`--directory`** *directory-path* - The directory path to process

**`-o`**, **`--output`** *outfile* - Output filename (`.meta4` or `.metalink`)

If at least one of `-u`/`--base-url`, `-f`/`--file-url`, or `--ni-url` must be supplied.

### Optional Arguments:

**`-c`**, **`--country`** *country-code* - [ISO3166-1 alpha-2](https://datahub.io/core/country-list) two letter country code of the server specified by *base-url* above

**`-h`**, **`--help`** - Show this screen

**`--version`** - Show version information

**`-s`**, **`--show-statistics`** - Show statistics at the end of processing

**`-u`**, **`--base-url`** *base-url* - The base/root URL of an online directory containing the files. For example, `ftp://ftp.example.com`. `dir2ml` will append the relative path of each file to the *base-url*.

**`-f`**, **`--file-url`** - Add a local source for the file, using the directory specified by `--directory` prepended by `file://`. This is useful for fingerprinting a directory or hard drive.

   Note: on Windows, backslashes (`\`) in the *base-url* will be replaced by forward slashes (`/`).

**`-v`**, **`--verbose`** - Verbose output to `stdout`

**`--hash-type`** *hash-list* - Calculate and output all of the hashes specified by *hash-list* (comma-separated). Available hash functions are `md5`, `sha1`, `sha256`, and `all`. If none are specified, `sha256` is used.

**`--find-duplicates`** - Add URLs from all duplicate files to *each* matching metalink `file` node. This **preserves the directory structure** (as does having neither `--find-duplicates` nor `--consolidate-duplicates` flags turned on) and **allows the directory structure to be rebuilt using identical files from directories other than the original directory**, but results in large `.meta4` file sizes and takes more time than `--consolidate-duplicates`. Use `--ignore-file-dates` in conjunction with `--find-duplicates` to reduce the `.meta4` file size **if preserving file dates is not important**.

**`--consolidate-duplicates`** - Add duplicate URLs from all duplicate files to the *first* matching metalink `file` node and remove the other matching `file` nodes. **This does *not* perfectly preserve the directory structure.** Running `dir2ml` with this flag turned on takes more time than without the flag but results in the smallest possible `.meta4` file sizes. Add `--ignore-file-dates` to consolidate duplicate files further by ignoring file modification times.

**`--ignore-file-dates`** - Ignore file "last modified" dates when finding or consolidating duplicates. **This will *not* preserve file dates.** Turn this flag on when you care more about keeping the `.meta4` file small than about preserving file dates. Requires `--find-duplicates` or `--consolidate-duplicates`.

**`--ni-url`** - Output Named Information ([RFC6920](https://tools.ietf.org/html/rfc6920)) links (experimental). Requires `--hash-type sha256`

## Example Output File
```xml
<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink" xmlns:nsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="metalink4.xsd">
  <generator>dir2ml/0.1.0</generator>
  <published>2010-05-01T12:15:02Z</published>
  <file name="example.ext">
    <size>14471447</size>
    <hash type="sha-256">17bfc4a6058d2d7d82db859c8b0528c6ab48d832fed620ed49fb3385dbf1684d</hash>
    <url location="us" type="ftp">ftp://ftp.example.com/example.ext</url>
  </file>
  <file name="subdir/example2.ext">
    <size>14471447</size>
    <hash type="sha-256">f44bcce2a9c2aa3f73ddc853ad98f87cd8e7cee5b5c18719ebb220da3fd4dbc9</hash>
    <url location="us" type="ftp">ftp://ftp.example.com/subdir/example2.ext</url>
  </file>
</metalink>
```

## Limitations
* Windows only but the code uses only standard C/C++ (no MFC/.NET, etc.) so porting to other operating systems should not be a major task.
* Single threaded so `dir2ml` is CPU-bound, especially when the storage device is fast.
* `dir2ml.exe` may run out of memory when processing a directory containing millions of files because the XML file isn't written until the very end. If you run into this problem, please  [open an issue](https://github.com/hofmand/metalink-builder/issues).
* Only one *base-url* can be specified.
* Only MD5, SHA-1, and SHA-256 hashes are currently supported. *(Do we need any others?)*

## Future Plans
* Port the code to Linux/BSD
* Generate additional metadata from [Image::ExifTool](http://owl.phy.queensu.ca/~phil/cpp_exiftool/), [uchardet](https://github.com/BYVoid/uchardet), etc.
* Add `.torrent` files to `<metaurl>` subnodes
* Filter by file size, type, etc.
* Provide a means to merge and split `.meta4` files
* Multithreaded hashing
* Support multiple `country-code`/`base-url` pairs
* Deep-inspect archive files? (`.zip`, `.iso`, etc.)
* Output the `.meta4` file directly to a `.zip` / `.rar` / `.7z` container, in order to save storage space
* Convert [QuickHash](https://quickhash-gui.org/) output files to metalink files if possible (unless the developer decides to [output metalink files directly](https://quickhash-gui.org/bugs/output-in-rfc5854-format/))
* Add [xxHash](https://github.com/Cyan4973/xxHash) and/or [FarmHash](https://github.com/google/farmhash) algorithms
* Import from [SFV](https://en.wikipedia.org/wiki/Simple_file_verification) format.

## Also See
* [Wikipedia: Comparison of file verification software](https://en.wikipedia.org/wiki/Comparison_of_file_verification_software)
* [Corz Checksum](http://corz.org/windows/software/checksum/) - a Windows file hashing application (call `checksum.exe crs1` *directory-path* to get similar output to `dir2ml.exe --file-url --hash-type sha1 --directory` *directory-path* `--output` *outfile*)
  * *NB: `checksum.exe` processes files before subdirectories.*
* [Hash Archive](https://hash-archive.org/) - a database of file hashes (Linux `.iso` files, etc.).
* [HashDeep](http://md5deep.sourceforge.net/) - a hashing utility that can output to Digital Forensics XML format (call `hashdeep64 -r -j0 -c sha256 -d` *directory-path* `>` *outfile* to get similar output to `dir2ml.exe --file-url --hash-type sha256 --directory` *directory-path* `--output` *outfile*)
  * *NB: `hashdeep` sorts the output in a non-trivial manner.*
* [HashMyFiles](https://www.nirsoft.net/utils/hash_my_files.html) - an application similar to `dir2ml`.
* [niemandsland](https://github.com/wiedi/niemandsland) - named information (NI, RFC6920) exchange.
* [OpenTimestamps](https://opentimestamps.org/) - a service to store hashes in the blockchain.
* [Redump.org](http://redump.org/) - a database of hashes of computer/console game dumps.
* [RHash](https://github.com/rhash/RHash) - another hashing utility.

## Built With
* Microsoft Visual Studio 2015; targeting x64, Unicode

## Authors
Derek Hofmann

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.