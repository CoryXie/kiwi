# Copyright (C) 2009-2010 Alex Smith
#
# Kiwi is open source software, released under the terms of the Non-Profit
# Open Software License 3.0. You should have received a copy of the
# licensing information along with the source code distribution. If you
# have not received a copy of the license, please refer to the Kiwi
# project website.
#
# Please note that if you modify this file, the license requires you to
# ADD your name to the list of contributors. This boilerplate is not the
# license itself; please refer to the copy of the license you have received
# for complete terms.

import os
from utilities.toolchain.component import ToolchainComponent

class BinutilsComponent(ToolchainComponent):
	name = 'binutils'
	version = '2.21'
	source = [
		'http://ftp.gnu.org/gnu/binutils/binutils-' + version + '.tar.bz2',
	]
	patches = [
		('binutils-' + version + '-kiwi.patch', 'binutils-' + version, 1),
	]

	def build(self):
		os.mkdir('binutils-build')

		# Work out configure options to use.
		confopts  = '--prefix=%s ' % (self.manager.destdir)
		confopts += '--target=%s ' % (self.manager.target)
		confopts += '--disable-werror '
		# Note: If adding platforms which do not support gold, their
		# GCC target definition must add --no-copy-dt-needed-entries
		# to LINK_SPEC. This behaviour is the default with gold, but
		# the opposite with GNU ld.
		confopts += '--enable-gold '

		# Build and install it.
		self.execute('../binutils-%s/configure %s' % (self.version, confopts), 'binutils-build')
		self.execute('make -j%d' % (self.manager.makejobs), 'binutils-build')
		self.execute('make install', 'binutils-build')
