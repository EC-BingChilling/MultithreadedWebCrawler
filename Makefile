.PHONY: all run clean install_curl

all:
	@gcc -std=c11 -pedantic -pthread crawler.c -lcurl -o crawler

run:
	@./crawler

clean:
	@rm -f crawler page*.html

install_curl:
	sudo apt-get update && sudo apt install libcurl4-openssl-dev
