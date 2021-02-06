# Simple pulse counter which sends data over MQTT

* This counter uses interrupts on ESP-01's GPIO3.
* It is "talk-only" client. 
* It sends out data while pulses detected.
* It keeps adding pulses until poweroff/reset.

### MQTT Message consists of 
       pulses - Number of pulses counted since last device boot.
       moved_last_half_hour - "1" if rising or falling edges were detected on last half hour.
       last_pulse - Last falling pulse detected datetime.
       since - ESP8266 last boot date.
  
![Schematic](https://github.com/logon84/waterflow_sensor_aka_pulse_counter/blob/master/schematic.png?raw=true)

Copyright (C) 2016 Anton Viktorov <latonita@yandex.ru>  
Mod by logon84@github 2021

This library is free software. You may use/redistribute it under The MIT License terms. 
