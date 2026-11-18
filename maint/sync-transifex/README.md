# Translations maintenance

## Requirements

* tx
    - Transifex client
    - https://developers.transifex.com/docs/cli
* po4a
    - Tool for converting translations between PO and other formats
    - https://po4a.org

## Configuration

First time you run `tx` command it will ask you for your API token and create `~/.transifexrc`.

## Maintenance

Check the `*-from-transifex.py` (run by hand) and `*-to-transifex.py` (used in CI) scripts.

Man pages are translated with po4a, one Transifex resource per page (`mc.1`, `mcedit.1`, `mcview.1`,
`mcdiff.1`; see `config.d`). `man-to-transifex.py` (used in CI) uploads the source strings, and
`man-from-transifex.py` (run by hand) pulls the translations and generates `doc/man/LANG/*.1.in`:

```shell
./maint/sync-transifex/man-from-transifex.py ro
```

The languages have to be named explicitly, because the man pages of the other languages are still maintained by
hand and must not be overwritten. To add a new language, also create `doc/man/LANG/Makefile.am` (copy it from
`doc/man/ro`) and `doc/hlp/LANG` (`Makefile.am` and a translated `xnc.hlp`, see `doc/hlp/ro`), add the language to
`ALL_DOC_LINGUAS` and both Makefiles to `AC_CONFIG_FILES` in `configure.ac`, and add the language to `DIST_SUBDIRS`
in `doc/man/Makefile.am` and `doc/hlp/Makefile.am`.

The built-in help is generated from `mc.1` by `src/man2hlp`, which reads its links and nodes from roff comments
(`.\"LINK2"`, `.\"NODE "..."` etc.). po4a would move these comments around, so before extracting the strings they
are turned into macros and inline markers, and turned back into comments in the generated pages (see
`translation_utils.py`). Translators see a link as `\X'mclink:Target'link text\X'mclink'`: the link text is
translated, and the marker and the target must be kept as they are.

The Transifex resources `mc-man--master`, `mcedit-man--master`, `mcview-man--master` and `mcdiff-man--master`
must exist in the project before the first upload.

Wrapper for modern Transifex client:

```shell
#!/bin/sh

touch ~/.transifexrc

export XUID=$(id -u)
export XGID=$(id -g)

docker run \
    --rm -i \
    --user $XUID:$XGID \
    --volume="/etc/group:/etc/group:ro" \
    --volume="/etc/passwd:/etc/passwd:ro" \
    --volume="/etc/shadow:/etc/shadow:ro" \
    --volume $(pwd):/app \
    --volume ~/.transifexrc:/.transifexrc \
    --volume /etc/ssl/certs/ca-certificates.crt:/etc/ssl/certs/ca-certificates.crt \
    transifex/txcli \
    --root-config /.transifexrc \
    "$@"
```