/* GNOME already handles the intenrationalization issues */
#ifndef _MC_I18N_H_
#define _MC_I18N_H_

#ifdef VFS_STANDALONE	/* We do not want vfs code to depend on internationalization, do we? */
#undef ENABLE_NLS
#endif

#ifdef HAVE_GNOME
#    define GNOME_REGEX_H
#    include <gnome.h>
#else
#    ifdef ENABLE_NLS
#    	 include <libintl.h>
#    	 define _(String) gettext (String)
#    	 ifdef gettext_noop
#    	     define N_(String) gettext_noop (String)
#    	 else
#    	     define N_(String) (String)
#    	 endif
#    else
         /* Stubs that do something close enough.  */
#    	 define textdomain(String) (String)
#    	 define gettext(String) (String)
#    	 define dgettext(Domain,Message) (Message)
#    	 define dcgettext(Domain,Message,Type) (Message)
#    	 define bindtextdomain(Domain,Directory) (Domain)
#    	 define _(String) (String)
#    	 define N_(String) (String)
#    endif
#endif

#endif /* _MC_I18N_H_ */
