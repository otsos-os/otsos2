/* !DEFINES!

$define %type uint16_t as 16 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type int32_t as 32 bit signed
$define %type api_entity_create_args as native entity create descriptor
$define %type api_entity_stat as native entity stat descriptor
$define %type api_entity_entry as native entity list entry
$define %type api_entity_data as native entity data access descriptor
$define %type api_entity_list as native entity namespace list descriptor
$define %type api_entity_query as native entity query descriptor
$define %func entityCreateEx as function with args archetype, flags, access, name
$define %func entityCreate as function with args archetype, access, name
$define %func entityOpen as function with args const char *, uint32_t
$define %func entityClose as function with args int
$define %func entityDup as function with args int, uint32_t
$define %func entityStat as function with args int, api_entity_stat *
$define %func entityList as function with args path, entries, max
$define %func entityQuery as function with args archetype, start, entries, max
$define %func entityCtl as function with args int, uint32_t, void *
$define %func entityGetData as function with args int, uint32_t, uint64_t *
$define %func entitySetData as function with args int, uint32_t, uint64_t
$define %func entityGetI32 as function with args int, uint32_t, int32_t *
$define %func entitySetI32 as function with args int, uint32_t, int32_t
$define %func entityBind as function with args int, const char *
$define %func entityUnbind as function with args int
$define %func entityDelete as function with args int

*/

/* !SPACE!

$space %export entityCreateEx, entityCreate, entityOpen, entityClose
$space %export entityDup, entityStat, entityList, entityQuery, entityCtl
$space %export entityGetData, entitySetData, entityGetI32, entitySetI32
$space %export entityBind, entityUnbind, entityDelete

*/

#include <native.h>
#include <stdint.h>
#include "private.h"

int
entityCreateEx(uint16_t archetype, uint16_t flags, uint32_t access,
    const char *name)
{
	struct api_entity_create_args	args;

	args.archetype = archetype;
	args.flags = flags;
	args.access = access;
	args.name = name;
	return (__sysret_int(__syscall1(CALL_ENTITY_CREATE,
	    (long)&args)));
}

int
entityCreate(uint16_t archetype, uint32_t access, const char *name)
{
	return (entityCreateEx(archetype, 0, access, name));
}

int
entityOpen(const char *name, uint32_t access)
{
	return (__sysret_int(__syscall2(CALL_ENTITY_OPEN, (long)name,
	    (long)access)));
}

int
entityClose(int handle)
{
	return (__sysret_int(__syscall1(CALL_ENTITY_CLOSE,
	    (long)handle)));
}

int
entityDup(int handle, uint32_t access)
{
	return (__sysret_int(__syscall2(CALL_ENTITY_DUP, (long)handle,
	    (long)access)));
}

int
entityStat(int handle, struct api_entity_stat *stat)
{
	return (__sysret_int(__syscall2(CALL_ENTITY_STAT, (long)handle,
	    (long)stat)));
}

int
entityList(const char *path, struct api_entity_entry *entries,
    uint32_t max_entries)
{
	struct api_entity_list	list;
	long			ret;

	list.path = path;
	list.entries = entries;
	list.max_entries = max_entries;
	list.count = 0;
	ret = __syscall1(CALL_ENTITY_LIST, (long)&list);
	if (ret < 0) {
		return (__sysret_int(ret));
	}
	return ((int)list.count);
}

int
entityQuery(uint16_t archetype, uint32_t start,
    struct api_entity_entry *entries, uint32_t max_entries)
{
	struct api_entity_query	query;
	long			ret;

	query.archetype = archetype;
	query.pad = 0;
	query.start = start;
	query.entries = entries;
	query.max_entries = max_entries;
	query.count = 0;
	ret = __syscall1(CALL_ENTITY_QUERY, (long)&query);
	if (ret < 0) {
		return (__sysret_int(ret));
	}
	return ((int)query.count);
}

int
entityCtl(int handle, uint32_t op, void *arg)
{
	return (__sysret_int(__syscall3(CALL_ENTITY_CTL, (long)handle,
	    (long)op, (long)arg)));
}

int
entityGetData(int handle, uint32_t index, uint64_t *value)
{
	struct api_entity_data	data;
	int			ret;

	data.index = index;
	data.pad = 0;
	data.value = 0;
	ret = entityCtl(handle, ENTITY_CTL_GET_DATA, &data);
	if (ret != 0) {
		return (ret);
	}
	if (value) {
		*value = data.value;
	}
	return (0);
}

int
entitySetData(int handle, uint32_t index, uint64_t value)
{
	struct api_entity_data	data;

	data.index = index;
	data.pad = 0;
	data.value = value;
	return (entityCtl(handle, ENTITY_CTL_SET_DATA, &data));
}

int
entityGetI32(int handle, uint32_t index, int32_t *value)
{
	struct api_entity_data	data;
	int			ret;

	data.index = index;
	data.pad = 0;
	data.value = 0;
	ret = entityCtl(handle, ENTITY_CTL_GET_I32, &data);
	if (ret != 0) {
		return (ret);
	}
	if (value) {
		*value = (int32_t)data.value;
	}
	return (0);
}

int
entitySetI32(int handle, uint32_t index, int32_t value)
{
	struct api_entity_data	data;

	data.index = index;
	data.pad = 0;
	data.value = (uint64_t)(int64_t)value;
	return (entityCtl(handle, ENTITY_CTL_SET_I32, &data));
}

int
entityBind(int handle, const char *name)
{
	return (entityCtl(handle, ENTITY_CTL_BIND, (void *)name));
}

int
entityUnbind(int handle)
{
	return (entityCtl(handle, ENTITY_CTL_UNBIND, NULL));
}

int
entityDelete(int handle)
{
	return (entityCtl(handle, ENTITY_CTL_DELETE, NULL));
}
