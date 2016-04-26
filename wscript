#! /usr/bin/env python
# encoding: utf-8

def options(opt):
	opt.load('compiler_c')

def configure(conf):
	conf.load('ar')
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
	conf.env.append_value('CFLAGS', '-std=c99')
	conf.env.append_value('CFLAGS', '-march=native')

	conf.env.append_value('LIB_ZF', conf.env.LIB_Z + conf.env.LIB_BZ2)
	conf.env.append_value('DEFINES_ZF', conf.env.DEFINES_Z + conf.env.DEFINES_BZ2)
	conf.env.append_value('OBJ_ZF', ['zf.o', 'kopen.o'])


def build(bld):

	bld.objects(source = 'kopen.c', target = 'kopen.o')
	bld.objects(source = 'zf.c', target = 'zf.o')

	bld.stlib(
		source = ['unittest.c'],
		target = 'ptask',
		use = bld.env.OBJ_ZF,
		lib = bld.env.LIB_ZF,
		defines = bld.env.DEFINES_ZF)

	bld.program(
		source = ['unittest.c'],
		target = 'unittest',
		use = bld.env.OBJ_ZF,
		lib = bld.env.LIB_ZF,
		defines = ['TEST'] + bld.env.DEFINES_ZF)
