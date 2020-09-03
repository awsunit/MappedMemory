#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{

  char boy[4] = "boy";
  char *b = &boy[0];

  b += 10;

  printf("b is: %c", *b);

  pass("cc");
  exit(0);
  return 0;

}
