
build:
	mkdir -p bin
	gcc hs.c -lreadline -o bin/hs

build_debug:
	mkdir -p bin
	gcc hs.c -lreadline -o bin/hs -O0 -g

clean:
	rm .hs_state/*
	rm bin/*
	