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


	# Write script ids:
	rc.hmset('scripts', script_sha)

	print(script_sha)

