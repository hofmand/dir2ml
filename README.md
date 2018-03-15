# metalink-builder
A tool to create metalink ([RFC5854](https://tools.ietf.org/html/rfc5854)) files (`*.meta4`/`*.metalink`) from a directory structure.

This tool, `mlbuilder`, will create a single metalink file from a supplied directory and base URL.

---

## Usage:

**`mlbuilder --help`**

**`mlbuilder --directory`** *path* **`--base-url`** *url* **`--output`** *outfile* [**`--country`** *code*] [**`--verbose`**]

**`mlbuilder -d`** *directory-path* **`-u`** *base-url* **`-o`** *outfile* [**`-c`** *country-code*] [**`-v`**]

### Example usage:

`mlbuilder -d ./MyMirror -u ftp://ftp.example.com -c us -o MyMirror.meta4`

---

## Required Arguments:

**`-h`**, **`--help`** - Show this screen

**`-d`**, **`--directory`** *directory* - The directory path to process

**`-u`**, **`--base-url`** *base-url* - The URL of the source directory

**`-o`**, **`--output`** - Output filename (`.meta4` or `.metalink`)

## Optional Arguments:

**`-c`**, **`--country`** *country-code* - [ISO3166-1 alpha-2](https://datahub.io/core/country-list) two letter country code of the server specified by *base-url* above

**`-v`**, **`--verbose`** - Verbose output

**`--no-md5`** - Don't calculate MD5

**`--no-sha1`** - Don't calculate SHA-1

**`--no-sha256`** - Don't calculate SHA-256

**`--no-hash`** - Don't calculate *any* hashes

## Limitations ##
* Windows only but using only standard C/C++ (no MFC/.NET, etc.) so compiling for other operating systems shouldn't be an issue.
* Single threaded so `mlbuilder` is CPU-bound when the storage device is sufficiently fast.
* If you're processing a directory with millions of files, you may run out of memory because the XML file isn't written until the very end. If you run into this problem, please  [open an issue](issues).
* Only one *base-url* can be specified.
* Only MD5, SHA-1, and SHA-256 hashes are currently supported.

## Built With ##
* Microsoft Visual Studio 2015

## Authors
Derek Hofmann

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.