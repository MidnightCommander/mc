# shellcheck shell=sh
export SHELL_FOR_TESTING="${SHELL_FOR_TESTING:-/bin/sh}"

setup_common() {
	HELPERS="$SELF_DIR/../../../../../src/vfs/shell/helpers"
	bin_dir=; get_temp bin_dir
	mock_bin echo expr ls tee wc
}

which() { _bin="$(type "$1")" && { _bin="${_bin##* }"; _bin="${_bin#\(}"; _bin="${_bin%\)}"; printf '%s' "$_bin"; } }

mock_bin() {
	for _arg; do
		ln -s "$(which "$_arg")" "$bin_dir/$_arg"
	done
}

setup_send_method() {
	case "$1" in
		nonposix_dd)
			if type dd >/dev/null 2>&1; then
				mock_bin dd
				HELPER_ENV="SHELL_HAVE_DD=1"
			else
				SKIP "dd not available"
				return 1
			fi
			;;
	esac
}

try_send() {
	# shellcheck disable=SC2086
	env \
		SHELL_FILENAME="${1#/}" \
		SHELL_FILESIZE="$2" \
		$HELPER_ENV \
		PATH="$bin_dir" \
		$SHELL_FOR_TESTING "$HELPERS/$HELPER_NAME"
}
