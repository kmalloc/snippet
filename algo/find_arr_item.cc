#include <iostream>
using namespace std;

/*
find item that is:
1) >= those items in the left side
2) <= those items in the right side.
*/
int findsth(int lmax, int cur, int arr[], int len)
{
    int nextLeftMax = (arr[lmax] < arr[cur]? cur : lmax);
    int rMin;

    if (cur < len - 1)
    {
        rMin = findsth(nextLeftMax, cur + 1, arr, len);
    }
    else
    {
        rMin = cur;
    }

    if (arr[cur] >= arr[lmax] && arr[cur] <= arr[rMin])
    {
        cout << cur << "th:" << arr[cur] << endl;
    }

    if (arr[cur] > arr[rMin]) return rMin;

    return cur;
}

#define ArrSize(arr) (sizeof(arr)/sizeof(arr[0]))

void test()
{
    int c1[] = {3, 4, 2, 7, 9, 5, 6, 8, 9, 10, 9, 11, 15, 12};

    cout << "case1: " << c1[0];
    for (int i = 1; i < ArrSize(c1); ++i)
        cout << ", " << c1[i];

    cout << endl;
    findsth(0, 0, c1, ArrSize(c1));

    int c2[] = {5, 6, 2, 4, 6, 7, 9, 8};

    cout << endl;
    cout << "case2: " << c2[0];
    for (int i = 1; i < ArrSize(c2); ++i)
        cout << ", " << c2[i];

    cout << endl;
    findsth(0, 0, c2, ArrSize(c2));
}

int main()
{
    test();
    return 0;
}

