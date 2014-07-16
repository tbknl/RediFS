#!/usr/bin/env python

from connect import client as rc


scripts = rc.hgetall('scripts')


def dir_create(path, dirname):
	return rc.evalsha(scripts['dir_create'], 0, path, dirname)


def dir_read(path):
	return rc.evalsha(scripts['dir_read'], 0, path)


def file_create(path, filename, data):
	return rc.evalsha(scripts['file_create'], 0, path, filename, data)


def file_read(path):
	return rc.evalsha(scripts['file_read'], 0, path)


def file_write(path, data, append=False):
	script = 'file_write' if not append else 'file_append'
	return rc.evalsha(scripts[script], 0, path, data)

