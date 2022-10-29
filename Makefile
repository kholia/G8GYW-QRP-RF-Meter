default:
	arduino-cli compile --fqbn=rp2040:rp2040:rpipico QRP_POWER_METER-v2 -e
	arduino-cli -v upload -p /dev/ttyACM0  --fqbn=rp2040:rp2040:rpipico QRP_POWER_METER-v2

install_platform:
	arduino-cli config init --overwrite
	arduino-cli core update-index
	arduino-cli core install rp2040:rp2040

install_arduino_cli:
	mkdir -p ~/.local/bin
	curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh

deps:
	arduino-cli lib install "Adafruit SSD1306"

format:
	astyle --options=formatter.conf QRP_POWER_METER-v2/QRP_POWER_METER-v2.ino


setup_system:
	sudo apt install python3-pip git vim make screen -y
	python -m pip install pyserial
