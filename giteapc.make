# giteapc: version=1 depends=Lephenixnoir/gint

-include giteapc-config.make

configure:
	@ fxsdk build-fx -c $(JUSTUI_CMAKEOPTS_FX)
	@ fxsdk build-cg -c $(JUSTUI_CMAKEOPTS_CG)

build:
	@ fxsdk build-fx
	@ fxsdk build-cg

install:
	@ fxsdk build-fx install
	@ fxsdk build-cg install

uninstall:
	@ if [ -e build-fx/install_manifest.txt ]; then \
	     xargs rm -f < build-fx/install_manifest.txt; \
          fi
	@ if [ -e build-cg/install_manifest.txt ]; then \
	     xargs rm -f < build-cg/install_manifest.txt; \
          fi

.PHONY: configure build install uninstall
