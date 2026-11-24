FOLDER=cminus-$(VER)-linux-x86_64
DIR=releases/$(FOLDER)

all: src/*.cpp
	mkdir -p bin
	g++ -Iinclude src/*.cpp -o bin/main
	sudo cp bin/main /usr/local/bin/cm

release:
	mkdir -p "$(DIR)"
	mkdir -p "$(DIR)/stdlib"
	cp bin/main "$(DIR)/cm"
	cp releases/Makefile releases/README.md "$(DIR)"
	cp /home/seagullson/Desktop/cm/stdlib/src/* "$(DIR)/stdlib"
	rm -f "$(DIR)/stdlib/main.cm"
	tar czf "$(DIR).tar.gz" -C releases $(FOLDER)