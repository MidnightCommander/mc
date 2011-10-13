po4a_POT_FILES=mc.hint/mc.doc.pot

HINT_FILES=$(srcdir)/../mc.hint

CLEANFILES = $(po4a_POT_FILES)

pot: po4a-init tx-init tx-push-source

docs: hint

hint: tx-init tx-pull-po po4a-run

po4a-init: $(po4a_POT_FILES)

$(po4a_POT_FILES): $(HINT_FILES)
	BASE_FILENAME=$$(dirname $@) ;\
	mkdir -p $$BASE_FILENAME ;\
	po4a-gettextize -f text -m $(srcdir)/../$$BASE_FILENAME  -p $$BASE_FILENAME/mc.doc.pot

tx-init:
	for POT_FILE in $(po4a_POT_FILES); do \
	    BASE_FILENAME=$$(dirname $$POT_FILE) ;\
	    mkdir -p $$BASE_FILENAME/.tx ;\
	    if test ! -e $$BASE_FILENAME/.tx/config; then \
	        cp $(srcdir)/tx/$$BASE_FILENAME.config $$BASE_FILENAME/.tx/config ;\
	    fi ;\
	done

tx-push-source:
	for POT_FILE in $(po4a_POT_FILES); do \
	    BASE_FILENAME=$$(dirname $$POT_FILE) ;\
	    tx -r $$BASE_FILENAME push -s ;\
	done

tx-pull-po:
	for POT_FILE in $(po4a_POT_FILES); do \
	    BASE_FILENAME=$$(dirname $$POT_FILE) ;\
	    cd $$BASE_FILENAME ; tx pull --all --force; cd .. ;\
	done

po4a-run:
        
	for POT_FILE in $(po4a_POT_FILES); do \
	    BASE_FILENAME=$$(dirname $$POT_FILE) ;\
	    LANG=C po4a -k 0 -v --variable docfile=$$BASE_FILENAME po4a.cfg ;\
	done
