#!/bin/sh
# The migrator's fresh-world rates, with db() replaced by a stub.
#
# The block is small and the SQL in it is what a world starts on, so the four
# answers it can give are worth pinning: a fresh world takes the launcher's
# numbers, a world that has been set from the panel is never touched, a
# database that will not answer leaves everything alone, and nonsense in .env
# reads as 100 rather than as an SQL syntax error.
#
#   sh tests/playerbot_migrate_rates_test.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
APPLY="${1:-$HERE/../linux-port-mt2009/docker/mariadb/playerbot/apply.sh}"
[ -f "$APPLY" ] || { echo "no apply.sh at $APPLY" >&2; exit 2; }

BLOCK=$(sed -n '/^rate_ok() {/,/^fi$/p' "$APPLY")
[ -n "$BLOCK" ] || { echo "the rates block is not in $APPLY" >&2; exit 2; }

fails=0
run() { # run DESCRIPTION FLAGS_PRESENT EXP DROP YANG EXPECTED_SUBSTRING
    _desc="$1"; _count="$2"; _want="$6"
    _out=$(M2_RATE_EXP="$3" M2_RATE_DROP="$4" M2_RATE_YANG="$5" COUNT="$_count" \
        sh -c '
db() {
    case "$*" in
        *"SELECT COUNT(*)"*) [ "$COUNT" = x ] && return 1; printf "%s\n" "$COUNT"; return 0 ;;
        *CREATE*)            return 0 ;;
        *)                   printf "SQL %s\n" "$(printf "%s" "$*" | tr -s " \n" " ")"; return 0 ;;
    esac
}
'"$BLOCK" 2>&1)
    if printf '%s' "$_out" | grep -q -- "$_want"; then
        echo "ok   - $_desc"
    else
        echo "FAIL - $_desc"
        echo "       expected to find: $_want"
        printf '       got: %s\n' "$_out"
        fails=$((fails + 1))
    fi
}

no_sql() { # no_sql DESCRIPTION FLAGS_PRESENT
    _out=$(M2_RATE_EXP=300 M2_RATE_DROP=200 M2_RATE_YANG=150 COUNT="$2" \
        sh -c '
db() {
    case "$*" in
        *"SELECT COUNT(*)"*) [ "$COUNT" = x ] && return 1; printf "%s\n" "$COUNT"; return 0 ;;
        *CREATE*)            return 0 ;;
        *)                   printf "SQL %s\n" "$(printf "%s" "$*" | tr -s " \n" " ")"; return 0 ;;
    esac
}
'"$BLOCK" 2>&1)
    if printf '%s' "$_out" | grep -q 'REPLACE INTO'; then
        echo "FAIL - $1 (it wrote something)"
        printf '       got: %s\n' "$_out"
        fails=$((fails + 1))
    else
        echo "ok   - $1"
    fi
}

run "a fresh world takes the launcher's numbers" 0 300 200 150 \
    "experience 300%, item drops 200%, yang 150%"
run "the flags it writes carry them"             0 300 200 150 \
    "(0, 'mob_exp', '', 300)"
run "and the premium twins with them"            0 300 200 150 \
    "(0, 'mob_exp_buyer', '', 300)"
run "the panel's own table is filled too"        0 300 200 150 \
    "REPLACE INTO player.web_admin_rates"
no_sql "a world set from the panel is left alone" 1
no_sql "a database that will not answer changes nothing" x
run "a database that will not answer says so"    x 300 200 150 \
    "could not read the rate flags"
run "nonsense and empty values read as 100"      0 "abc" "" " 99999 " \
    "experience 100%, item drops 100%, yang 100%"

if [ "$fails" -ne 0 ]; then
    echo "$fails test(s) failed" >&2
    exit 1
fi
echo "all rates-block tests passed"
