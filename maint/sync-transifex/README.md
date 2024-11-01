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