#!/bin/sh

TESTFILE="testcases.txt"
PASS=0
TOTAL=0
case=""
input=""
expected=""

run_test() {
    TOTAL=$((TOTAL + 1))
    actual=$(printf '%s' "$input" | ./lisp stdin)
    if [ "$actual" = "$expected" ]; then
        echo "$case OK"
        PASS=$((PASS + 1))
    else
        echo "$case FAIL"
        echo "  input:    $input"
        echo "  expected: $expected"
        echo "  actual:   $actual"
    fi
}

while IFS= read -r line; do
    case "$line" in
        '#'*)  ;;
        'case '*)
            case="${line#case }"
            ;;
        # note newline in var expansion
        '> '*) input="${input:+$input
}${line#> }" ;;
        '')
            if [ -n "$input" ]; then
            run_test
            case=""
            input=""
            expected=""
            fi
            ;;
        # note newline in var expansion
        *) expected="${expected:+$expected
}$line" ;;
    esac
done < "${1:-$TESTFILE}"

[ -n "$input" ] && run_test

echo "$PASS/$TOTAL passed"
[ $PASS -eq $TOTAL ]
