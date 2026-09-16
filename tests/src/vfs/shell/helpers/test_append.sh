#!/bin/sh
# shellcheck disable=SC2034,SC1091
SELF="$0"
# shellcheck disable=SC2164
SELF_DIR="$(cd "${0%/*}" 2>/dev/null; pwd)"
. "$SELF_DIR/testutil.sh"
. "$SELF_DIR/test_send_common.sh"

setup() {
	setup_common
	HELPER_NAME="append"
}

test_append_reports_errors() {
	# Test appending to a write-protected file

	setup_send_method "$1" || return 0
	dir=; get_temp dir
	echo "a" >"$dir/test.txt"
	chmod 0555 "$dir/test.txt" || abort "chmod failed"

	RUN try_send "$dir/test.txt" 5 <<EOF
test
EOF
	assert_output_match '^### 500'
}

header "$@"

# test every method that our script may choose
for method in nonposix_dd; do
	run_test test_append_reports_errors $method
done

summary
