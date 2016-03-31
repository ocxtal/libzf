#! /usr/bin/env python
# encoding: utf-8

def options(opt):
	opt.load('compiler_c')

def configure(conf):
	conf.load('compiler_c')
	if 'LIB_Z' not in conf.env:
		conf.check_cc(
			lib = 'z',
			defines = ['HAVE_Z'],
			mandatory = False)

	if 'LIB_BZ2' not in conf.env:
		conf.check_cc(
			lib = 'bz2',
			defines = ['HAVE_BZ2'],
			mandatory = False)

	conf.env.append_value('CFLAGS', '-O3')
	conf.env.append_value('CFLAGS', '-march=native')


def build(bld):
	bld.stlib(
		source = ['zf.c', 'kopen.c'],
		target = 'zf',
		lib = bld.env.LIB_Z + bld.env.LIB_BZ2,
		defines = bld.env.DEFINES_Z + bld.env.DEFINES_BZ2)

	bld.program(
		source = ['zf.c', 'kopen.c'],
		target = 'unittest',
		lib = bld.env.LIB_Z + bld.env.LIB_BZ2,
		defines = ['TEST'] + bld.env.DEFINES_Z + bld.env.DEFINES_BZ2)
