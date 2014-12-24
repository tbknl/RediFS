#!/usr/bin/env python

from connect import client as rc


# Create root dir:
def reset():
	rc.flushdb()
	rootDirId = rc.incr('curNodeId')
	rc.hmset('node:{}'.format(rootDirId), {'type': 'dir', 'ref': 1, 'mode': 0})
	rc.hmset('fsdata', {'rootDirId': rootDirId})
	load_scripts()


def load_scripts():
	fsdata = rc.hgetall('fsdata')

	# Helper functions:
	helpers = {}

	## Find node id:
	helpers['findNodeId'] = """
		local function findNodeId(path)
			local dirid = {rootDirId}
			for d in string.gmatch(path, '[^\/]+') do
				dirid = redis.call('hget', 'node:' .. dirid .. ':entries', d)
				if dirid == false then return false end
			end
			return dirid
		end
	""".format(rootDirId=fsdata['rootDirId'])

	## Find parent node id:
	helpers['findNodeIdExt'] = """
		local function findNodeIdExt(path)
			local dirid = {rootDirId}
			local parentid = 0
			local basename = path
			for d in string.gmatch(path, '[^\/]+') do
				basename = d
				parentid = dirid
				dirid = redis.call('hget', 'node:' .. dirid .. ':entries', d)
				if dirid == false then break end
			end
			return {{id=dirid, parentid=parentid, basename=basename}}
		end
	""".format(rootDirId=fsdata['rootDirId'])


	# Scripts
	script_sha = {}

	## Create dir:
	script_sha['dir_create'] = rc.script_load(
		helpers['findNodeId'] + """
		local dirid = redis.call('incr', 'curNodeId')
		redis.call('hmset', 'node:' .. dirid, 'type', 'dir', 'ref', 1, 'mode', 0)
		local parentid = findNodeId(ARGV[1])
		if parentid == false then return false end
		if redis.call('hget', 'node:' .. parentid, 'type') ~= 'dir' then return false end
		redis.call('hset', 'node:' .. parentid .. ':entries', ARGV[2], dirid)
		return dirid
	""")


	## Read dir:
	script_sha['dir_read'] = rc.script_load(
		helpers['findNodeId'] + """
		local dirid = findNodeId(ARGV[1])
		if dirid == false then return false end
		if redis.call('hget', 'node:' .. dirid, 'type') ~= 'dir' then return false end
		return redis.call('hkeys', 'node:' .. dirid .. ':entries')
	""")


	## Retrieve attributes:
	script_sha['getattr'] = rc.script_load(
		helpers['findNodeId'] + """
		local nodeid = findNodeId(ARGV[1])
		if nodeid == false then return false end
		local info = redis.call('hmget', 'node:' .. nodeid, 'type', 'mode')
		if info[1] == 'file' then
			info[3] = '' .. redis.call('strlen', 'node:' .. nodeid .. ':data')
		else
			info[3] = '0'
		end
		return info
	""")


	## Create file:
	script_sha['file_create'] = rc.script_load(
		helpers['findNodeId'] + """
		local fileid = redis.call('incr', 'curNodeId')
		redis.call('hmset', 'node:' .. fileid, 'type', 'file', 'ref', 1, 'mode', 0)
		redis.call('set', 'node:' .. fileid .. ':data', ARGV[3])
		local parentid = findNodeId(ARGV[1])
		if parentid == false then return false end
		if redis.call('hget', 'node:' .. parentid, 'type') ~= 'dir' then return false end
		redis.call('hset', 'node:' .. parentid .. ':entries', ARGV[2], fileid)
		return fileid
	""")


	## Open file:
	script_sha['file_open'] = rc.script_load(
		helpers['findNodeId'] + """
		local fileid = findNodeId(ARGV[1])
		if fileid == false then return -1234 end
		if redis.call('hget', 'node:' .. fileid, 'type') ~= 'file' then return -2345 end
		return fileid
	""")


	## Read file:
	script_sha['file_read'] = rc.script_load(
		helpers['findNodeId'] + """
		local fileid = findNodeId(ARGV[1])
		if fileid == false then return false end
		if redis.call('hget', 'node:' .. fileid, 'type') ~= 'file' then return false end
		return redis.call('get', 'node:' .. fileid ..':data')
	""")


	## Read file chunk by file id:
	script_sha['fileid_read_chunk'] = rc.script_load("""
		local fileid = ARGV[1]
		if fileid == false then return false end
		if redis.call('hget', 'node:' .. fileid, 'type') ~= 'file' then return false end
		if ARGV[2] ~= nil and ARGV[3] ~= nil then
			local offset = ARGV[2]
			local maxsize = ARGV[3]
			return redis.call('getrange', 'node:' .. fileid .. ':data', offset, offset + maxsize - 1)
		else
			return redis.call('get', 'node:' .. fileid ..':data')
		end
	""")


	## Write file:
	script_sha['file_write'] = rc.script_load(
		helpers['findNodeId'] + """
		local fileid = findNodeId(ARGV[1])
		if fileid == false then return false end
		if redis.call('hget', 'node:' .. fileid, 'type') ~= 'file' then return false end
		return redis.call('set', 'node:' .. fileid .. ':data', ARGV[2])
	""")


	## Append to file:
	script_sha['file_append'] = rc.script_load(
		helpers['findNodeId'] + """
		local fileid = findNodeId(ARGV[1])
		if fileid == false then return false end
		if redis.call('hget', 'node:' .. fileid, 'type') ~= 'file' then return false end
		return redis.call('append', 'node:' .. fileid .. ':data', ARGV[2])
	""")


	## Read file chunk by file id:
	script_sha['fileid_write_chunk'] = rc.script_load("""
		local fileid = ARGV[1]
		if fileid == false then return false end
		if redis.call('hget', 'node:' .. fileid, 'type') ~= 'file' then return false end
		if ARGV[2] ~= nil and ARGV[3] ~= nil then
			local offset = ARGV[2]
			local data = ARGV[3]
			redis.call('setrange', 'node:' .. fileid .. ':data', offset, data)
			return 1
		end
		return 0
	""")


	## Truncate file:
	script_sha['file_truncate'] = rc.script_load(
		helpers['findNodeId'] + """
		local fileid = findNodeId(ARGV[1])
		if fileid == false then return false end
		if redis.call('hget', 'node:' .. fileid, 'type') ~= 'file' then return false end
		local oldlen = redis.call('strlen', 'node:' .. fileid ..':data')
		local newlen = tonumber(ARGV[2])
		if newlen > oldlen then
			redis.call('setbit', 'node:'.. fileid ..':data', (newlen - 1) * 8, 0)
		elseif newlen == 0 then
			redis.call('set', 'node:'.. fileid ..':data', '')
		elseif newlen < oldlen then
			local newvalue = redis.call('getrange', 'node:'.. fileid .. ':data', 0, newlen - 1)
			redis.call('set', 'node:'.. fileid ..':data', newvalue)
		end
		return 1
	""")


	## Unlink:
	script_sha['unlink'] = rc.script_load(
		helpers['findNodeIdExt'] + """
		local file = findNodeIdExt(ARGV[1])
		if file.id == false then return false end
		if redis.call('hget', 'node:' .. file.id, 'type') ~= 'file' then return false end
		redis.call('hdel', 'node:' .. file.parentid .. ':entries', file.basename)
		if redis.call('hincrby', 'node:' .. file.id, 'ref', -1) <= 0 then
			redis.call('del', 'node:'.. file.id, 'node:'.. file.id ..':data')
		end
		return 1
	""")


	## Link:
	script_sha['link'] = rc.script_load(
		helpers['findNodeId'] + helpers['findNodeIdExt'] + """
		local fileid = findNodeId(ARGV[1])
		if fileid == false then return false end
		local destfile = findNodeIdExt(ARGV[2])
		if destfile.id ~= false then return -1 end
		if destfile.parentid == false then return -2 end
		if redis.call('hget', 'node:' .. destfile.parentid, 'type') ~= 'dir' then return -3 end
		redis.call('hset', 'node:'.. destfile.parentid ..':entries', destfile.basename, fileid)
		redis.call('hincrby', 'node:'.. fileid, 'ref', 1)
		return 1
	""")


	## Remove dir:
	script_sha['dir_remove'] = rc.script_load(
		helpers['findNodeIdExt'] + """
		local dir = findNodeIdExt(ARGV[1])
		if dir.id == false then return -1 end
		if redis.call('hget', 'node:'.. dir.id, 'type') ~= 'dir' then return -2 end
		if redis.call('hlen', 'node:'.. dir.id ..':entries') ~= 0 then return -3 end
		redis.call('hdel', 'node:' .. dir.parentid .. ':entries', dir.basename)
		redis.call('del', 'node:'.. dir.id, 'node:'.. dir.id ..':entries')
		return 1
	""")


	# Write script ids:
	rc.hmset('scripts', script_sha)

	print(script_sha)

