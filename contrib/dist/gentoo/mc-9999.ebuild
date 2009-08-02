# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=2
inherit autotools flag-o-matic git

DESCRIPTION="GNU Midnight Commander is a text mode file manager."
HOMEPAGE="http://midnight-commander.org/"
EGIT_REPO_URI="git://midnight-commander.org/git/mc.git"

LICENSE="GPL-2"
SLOT="0"

KEYWORDS=""

IUSE="+background +editor ext2undel gpm +network nls samba +vfs X +slang +charset"

RDEPEND=">=dev-libs/glib-2
	ext2undel? ( sys-fs/e2fsprogs )
	gpm? ( sys-libs/gpm )
	slang? ( >=sys-libs/slang-2.1.3 )
	!slang? ( sys-libs/ncurses )
	X? ( x11-libs/libX11
		x11-libs/libICE
		x11-libs/libXau
		x11-libs/libXdmcp
		x11-libs/libSM )"

DEPEND="${RDEPEND}
	nls? ( sys-devel/gettext )
	dev-util/pkgconfig"

# needed only for SCM source tree (autopoint uses it)
DEPEND="${DEPEND} dev-util/cvs"

src_unpack() {
	git_src_unpack
	cd "${S}"
	./autogen.sh
}

src_configure() {
	# check for conflicts (currently doesn't compile with --without-vfs)
	use vfs || {
		use network || use samba && \
		die "VFS is required for network or samba support."
	}

	local myconf=""
	# TODO: make sure we really need it (bundled samba?)
	append-flags "-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"

	if use samba; then
		myconf+=" --with-samba --with-configdir=/etc/samba --with-codepagedir=/var/lib/samba/codepages"
	else
		myconf+=" --without-samba"
	fi

	if use slang; then
		# slang is known to work better on various systems
		myconf+=" --with-screen=slang"
	else
		myconf+=" --with-screen=ncurses"
	fi

	econf --disable-dependency-tracking \
			$(use_enable background) \
			$(use_enable network netcode) \
			$(use_enable nls) \
			$(use_enable charset) \
			$(use_with editor edit) \
			$(use_with ext2undel) \
			$(use_with gpm gpm-mouse) \
			$(use_with vfs) \
			$(use_with X x) \
			${myconf}
}

src_compile() {
	emake || die "emake failed."
}

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed."
	dodoc doc/AUTHORS doc/COPYING doc/NEWS doc/README* doc/FAQ doc/HACKING doc/MAINTAINERS doc/TODO

	# Install cons.saver setuid to actually work
	# for more actual info see mc/src/cons.saver.c
	fowners tty:tty /usr/libexec/mc/cons.saver
	fperms u+s /usr/libexec/mc/cons.saver
}
