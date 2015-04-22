int global = 100;

int function(int i)
{
    return i + global;
}

int caller(int i)
{
    function(i + 100);
}


