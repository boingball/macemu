# Makefile for BasiliskII with GAS-based asm_support.s (GCC-compatible)

# Compiler settings
CC = m68k-amigaos-gcc
CXX = m68k-amigaos-g++
CFLAGS = -g -O1 -noixemul -m68020 -msmall-code -Wno-multichar
CXXFLAGS = $(CFLAGS)
CPPFLAGS = -I../include -I../native_cpu -I.
LDFLAGS = -noixemul
LIBS = /gg/lib/libnix/swapstack.o

# VPATH so make can find files in parent dir
VPATH = .:..

# Sources
SRCS = \
    ../main.cpp main_amiga.cpp ../prefs.cpp ../prefs_items.cpp \
    prefs_amiga.cpp prefs_editor_amiga.cpp sys_amiga.cpp ../rom_patches.cpp \
    ../slot_rom.cpp ../rsrc_patches.cpp ../emul_op.cpp \
    ../macos_util.cpp ../xpram.cpp xpram_amiga.cpp ../timer.cpp \
    timer_amiga.cpp clip_amiga.cpp ../adb.cpp ../serial.cpp \
    serial_amiga.cpp ../ether.cpp ether_amiga.cpp ../sony.cpp ../disk.cpp \
    ../cdrom.cpp ../scsi.cpp scsi_amiga.cpp ../video.cpp video_amiga.cpp \
    ../audio.cpp audio_amiga.cpp ../extfs.cpp extfs_amiga.cpp \
    ../user_strings.cpp user_strings_amiga.cpp asm_support.s

# Output binary name
APP = BasiliskII

# Object directory
OBJ_DIR = obj
OBJS = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS:.cpp=.o)))
OBJS := $(OBJS:.c=.o)
OBJS := $(OBJS:.s=.o)

# Default target
all: $(OBJ_DIR) $(APP)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(APP): $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $(LIBS) $(OBJS)

# Compile rules using VPATH
$(OBJ_DIR)/%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(APP) $(OBJ_DIR)/*.o *~ *.bak

distclean: clean
	rm -rf $(OBJ_DIR)
