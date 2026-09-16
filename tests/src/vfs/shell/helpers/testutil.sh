# shellcheck shell=sh disable=SC2006,SC1091
set -eu

# shellcheck disable=SC2034
SELF_DIR=`cd "${SELF%/*}" && pwd`
temp_paths=
trap '${temp_paths:+rm -fr $temp_paths}' INT TERM EXIT
trap 'exit 124' ALRM

log() { printf '%s\n' "$*" 1>&2; }
abort() { log "$@"; exit 1; }
quote() { printf %s\\n "$1" | sed "s/'/'\\\\''/g;1s/^/'/;\$s/\$/'/"; }
set_var() { eval "$1=\"\$2\""; }

type timeout >/dev/null 2>&1 || timeout() { perl -e 'alarm shift; exec @ARGV' "$@"; }
[ ! -x /usr/xpg4/bin/grep ] || grep() { /usr/xpg4/bin/grep "$@"; }

run_test() {
	get_temp state_dir
	# shellcheck disable=SC2086
	timeout 4 "$SELF" $set_x run_inner "$state_dir" "$@" || {
		[ $? != 124 ] || log "FAIL: $*: the test has timed out, output: $(tail -1000 "$state_dir/output")"
		n_failed=$(( n_failed + 1 ))
	}
}

run_inner() {
	state_dir="$1"; shift
	run_name="$*"
	run_result=
	setup
	"$@"
	[ -z "${failed:-}" ]
}

get_temp() {
	_temp_path="$(mktemp -d)" || abort "mktemp failed"
	temp_paths="${temp_paths:+"$temp_paths "}$_temp_path"
	set_var "$1" "$_temp_path"
}

RUN() { set +e; "$@" 2>&1 | tee "$state_dir/output" >/dev/null; run_result=$?; set -e; run_cmd="$(quote "$*")"; }
FAIL() { log "FAIL: $run_name Command: ${run_cmd:-}; $1"; failed=true; }
SKIP() { log "SKIP: $run_name: $1"; }
assert_failure() { [ "$run_result" != 0 ] || FAIL "Expected: failure, actual: success"; }
assert_success() { [ "$run_result" = 0 ] || FAIL "Expected: success, actual: exit code $run_result"; }
assert_output() { diff_output=`printf '%s' "$1" | diff -u - "$state_dir/output" | tail -n +3` || FAIL "Expected output mismatch:
$diff_output"; }
assert_output_match() { grep -q "$@" "$state_dir/output" || FAIL "Output does not match '$*', actual: $(tail -50 "$state_dir/output")"; }
assert_output_nomatch() { grep -q "$@" "$state_dir/output" || return 0; FAIL "Output matches '$*': $(grep -m1 "$@" "$state_dir/output")"; }

header() {
	set_x=; [ "${1:-}" != "-x" ] || { set_x="$1"; shift; }
	n_failed=0
	[ $# = 0 ] || { "$@"; exit $?; }
}

summary() {
	printf '%d failed tests\n' "$n_failed"
	[ "$n_failed" = 0 ]
}
