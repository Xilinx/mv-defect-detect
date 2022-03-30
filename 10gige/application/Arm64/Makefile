# Add any other object files to this list below
APP_OBJS = main.o user.o flash.o framebuffer.o gendc.o

LIBGIGE = ../../libGigE/Arm64/Release/libGigE.a

CFLAGS+=-I../../libGigE
CFLAGS+=-fPIC

LDFLAGS+=-L../../libGigE/Arm64/Release/

OUTDIR = Release

$(shell test -d "$(OUTDIR)" || mkdir -p "$(OUTDIR)" )

APPNAME = $(OUTDIR)/gvrd


#all: build install
#all: build

#build: $(APP)
all: $(APPNAME)

$(APPNAME): $(APP_OBJS)
	$(CC) $(LDFLAGS) $(wildcard $(OUTDIR)/*.o) $(LDLIBS) $(LIBGIGE) -o $(APPNAME) -lm -lpthread -lGigE
	cp ../xgvrd-kr260.xml $(OUTDIR)
	cp start.sh $(OUTDIR)

clean:
	-rm -f $(APP) *.elf *.gdb $(OUTDIR)/*.o

%.o: ../%.c
#	$(CC) -c $(CFLAGS) -o $(OUTDIR)/$@	
	$(CC) $(CFLAGS) -c $< -o $(OUTDIR)/$@	
	

