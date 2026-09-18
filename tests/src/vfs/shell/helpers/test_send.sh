#!/bin/sh
# shellcheck disable=SC2034,SC1091
SELF="$0"
# shellcheck disable=SC2164
SELF_DIR="$(cd "${0%/*}" 2>/dev/null; pwd)"
. "$SELF_DIR/testutil.sh"
. "$SELF_DIR/test_send_common.sh"

setup() {
	setup_common
	HELPER_NAME="send"
}

test_send_reports_errors() {
	# Test writing to a write-protected directory

	setup_send_method "$1" || return 0
	dir=; get_temp dir
	chmod 0555 "$dir" || abort "chmod failed"

	RUN try_send "$dir/test.txt" 5 <<EOF
test
EOF
	assert_output_match '^### 500'
}

test_send_short_reads() {
	# Test if short reads resulting from network congestion don't affect us

	setup_send_method "$1" || return 0
	dir=; get_temp dir
	dd if=/dev/random bs=1000 count=70 of="$dir/source.rnd" 2>/dev/null

	RUN try_send_with_jitter "$dir/source.rnd" "$dir/test.rnd" 70000
	assert_output_match '^### 200'
	dd_runs="$(if [ -f "$state_dir/dd_runs" ]; then wc -l <"$state_dir/dd_runs"; else echo 0; fi)"
	if [ "$dd_runs" -gt 3 ]; then
		log "NOTE: $run_name: dd had to run $dd_runs times, probably due to partial reads"
	fi

	RUN wc -c < "$dir/test.rnd"
	assert_output_match '^[[:space:]]*70000$'

	RUN cmp "$dir/source.rnd" "$dir/test.rnd"
	assert_success
	assert_output ''
}

header "$@"

# test every method that our script may choose
for method in posix_dd head_c perl nonposix_dd; do
	run_test test_send_reports_errors $method
	run_test test_send_short_reads $method
done

summary
