
def get_odd(ls):
    def is_odd(x):
        return x % 2
    return filter(is_odd, ls)

print get_odd([1, 3, 5, 0, 4, 6])


class DummyClass(object):

    def __init__(self, age):
        self.__age = age

    def Print(self, extra):
        print "age:", self.__age, ", extra info:", extra

    def SetAge(self, age):
        self.__age = age


def print_cs(c, a = -1):
    c.Print("")
    if a > 0:
        c.SetAge(a)
        c.Print("after set age")


print "dir(DummyClass):", dir(DummyClass)

c = DummyClass(23)

print_cs(c)
print_cs(c, 32)
