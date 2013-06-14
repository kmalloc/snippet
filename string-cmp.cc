#include <string.h>
#include <iostream>

using namespace std;


static char* strstr(register const char* s, register const char* wanted)
{
    register const size_t len = strlen(wanted);

    if (len == 0) return (char*)s;

    while (*s != *wanted || strncmp(s, wanted, len))
        if (*s++ == '\0')
            return (char*)NULL;

    return s;

}


void GetNext(register const char* s, register int* next)
{
    if (s == NULL || next == NULL || *s == 0 || *(s+1) == 0) return;

    char* p = s;
    int i = 2, k = i - 1;
    next[0] = 0;
    next[1] = 0;

    while (*(s + i))
    {
        if (s[i - 1] == s[next[k]])
        {//we can have an optimization here. TODO
            next[i] = next[k] + 1;
            k = i++;
        }
        else if (k == 0)
        {
            next[i] = 0;
            k = i++;
        }
        else
        {
            k = next[k];
        }
    }
}


char* kmp(register const char* s, register const char* wanted)
{
    if (s == NULL || wanted == NULL || *s == 0 || *wanted == 0) return (char*)NULL;

    int len = strlen(wanted);
    
    int* next = (int*)malloc(len*sizeof(int));

    if (next == NULL) return NULL;

    GetNext(wanted, next);

    int i = 0, j = 0;

    while (*(s + i) && *(wanted + j))
    {
        if (*(s + i) == *(wanted + j))
        {
            ++i;
            ++j;
        }
        else if (j == 0)
        {
            i++;
        }
        else 
        {
            j = next[j];
        }
    }

    free(next);

    if (*(wanted + j) == 0)
    {
        return s - i -j;
    }

    return NULL;
}

int main()
{

    return 0;
}
