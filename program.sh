while(true) do avrdude -c usbtiny -C /Applications/Arduino.app/Contents/Java/hardware/tools/avr/etc/avrdude.conf -p m32u4 -b 115200 -V -U efuse:w:0xFF:m  -U lfuse:w:0xFF:m -U hfuse:w:0xD8:m -U flash:w:shibboleth.hex; sleep 5; done