typedef enum my_enum
{
  A = 1,
  B = 2
};
int my_array[10];

int main()
{
  // should fail
  (enum my_enumt)3;

  // should fail
  my_array[(enum my_enumt)4] = 10;

  return 0;
}
