#include "stringutil.h"

#include <string>
#include <ctype.h>

using namespace std;

void StringUtil::toUpper(string s)
{
    for(int i = 0; i < s.length(); i++)
    {
        s[i] = toupper(s[i]);
    }
}

bool StringUtil::contains(string original, string substring)
{
    size_t found;

    found=original.find(substring);
    return (found!=string::npos);
}
