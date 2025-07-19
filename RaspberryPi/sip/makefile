# =========  Makefile  ==================================================
# Kompiluje jedną z dwóch wersji – tylko wtedy, gdy jawnie o nią poprosisz:
#   make m32   -> build/sip_receiver_armhf   (32‑bit)
#   make m64   -> build/sip_receiver_aarch64 (64‑bit)
#   make clean    -> usuwa katalog build/
# -----------------------------------------------------------------------

### KONFIGUROWALNE ŚCIEŻKI ##############################################
SRC      := linphone_p2p_custom_devices.c       # pojedynczy plik źródłowy
OUTDIR   := build

# nagłówki i wspólne biblioteki
INC      := -I/usr/include -I/usr/include/linphone \
            -I/usr/include/mediastreamer2 -I/usr/include/ortp
LIBS     := -llinphone -lmediastreamer -lbctoolbox -lortp -lpthread -ldl -lm
CFLAGS   := -Wall -O2
#######################################################################

### ARMHF – 32 bit  #####################################################
CC_ARMHF        ?= arm-linux-gnueabihf-gcc
LDFLAGS_ARMHF    = -L/usr/lib/arm-linux-gnueabihf
BIN_ARMHF        = $(OUTDIR)/sip_receiver_armhf

### AARCH64 – 64 bit ####################################################
CC_AARCH64      ?= aarch64-linux-gnu-gcc
LDFLAGS_AARCH64  = -L/usr/lib/aarch64-linux-gnu
BIN_AARCH64      = $(OUTDIR)/sip_receiver_aarch64
#######################################################################

.PHONY: m32 m64 clean

$(OUTDIR):
	@mkdir -p $@

# ------------ 32‑bit build --------------------------------------------
$(BIN_ARMHF): $(SRC) | $(OUTDIR)
	$(CC_ARMHF) $(CFLAGS) $(INC) $< -o $@ $(LDFLAGS_ARMHF) $(LIBS)
	@echo "✔  zbudowano 32‑bit (armhf)  =>  $@"

# ------------ 64‑bit build --------------------------------------------
$(BIN_AARCH64): $(SRC) | $(OUTDIR)
	$(CC_AARCH64) $(CFLAGS) $(INC) $< -o $@ $(LDFLAGS_AARCH64) $(LIBS)
	@echo "✔  zbudowano 64‑bit (aarch64) =>  $@"

# Publiczne cele, o które wołasz w linii poleceń
m32: $(BIN_ARMHF)

m64: $(BIN_AARCH64)

clean:
	rm -rf $(OUTDIR)
# ======================================================================

