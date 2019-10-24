#include "mut.hpp"
#include <iostream>
#include <thread>

int main() 
{
    mut t1(5);

    std::thread mutT1(&mut::test,t1,1);


    mutT1.join();

    return 0;
}
