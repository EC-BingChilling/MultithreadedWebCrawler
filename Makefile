all: 
	gcc -std=c11 -pedantic -pthread -lcurl crawler.c -lcurl -o crawler

install_curl:
	sudo apt-get update
	sudo apt install libcurl4-openssl-dev
clean: 
	rm -f crawler page*.html 
run: 
	./crawler 
 