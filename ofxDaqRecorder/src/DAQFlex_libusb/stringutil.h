#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include <string>

using namespace std;

class StringUtil
{
    public:
        static void toUpper(string s);
        static bool contains(string original, string substring);
};

#endif
