OBJECTS=\
	   main.o \
	   inih/ini.o \
	   bluez/lib/hci.o \
	   bluez/lib/bluetooth.o  \
		
bacon: $(OBJECTS)
	$(CC) $^ -o $@


clean:
	rm -f $(OBJECTS)

