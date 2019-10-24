#include <thread>
#include <mutex>
#include <iostream>
#include <chrono>

class foo
{
    private:
        std::mutex* mut;
        int attr01;
        int attr02;

    public:
        foo(int i) {
            attr01 = i;
            attr02 = 1;
            mut = new std::mutex();
        }

        void f(int j){
            
            std::lock_guard<std::mutex> lock (*mut);
            
            if(this->attr01 == 5 && j == 1)
                std::this_thread::sleep_for(std::chrono::seconds(2));
            
            this->attr02 = this->attr02 + 1;

            std::cout<<"\nATTR01::"<<this->attr01<<"    ATTR02::"<<this->attr02<<"    ADDRESS ATTR02::" << &this->attr02<<"    THREAD ::"<<j<<std::endl;
        }
};

int main()
{
    foo f1(5);
    foo f2(6);
    
    
    std::thread foo_thread1(&foo::f,&f1,1);
    std::thread foo_thread2(&foo::f,&f2,1);
    std::thread foo_thread3(&foo::f,&f1,2);
    std::thread foo_thread4(&foo::f,&f1,3);
    
    
    
    foo_thread1.join();
    foo_thread2.join();
    foo_thread3.join();
    foo_thread4.join();

    return 0;
}
