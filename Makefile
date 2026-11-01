# Makefile - seccomp syscall allow-list project (BMU1301 #018)

CC       ?= gcc
CSTD     := -std=c11 -D_GNU_SOURCE
WARN     := -Wall -Wextra -Werror -Wshadow -Wconversion -Wpedantic
OPT      ?= -O2 -g
CFLAGS   += $(CSTD) $(WARN) $(OPT) -Isrc
SAN      := -fsanitize=address,undefined -fno-omit-frame-pointer

# libseccomp: system package if present, else the rootless copy in .deps
# (created by scripts/kur-rootsuz.sh, linked statically).
ifeq ($(shell pkg-config --exists libseccomp 2>/dev/null && echo yes),yes)
  SECCOMP_CFLAGS := $(shell pkg-config --cflags libseccomp)
  SECCOMP_LIBS   := $(shell pkg-config --libs libseccomp)
  SECCOMP_OK     := yes
else ifneq ($(wildcard .deps/usr/include/seccomp.h),)
  SECCOMP_CFLAGS := -I.deps/usr/include
  SECCOMP_LIBS   := .deps/usr/lib/x86_64-linux-gnu/libseccomp.a
  SECCOMP_OK     := yes
else
  SECCOMP_OK     := no
endif

BIN      ?= bin
PROGRAMLAR := $(BIN)/hedef $(BIN)/sandbox $(BIN)/profil $(BIN)/profil_uret $(BIN)/ihlal_testi $(BIN)/arch_testi
HDR      := src/ortak.h src/json_mini.h src/profil_oku.h

.PHONY: all kontrol asan test surum profil kirilma temiz

all: kontrol $(PROGRAMLAR)
	@echo "derleme tamam: $(words $(PROGRAMLAR)) program -> $(BIN)/"

kontrol:
	@command -v $(CC) >/dev/null || { echo "HATA: $(CC) yok"; exit 1; }
	@command -v strace >/dev/null || { echo "HATA: strace yok (sudo scripts/kur.sh)"; exit 1; }
	@[ "$(SECCOMP_OK)" = yes ] || { echo "HATA: libseccomp-dev yok -> 'sudo scripts/kur.sh' veya root'suz 'scripts/kur-rootsuz.sh'"; exit 1; }

$(BIN):
	mkdir -p $@

$(BIN)/hedef: src/hedef.c $(HDR) | $(BIN)
	$(CC) $(CFLAGS) -pthread -o $@ $< $(LDFLAGS)

$(BIN)/sandbox: src/sandbox.c $(HDR) | $(BIN)
	$(CC) $(CFLAGS) $(SECCOMP_CFLAGS) -o $@ $< $(LDFLAGS) $(SECCOMP_LIBS)

$(BIN)/profil: tools/profil.c $(HDR) | $(BIN)
	$(CC) $(CFLAGS) $(SECCOMP_CFLAGS) -o $@ $< $(LDFLAGS)

$(BIN)/profil_uret: tools/profil_uret.c $(HDR) | $(BIN)
	$(CC) $(CFLAGS) $(SECCOMP_CFLAGS) -o $@ $< $(LDFLAGS) $(SECCOMP_LIBS)

$(BIN)/ihlal_testi: tools/ihlal_testi.c $(HDR) | $(BIN)
	$(CC) $(CFLAGS) $(SECCOMP_CFLAGS) -o $@ $< $(LDFLAGS) $(SECCOMP_LIBS)

$(BIN)/arch_testi: tools/arch_testi.c $(HDR) | $(BIN)
	$(CC) $(CFLAGS) $(SECCOMP_CFLAGS) -o $@ $< $(LDFLAGS) $(SECCOMP_LIBS)

# ASan/UBSan build in bin/asan; runs only the tests that make sense under the
# sanitizer runtime (it uses extra syscalls, so sandboxed runs of the
# sanitized target are out of scope - see docs/03 R6).
asan:
	$(MAKE) BIN=bin/asan OPT="-O1 -g $(SAN)" LDFLAGS="$(SAN)" all
	BIN=bin/asan ASAN=1 bash test/test_profil.sh
	BIN=bin/asan ASAN=1 bash test/test_filtre.sh
	BIN=bin/asan ASAN=1 bash test/test_hata.sh
	@echo "asan/ubsan: temiz"

test: all
	@set -e; n=0; for t in test/test_*.sh; do \
	  echo "== $$t"; bash "$$t"; n=$$((n+1)); done; \
	  echo "test tamam: $$n betik"

# End-to-end pipeline: profile -> filter -> sandboxed run
profil: all
	bash scripts/calistir.sh

# Break tests (F5): kill -9, disk full (tmpfs), MemoryMax, CPUQuota -> olcum/kirilma-*.txt
kirilma: all
	for t in kill9 disk bellek cpu; do bash tools/kirilma.sh $$t | tee olcum/kirilma-$$t.txt; done

surum:
	@uname -a
	@$(CC) --version | head -1
	@if v=$$(pkg-config --modversion libseccomp 2>/dev/null); then echo "libseccomp $$v (sistem)"; \
	  elif [ -f .deps/usr/include/seccomp.h ]; then echo "libseccomp $$(grep -m1 -o 'Version: .*' .deps/usr/lib/x86_64-linux-gnu/pkgconfig/libseccomp.pc) (.deps, statik)"; \
	  else echo "libseccomp-dev: yok"; fi
	@strace -V | head -1
	@ldd --version | head -1

temiz:
	rm -rf bin
