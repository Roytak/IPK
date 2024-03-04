
all: cmake

cmake:
	{ \
	mkdir build; \
	cd build; \
	cmake ../; \
	make; \
	cp ./ipkcpc ../; \
	cd ..; \
		}

clean:
	rm -rf build/ ipkcpc
