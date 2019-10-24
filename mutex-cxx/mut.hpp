#include <string>
#include <iostream>
#include <mutex>

class mut {

    public:

        mut(int data) {
            this->data = data;
        };
        
        void test(int i) {
            
            std::lock_guard<std::mutex> guard(datamut);

            std::cout<<"PRINTING ::"<<this->data<<std::endl;
        };

    private:
        std::mutex datamut;
        int data;

};
