#NAME: Daniel Adea
#EMAIL: dadea@ucla.edu
#ID: 204999515

default:
	gcc -g lab4c_tcp.c -o lab4c_tcp -lmraa -lm -Wall -Wextra
	gcc -g lab4c_tls.c -o lab4c_tls -lmraa -lm -Wall -Wextra -lssl -lcrypto

dist:
	tar -cvzf lab4c-204999515.tar.gz lab4c_tcp.c lab4c_tls.c Makefile README  

clean:
	rm -f lab4c_tcp lab4c_tls *.tar.gz
