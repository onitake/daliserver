#include <stdio.h>
#include <stdint.h>
#include "pack.h"
#include "util.h"

struct Test {
	unsigned int a;
	char b;
	int64_t c;
	float d;
};

int main(int argc, char **argv) {
	struct Test t;
	t.a = 0x7239223;
	t.b = 43;
	t.c = 0x9243908329901289L;
	t.d = 0.00004392;
	printf("0x%x 0x%x 0x%16llx 0x%x\n", t.a, t.b, t.c, *(unsigned int *) &t.d);
	size_t length = 0;
	uint8_t *buffer = pack(">Iclf", NULL, &length, t.a, t.b, t.c, t.d);
	hexdump(buffer, length);
	struct Test u;
	unpack(">Iclf", buffer, &length, &u.a, &u.b, &u.c, &u.d);
	printf("0x%x 0x%x 0x%16llx 0x%x\n", u.a, u.b, u.c, *(unsigned int *) &u.d);
	return 0;
}
