#!/bin/sh

file="$1"

if [ ! -f "$file" ]; then
	exit 0
fi

awk '
/^\[modules\]/ { in_section = 1; next }
/^\[/ { in_section = 0; next }
in_section {
	eq = index($0, "=")
	if (eq > 0) {
		k = substr($0, 1, eq - 1)
		gsub(/[ \t]/, "", k)
		if (k != "") {
			print k
		}
	}
}
' "$file"
