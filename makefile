CC = avr-g++
OBJCOPY = avr-objcopy
SIZE = avr-size

MCU = atmega328p
TARGET = demo

# Output format. (can be srec, ihex, binary)
FORMAT = ihex

AVRDUDE = avrdude
AVRDUDE_PROGRAMMER = usbtiny

AVRDUDE_WRITE_FLASH = -U flash:w:$(TARGET).hex
# AVRDUDE_WRITE_EEPROM = -U eeprom:w:$(TARGET).eep
AVRDUDE_FLAGS = -p $(MCU) -c $(AVRDUDE_PROGRAMMER)

all:
	$(CC) main.cpp -mmcu=$(MCU) -std=c++17 --output $(TARGET).elf -D F_CPU=16000000 -O2 -Wall -Wextra
	$(OBJCOPY) -O $(FORMAT) -R .eeprom $(TARGET).elf $(TARGET).hex
	$(SIZE) -A $(TARGET).elf

flash:
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_WRITE_FLASH) $(AVRDUDE_WRITE_EEPROM)
