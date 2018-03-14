# metalink-builder
A tool to create and manipulate metalink ([RFC5854](https://tools.ietf.org/html/rfc5854)) files (`*.metalink`/`*.meta4`).

---

Example usage:

`mlbuilder -d ./MyMirror -u ftp://ftp.example.com -l us -o MyMirror.meta4`

---

Arguments:

[**-c** *country*] ([ISO3166-1 alpha-2](https://datahub.io/core/country-list) two letter country code)

**-d** *directory*

**-h** *(help)*

**-o** *output filename (.meta4)*

**-u** *base URL*

**[-v]** *(want verbose)*