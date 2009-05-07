AC_DEFUN([AC_MC_VFS_SAMBA],
[

  dnl
  dnl Samba support
  dnl
  use_smbfs=
  AC_ARG_WITH(samba,
  	  [  --with-samba             Support smb virtual file system [[no]]],
	  [if test "x$withval" != "xno"; then
  		  AC_DEFINE(WITH_SMBFS, 1, [Define to enable VFS over SMB])
	          vfs_flags="$vfs_flags, smbfs"
		  use_smbfs=yes
  	  fi
  ])

  if test -n "$use_smbfs"; then
  #################################################
  # set Samba configuration directory location
  configdir="/etc"
  AC_ARG_WITH(configdir,
  [  --with-configdir=DIR     Where the Samba configuration files are [[/etc]]],
  [ case "$withval" in
    yes|no)
    #
    # Just in case anybody does it
    #
	AC_MSG_WARN([--with-configdir called without argument - will use default])
    ;;
    * )
	configdir="$withval"
    ;;
  esac]
  )
  AC_SUBST(configdir)

  AC_ARG_WITH(codepagedir,
    [  --with-codepagedir=DIR   Where the Samba codepage files are],
    [ case "$withval" in
      yes|no)
      #
      # Just in case anybody does it
      #
	AC_MSG_WARN([--with-codepagedir called without argument - will use default])
      ;;
    esac]
  )
  fi
])
