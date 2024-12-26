int some_int;
int *pointer_global;

void foobar(int *pointer_parameter)
{
  int *pointer_local;
  pointer_local = &some_int;
}
