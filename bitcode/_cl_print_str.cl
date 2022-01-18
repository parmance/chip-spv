
void _cl_print_str(__generic const char* s) {
  unsigned pos = 0;
  char c;
  while ((c = s[pos]) != 0) {
    printf("%c", c);
    ++pos;
  }
}
