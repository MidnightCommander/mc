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
`doc/man/ro`), and add the language to `ALL_DOC_LINGUAS` and `AC_CONFIG_FILES` in `configure.ac` and to
`DIST_SUBDIRS` in `doc/man/Makefile.am`.

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