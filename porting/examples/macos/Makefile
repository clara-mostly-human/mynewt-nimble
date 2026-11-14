# NimBLE host on macOS (64-bit, clang), HCI over TCP. Spike for zigbee-ble.
NIMBLE_ROOT := ../../..
APP ?= scan
SAN ?=
SANFLAGS := -fsanitize=undefined -fno-omit-frame-pointer   # ASan hangs in its own init on macOS 26.5 (AsanInitInternal spin lock)
NIMBLE_IGNORE := $(NIMBLE_ROOT)/porting/nimble/src/hal_timer.c \
	$(NIMBLE_ROOT)/porting/nimble/src/os_cputime.c \
	$(NIMBLE_ROOT)/porting/nimble/src/os_cputime_pwr2.c
include $(NIMBLE_ROOT)/porting/nimble/Makefile.defs
# macOS has no 32-bit userland: drop NimBLE's -m32
NIMBLE_CFLAGS :=
NIMBLE_LDFLAGS :=
SRC := $(NIMBLE_SRC) \
	$(wildcard $(NIMBLE_ROOT)/porting/npl/macos/src/*.c) \
	$(wildcard $(NIMBLE_ROOT)/porting/npl/macos/src/*.cc) \
	$(wildcard $(NIMBLE_ROOT)/nimble/transport/socket/src/*.c) \
	./$(APP).c ./main.c
INC := ./include $(NIMBLE_ROOT)/porting/npl/macos/include $(NIMBLE_ROOT)/nimble/transport/socket/include $(NIMBLE_INCLUDE)
CFLAGS := $(addprefix -I, $(INC)) -g -O1 -DMN_OSX \
	-DMYNEWT_VAL_BLE_SOCK_TYPE__linux_blue=0 -DMYNEWT_VAL_BLE_SOCK_TYPE__linux_tcp=1 \
	-DMYNEWT_VAL_BLE_SM_LEGACY=0 -DMYNEWT_VAL_BLE_SM_SC=0 \
	-Wno-unused-but-set-variable -Wno-gnu-variable-sized-type-not-at-end \
	$(if $(SAN),$(SANFLAGS))
OBJ := $(patsubst %.c,%.o,$(filter %.c,$(SRC))) $(patsubst %.cc,%.o,$(filter %.cc,$(SRC)))
all: nimble-$(APP)
%.o: %.c ; clang -c $(CFLAGS) -o $@ $< 2>>build-warnings.log
%.o: %.cc ; clang++ -std=c++17 -c $(CFLAGS) -o $@ $< 2>>build-warnings.log
nimble-$(APP): $(OBJ) ; clang++ $(if $(SAN),$(SANFLAGS)) -o $@ $^ -lpthread
clean: ; rm -f $(OBJ) nimble-scan nimble-connect nimble-macos build-warnings.log
