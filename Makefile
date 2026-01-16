update: compile
	arduino-cli upload -p /dev/ttyUSB0

compile:
	arduino-cli compile .

.PHONY: update compile