# Add any other object files to this list below
APP_OBJS = main.o user.o flash.o framebuffer.o gendc.o

LIBGIGE = ../../libGigE/Arm64/Release/libgigev.so

DESTDIR = /opt/xilinx/xlnx-app-kr260-mv-defect-detect/

CFLAGS+=-I../../libGigE
CFLAGS+=-fPIC

LDFLAGS+=-L../../libGigE/Arm64/Release/

OUTDIR = $(DESTDIR)/usr/bin/
LIBDIR = $(DESTDIR)/usr/lib/

APPNAME = gvrd

all: $(APPNAME)

$(APPNAME): $(APP_OBJS)
	$(CC) $(LDFLAGS) $(APP_OBJS) $(LDLIBS) $(LIBGIGE) -o $(APPNAME) -lm -lpthread -lgigev

install:
	mkdir -p "$(OUTDIR)"
	cp Release/update_eeprom $(OUTDIR)
	cp Release/eeprom.bin $(OUTDIR)
	cp Release/update_atable $(OUTDIR)
	cp Release/zcip $(OUTDIR)
	cp Release/zcip.script $(OUTDIR)
	cp Release/alloc_table.bin $(OUTDIR)
	cp $(APPNAME) $(OUTDIR)
	mkdir -p "$(LIBDIR)"
	cp -a ../../libGigE/Arm64/Release/libgigev.so.2.0.1 $(LIBDIR)
	cp -a ../../libGigE/Arm64/Release/libgigev.so.2.0 $(LIBDIR)
	cp -a ../../libGigE/Arm64/Release/libgigev.so $(LIBDIR)

clean:
	-rm -f $(APPNAME) *.elf *.gdb $(APP_OBJS)

%.o: ../%.c
#	$(CC) -c $(CFLAGS) -o $(OUTDIR)/$@	
	$(CC) $(CFLAGS) -c $< -o $@
