.SUFFIXES:

CC = riscv64-linux-gnu-gcc
AS = riscv64-linux-gnu-gcc
LD = riscv64-linux-gnu-gcc

COMMON_FLAGS = -march=rv32gc -mabi=ilp32
ASFLAGS = $(COMMON_FLAGS) -g -Wa,--gdwarf-2 -O0
LDFLAGS = $(COMMON_FLAGS) -static -nostdlib -nostartfiles -no-pie
CFLAGS = $(COMMON_FLAGS) -g -fno-stack-protector

QEMU = qemu-riscv32
GDB_CONFIG = gdb-riscv32-qemu-init

%.o: %.s
	$(AS) $(ASFLAGS) -c -o $@ $<

%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test/test: test/test.o intern.o reader.o test/nostd.o test/test.ld
	$(LD) $(LDFLAGS) -T test/test.ld test/test.o test/nostd.o intern.o reader.o -o $@

EXECUTABLE = test/test
debug: $(EXECUTABLE)
	$(QEMU) -g 1234 $(EXECUTABLE) input_file & \
	QEMU_PID=$$! ; \
	trap "kill $$QEMU_PID" EXIT INT TERM ; \
	gdb-multiarch -tui -x $(GDB_CONFIG) $(EXECUTABLE)
