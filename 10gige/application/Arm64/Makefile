# Add any other object files to this list below
APP_OBJS = main.o user.o flash.o framebuffer.o gendc.o

LIBGIGE = ../../libGigE/Arm64/Release/libgigev.so

CFLAGS+=-I../../libGigE
CFLAGS+=-fPIC

LDFLAGS+=-L../../libGigE/Arm64/Release/

OUTDIR = /usr/bin/
LIBDIR = /usr/lib/

$(shell test -d "$(OUTDIR)" || mkdir -p "$(OUTDIR)" )

APPNAME = $(OUTDIR)/gvrd

all: $(APPNAME)

$(APPNAME): $(APP_OBJS)
	$(CC) $(LDFLAGS) $(wildcard $(OUTDIR)/*.o) $(LDLIBS) $(LIBGIGE) -o $(APPNAME) -lm -lpthread -lgigev

install:
	cp Release/update_eeprom $(OUTDIR)
	cp Release/eeprom.bin $(OUTDIR)
	cp Release/update_atable $(OUTDIR)
	cp Release/zcip $(OUTDIR)
	cp Release/zcip.script $(OUTDIR)
	cp Release/alloc_table.bin $(OUTDIR)
	cp Release/gvrd $(OUTDIR)
	cp ../../libGigE/Arm64/Release/libgigev.so.2.0.1 $(LIBDIR)
	ln -sf /usr/lib/libgigev.so.2.0.1 /usr/lib/libgigev.so.2.0
	ln -sf /usr/lib/libgigev.so.2.0 /usr/lib/libgigev.so

clean:
	-rm -f $(APP) *.elf *.gdb $(OUTDIR)/*.o

%.o: ../%.c
#	$(CC) -c $(CFLAGS) -o $(OUTDIR)/$@	
	$(CC) $(CFLAGS) -c $< -o $(OUTDIR)/$@	
	

