WUPPER Tools
======================================

This directory contains a collection of tools useful for controlling
and debugging the FPGA card.


1. Requirements
---------------

The drivers 'wupper' and 'cmem' have to be loaded and working. This can
be verified with 'cat /proc/cmem' and 'cat /proc/wupper'. See the
documentation in 'hostSoftware/wupper/README'.


2. Compilation
--------------

$ cd hostSoftware/wuppertools
$ mkdir build
$ cd build
$ cmake ..
$ make 

3. Usage
--------

All tools have a '-h' option to display a hopefully helpful message.

In general, when using different devices, It's possible to specify a device using "-d" option. (0 is used by default)

	EG: ./wupper-info -d 1

Additionally, most of the tools have a verbose option available, which displays debugging information.

	EG: ./wupper-init -v


3.1 wupper-config

Allows to look at the PCIe configuration registers, set configuration
parameters, store and load configuration. 

wupper-config accepts the following commands:

	-list
	-set KEY=VALUE
	-store FILENAME
	-load FILENAME

Using "./wupper-config list", a list of availables register, their addresses and values is displayed. 
To set a value in one on the registers, It's necessary to use the registered name displayed on wupper-config list.

	EG: ./wupper-config set STATUS_LEDS=0x1 -> Write 0x1 to register 0x0010.

You can save snapshots from the wupper device with "./wupper-config store file" and load it using "./wupper-config load file", overwriting all registers.

	EG: ./wupper-config store myfile
		./wupper-config load myfile


3.2 wupper-dma-test

Running this will program a 1024 Byte DMA transfer every second
and dump it to the screen, useful for debugging.

3.3 wupper-dump-blocks

Dumps the specified amount of 1KB blocks into the given file.


3.4 wupper-info

Prints information from different devices. This tools accepts one command to specify which information shows.

	-ALL
Display information for all options

In case no command is used, general information about wupper card is displayed.

Some examples are:

	./wupper-info                 ->Display general info from wupper device.
	./wupper-info -d 1            ->Display general information from device 1.


3.5 wupper-irq-test

Program I use to test for interrupts. It simply waits for an interrupt
to be fired.

3.6 wupper-throughput

A program that measures the throughput of the DMA engine. 
wupper-throughput accepts the following commands:

 -d NUMBER      Use card indicated by NUMBER. Default: 0.
 -b NUM         Use a buffer of size NUM blocks. Default: 100.
 -w             Use circular buffer wraparound mechanism.
 -h             Display help.


3.7 wupper-reset

Used to reset different parts of the card. Available command are:
	-flush          ->Flushes the main output FIFO.
	-reset          ->Resets Wupper core
	-soft-reset     ->Resets Wupper aplication.
	-all            ->Does all three previous commands.

Examples are:
	./wupper-reset flush          ->Flushes the main output FIFO from wupper device 0.
	./wupper-reset flush -d 1     ->Flushes the main output FIFO from wupper device 1.
	./wupper-reset all            ->Does everything for device 0.
	./wupper-reset                ->Displays an error.



