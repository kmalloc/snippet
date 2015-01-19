
def get_odd(ls):
    def is_odd(x):
        return x % 2
    return filter(is_odd, ls)

print get_odd([1, 3, 5, 0, 4, 6])


class DummyClass(object):

    def __init__(self, age):
        self.__age = age

    def prt(self, extra):
        print "age:", self.__age, ", extra info:", extra

    def setAge(self, age):
        self.__age = age

    def __str__(self):
        return "dummy string"

    def __len__(self):
        return 32


def print_cs(c, a = -1):
    c.prt("")
    if a > 0:
        c.setAge(a)
        c.prt("after set age")


c = DummyClass(23)

print_cs(c)
print_cs(c, 32)

print "len of DummyClass:" + str(len(c))
print "str of DummyClass:" + str(c)

print "show properties of DummyClass"
print "dir(DummyClass):", dir(DummyClass)
