# tigo_server
Arduino Sketch to read Tigo data between CCA and TAP

Hardware:
- Esp32s3 connected via a RS485 board to the Bus. Power can be also get from there via a voltage regulator (depending the voltage).

Software:
- Arduino Sketch
- HTML file to read the Websocket

Further Information:

Tigo sends the information from ShortID to LongID(Barcode on Modules) only once a day, or after Startup. As long as there is no nodetable available the data table is not complete. However you can save the nodetable in spiffs, so that its always available (tigoserver.local/debug). Files in Spiffs can be edited tigoserver.local/spiffs. There you also put the index.html of your layout so that you can easy see it under tigoserver.local

Open Points:
- Calculating the Checksum of the Barcode
- Including Timestamps (Use internal Slot Counter)
- Logging
- etc...

Thanks goes to Willglynn who offered the details of the Tigo Protocoll!
https://github.com/willglynn/taptap/
