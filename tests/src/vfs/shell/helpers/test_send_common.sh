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
		posix_dd)
			if dd if=/dev/zero bs=1 count=1 iflag=fullblock >/dev/null 2>&1; then
				cat >"$bin_dir/dd" <<-EOF_POSIX_DD
					#!/bin/sh
					echo "dd $*" >>"$state_dir/dd_runs"
					exec $(which dd) "\$@"
				EOF_POSIX_DD
				chmod 0755 "$bin_dir/dd"
				HELPER_ENV="SHELL_HAVE_POSIX_DD=1"
			else
				SKIP "POSIX dd not available"
				return 1
			fi
			;;
		head_c) if [ "$(echo abcd | { head -c 2 >/dev/null 2>&1; cat; })" = cd ]; then
				mock_bin head
				HELPER_ENV="SHELL_HAVE_HEAD=1"
			else
				SKIP "head -c not available"
				return 1
			fi
			;;
		perl)	if type perl >/dev/null 2>&1; then
				mock_bin perl
				HELPER_ENV="SHELL_HAVE_PERL=1"
			else
				SKIP "Perl not available"
				return 1
			fi
			;;
		nonposix_dd)
			if type dd >/dev/null 2>&1; then
				cat >"$bin_dir/dd" <<-EOF_NONPOSIX_DD
					#!/bin/sh
					case "\$*" in *fullblock*) exit 1 ;; esac
					echo "dd $*" >>"$state_dir/dd_runs"
					exec $(which dd) "\$@"
				EOF_NONPOSIX_DD
				chmod 0755 "$bin_dir/dd"
				HELPER_ENV="SHELL_HAVE_DD=1"
			else
				SKIP "dd not available"
				return 1
			fi
			;;
	esac
}

rand() {
	read -r _ _t _ <<EOF_RAND
$(od -N1 -tu1 /dev/random | head -1)
EOF_RAND
	while case "$_t" in 0[0-9]*) : ;; *) false ;; esac; do
		_t="${_t#0}"
	done
	printf "%s" "$(( 1 + _t ))"
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

try_send_with_jitter() {
	_rest="$3"
	while [ "$_rest" -gt 0 ]; do
		_size="$(rand)0"
		dd bs="$_size" count=1 2>/dev/null
		_rest=$((_rest - _size))
	done < "$1" | try_send "$2" "$3"
}
