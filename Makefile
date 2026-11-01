# Makefile - seccomp syscall allow-list project (BMU1301 #018)
# Faz 0: only dependency check + whatever sources already exist get built.

CC       ?= gcc
CSTD     := -std=c11 -D_GNU_SOURCE
WARN     := -Wall -Wextra -Werror -Wshadow -Wconversion -Wpedantic
OPT      ?= -O2 -g
CFLAGS   += $(CSTD) $(WARN) $(OPT) -Isrc
SAN      := -fsanitize=address,undefined -fno-omit-frame-pointer

SECCOMP_CFLAGS := $(shell pkg-config --cflags libseccomp 2>/dev/null)
SECCOMP_LIBS   := $(shell pkg-config --libs libseccomp 2>/dev/null)

BIN := bin

# Programs are added here as their sources land (Faz 2).
PROGRAMLAR := $(patsubst src/%.c,$(BIN)/%,$(wildcard src/hedef.c src/sandbox.c)) \
              $(patsubst tools/%.c,$(BIN)/%,$(wildcard tools/*.c))

.PHONY: all kontrol sanitize test surum temiz

all: kontrol $(PROGRAMLAR)
	@echo "derleme tamam: $(words $(PROGRAMLAR)) program"

kontrol:
	@command -v $(CC) >/dev/null || { echo "HATA: $(CC) yok"; exit 1; }
	@command -v strace >/dev/null || { echo "HATA: strace yok (scripts/kur.sh)"; exit 1; }
	@pkg-config --exists libseccomp || { echo "HATA: libseccomp-dev yok -> sudo scripts/kur.sh"; exit 1; }

$(BIN):
	mkdir -p $@

$(BIN)/hedef: src/hedef.c src/ortak.h | $(BIN)
	$(CC) $(CFLAGS) -pthread -o $@ $< $(LDFLAGS)

$(BIN)/%: src/%.c src/ortak.h | $(BIN)
	$(CC) $(CFLAGS) $(SECCOMP_CFLAGS) -o $@ $< $(LDFLAGS) $(SECCOMP_LIBS)

$(BIN)/%: tools/%.c src/ortak.h | $(BIN)
	$(CC) $(CFLAGS) $(SECCOMP_CFLAGS) -o $@ $< $(LDFLAGS) $(SECCOMP_LIBS)

sanitize:
	$(MAKE) temiz
	$(MAKE) all OPT="-O1 -g $(SAN)" LDFLAGS="$(SAN)"

test: all
	@set -e; n=0; for t in test/test_*.sh; do \
	  [ -e "$$t" ] || continue; echo "== $$t"; bash "$$t"; n=$$((n+1)); done; \
	  echo "test tamam: $$n betik"

surum:
	@uname -a
	@$(CC) --version | head -1
	@if v=$$(pkg-config --modversion libseccomp 2>/dev/null); then echo "libseccomp $$v"; else echo "libseccomp-dev: yok"; fi
	@strace -V | head -1
	@ldd --version | head -1

temiz:
	rm -rf $(BIN)
