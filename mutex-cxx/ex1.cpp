#include <thread>
#include <mutex>
#include <iostream>

class Beta
{
    public:
        void Gamma(int y)
        {
            std::unique_lock<std::mutex> lck(mtx);    
            std::cout << y << std::endl;
        }
    private:
        std::mutex mtx;
};

int main()
{
    Beta my_beta;
    std::thread gamma_thread(&Beta::Gamma,my_beta, 5);
    gamma_thread.join();

    return 0;
}
