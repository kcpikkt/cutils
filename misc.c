char *rand_key() 
{
  ssize_t sz = rand() % 30 + 10;
  char *key = RB_REALLOC(NULL, sz);
  for(int i = 0; i < sz-1; ++i)
    key[i] = (rand() % 36) + 48;
  key[sz-1] = '\0';
  return key;
}


