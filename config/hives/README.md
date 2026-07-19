# Registry Hive DSL

These files are the source form of the initial otsos registry.
They are compiled with:

```sh
python3 tools/hivec.py -o bin/cmseed config/hives/*.hive
```

The output is a boot seed pack named `cmseed`.  Bootloaders treat it as an
opaque Multiboot module.  The future kernel `cm` subsystem will parse it.

## Syntax

```text
hive SYSTEM @access(read=user, add=kusr, edit=kusr) {
	key Network.Interfaces.eth0 @boot {
		bool Enabled = true
		ipv4 Address = 10.0.2.15
		string Name = "virtio-net0"
	}
}
```

Supported value types:

```text
string
bool
i32
u32
u64
ipv4
bytes
multi_string
```

Supported attributes:

```text
@protected
@volatile
@readonly
@boot
@access(read=user, add=kusr, edit=kusr)
```

`@access` supports only:

```text
read
add
edit
```

Subjects are only:

```text
user
kusr
```

Access is inherited from value to key, parent keys and hive defaults.
If a hive does not specify access explicitly, the default is
`read=user, add=kusr, edit=kusr`.  `regUpd()` is not a hive permission; CM
checks it separately.

## Binary Format

`cmseed` is an HPK pack:

```text
HPK header
HPK hive table
HIVE blob...
```

Each HIVE blob is table-based:

```text
HIVE header
node table
value table
string table
data heap
```

Everything is little-endian.  Headers carry version, size and CRC32 fields.
