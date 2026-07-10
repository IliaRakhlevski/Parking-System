#include "database.hpp"
#include <iostream>

int main()
{
    Database database;

    if (!database.initialize())
    {
        return 1;
    }

    if (!database.shutdown())
    {
        return 1;
    }

    return 0;
}