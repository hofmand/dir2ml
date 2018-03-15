# metalink-builder
A tool to create metalink ([RFC5854](https://tools.ietf.org/html/rfc5854)) files (`*.meta4`/`*.metalink`) from a directory structure.

This tool, `mlbuilder`, will create a single metalink file from a supplied directory and base URL. The metalink file is an XML format so it can be rendered in a variety of ways using ordinary tools such as XML transformation utilities operating on XML stylesheets (`.xslt`).

---

## Usage:

**`mlbuilder --help`**

**`mlbuilder --directory`** *path* **`--base-url`** *url* **`--output`** *outfile* [**`--country`** *code*] [**`--verbose`**]

**`mlbuilder -d`** *directory-path* **`-u`** *base-url* **`-o`** *outfile* [**`-c`** *country-code*] [**`-v`**]

### Example usage:

`mlbuilder -d ./MyMirror -u ftp://ftp.example.com -c us -o MyMirror.meta4`

---

## Required Arguments:

**`-d`**, **`--directory`** *directory* - The directory path to process

**`-u`**, **`--base-url`** *base-url* - The URL of the source directory

**`-o`**, **`--output`** - Output filename (`.meta4` or `.metalink`)

## Optional Arguments:

**`-c`**, **`--country`** *country-code* - [ISO3166-1 alpha-2](https://datahub.io/core/country-list) two letter country code of the server specified by *base-url* above

**`-h`**, **`--help`** - Show this screen

**`-s`**, **`--show-statistics`** - Show statistics at the end of processing

**`-v`**, **`--verbose`** - Verbose output

**`--no-md5`** - Don't calculate MD5

**`--no-sha1`** - Don't calculate SHA-1

**`--no-sha256`** - Don't calculate SHA-256

**`--no-hash`** - Don't calculate *any* hashes

## Example Output File
```
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

## Limitations ##
* Windows only but the code uses only standard C/C++ (no MFC/.NET, etc.) so compiling for other operating systems should not be an issue.
* Single threaded so `mlbuilder` is CPU-bound when the storage device is sufficiently fast.
* If you're processing a directory with millions of files, you may run out of memory because the XML file isn't written until the very end. If you run into this problem, please  [open an issue](https://github.com/hofmand/metalink-builder/issues).
* Only one *base-url* can be specified.
* Only MD5, SHA-1, and SHA-256 hashes are currently supported.

## Built With ##
* Microsoft Visual Studio 2015

## Authors
Derek Hofmann

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.