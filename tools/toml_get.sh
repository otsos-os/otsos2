file="$1"
section="$2"
key="$3"
default="$4"

if [ ! -f "$file" ]; then
	echo "$default"
	exit 0
fi

val=$(awk -v section="$section" -v key="$key" '
BEGIN { in_section = 0 }
/^\[.*\]/ {
	s = $0
	sub(/^\[/, "", s)
	sub(/\].*$/, "", s)
	gsub(/[ \t]/, "", s)
	in_section = (s == section) ? 1 : 0
	next
}
in_section {
	eq = index($0, "=")
	if (eq > 0) {
		k = substr($0, 1, eq - 1)
		gsub(/[ \t]/, "", k)
		if (k == key) {
			v = substr($0, eq + 1)
			sub(/[ \t]*#.*$/, "", v)
			gsub(/^[ \t]+|[ \t]+$/, "", v)
			gsub(/^"|"$/, "", v)
			print v
			exit
		}
	}
}
' "$file")

if [ -n "$val" ]; then
	echo "$val"
else
	echo "$default"
fi
