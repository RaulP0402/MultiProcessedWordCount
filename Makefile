all: wc wc_mul

wc: wc.c
	$(CC) wc.c -g -o wc

wc_mul: wc_mul.c
	$(CC) wc_mul.c -g -o wc_mul

clean:
	rm -rf wc wc_mul