# giteapc: version=1 depends=Lephenixnoir/gint

-include giteapc-config.make

configure:
	@ fxsdk build-fx -c $(JUSTUI_CMAKEOPTS_FX)
	@ fxsdk build-cg -c $(JUSTUI_CMAKEOPTS_CG)
	@ fxsdk build-cp -c $(JUSTUI_CMAKEOPTS_CP)

build:
	@ fxsdk build-fx
	@ fxsdk build-cg
	@ fxsdk build-cp

install:
	@ fxsdk build-fx install
	@ fxsdk build-cg install
	@ fxsdk build-cp install

uninstall:
	@ if [ -e build-fx/install_manifest.txt ]; then \
	     xargs rm -f < build-fx/install_manifest.txt; \
          fi
	@ if [ -e build-cg/install_manifest.txt ]; then \
	     xargs rm -f < build-cg/install_manifest.txt; \
          fi
	@ if [ -e build-cp/install_manifest.txt ]; then \
	     xargs rm -f < build-cp/install_manifest.txt; \
	  fi

.PHONY: configure build install uninstall
