#include <vector>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;

int Split(const string& str, char delim, vector<string>& out)
{
    int c = 0;
    stringstream ss(str);
    string item;

    while (getline(ss, item, delim))
    {
        ++c;
        out.push_back(item);
    }

    return c;
}

int main()
{
    string str("abc;;cd;ef;");
    vector<string> out;

    int c = Split(str, ';', out);

    cout << "total sub items: " << c << endl;

    if (c) cout << out[0];

    for (int i = 1; i < c; ++i)
        cout << ", " << out[i];

    cout << endl;

    return 0;
}

