#!/usr/bin/env python

import command

def execute():
	command.dir_create('/', 'usr')
	command.dir_create('/', 'bin')
	command.dir_create('/usr', 'share')
	command.dir_create('/usr', 'images')
	command.file_create('/usr/images', 'test1.png', 'This is not a PNG!')
	command.file_create('/usr/images', 'test2.png', 'This is also not a PNG!')
	command.file_write('/usr/images/test2.png', 'Overwritten file content.')
	command.file_write('/usr/images/test1.png', 'Appended content.', append=True)
	command.file_write('/usr/images/test1.png', 'And more appended content.', append=True)

