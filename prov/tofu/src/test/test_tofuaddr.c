#include <pmix.h>
#include <pmix_fjext.h>
#include <process_map_info.h>
#include <stdio.h>

extern void tofuaddr(tlib_tofu6d start, tlib_tofu6d size);

int
main(int argc, char **argv)
{
    tlib_tofu6d	start, size;
    int	x, y, z, a, b, c;
    if (argc != 3) {
	fprintf(stderr, "%s: <start tofu x:y:z:a:b:c> <size x:y:z:a:b:c>\n"
		"\t eg. %s 17:18:20:0:0:0 11:2:1:2:3:2\n", argv[0], argv[0]);
	exit(-1);
    }
    sscanf(argv[1], "%d:%d:%d:%d:%d:%d", &x, &y, &z, &a, &b, &c);
    printf("%d:%d:%d:%d:%d:%d\n", x, y, z, a, b,c);
    start = 0;
    start =  TLIB_SET_X(start, x) | TLIB_SET_Y(start, y) | TLIB_SET_Z(start, z)
	| TLIB_SET_A(start, a) | TLIB_SET_B(start, b)  | TLIB_SET_C(start, c);
    sscanf(argv[2], "%d:%d:%d:%d:%d:%d", &x, &y, &z, &a, &b, &c);
    printf("%d:%d:%d:%d:%d:%d\n", x, y, z, a, b,c);
    size = 0;
    size =  TLIB_SET_X(size, x) | TLIB_SET_Y(size, y) | TLIB_SET_Z(size, z)
	| TLIB_SET_A(size, a) | TLIB_SET_B(size, b)  | TLIB_SET_C(size, c);
    tofuaddr(start, size);
    return 0;
}
