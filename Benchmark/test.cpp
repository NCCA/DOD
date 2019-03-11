#include "Benchmark.h"
#include <iostream>
#include <thread>
#include <random>
int main()
{
//  Benchmark<std::chrono::high_resolution_clock,std::chrono::milliseconds> b;
  Benchmark<> b;
  using namespace std::chrono_literals;
  std::random_device seed_generator;
  std::mt19937 gen(seed_generator());
  std::uniform_int_distribution<> distribution(30,400);

  for(int i=0; i<50; ++ i)
  {
    auto start = std::chrono::high_resolution_clock::now();
    auto sleepTime=distribution(gen);
    std::this_thread::sleep_for( std::chrono::duration<int, std::milli> (sleepTime));
    std::cout<<"Tick "<<sleepTime<<'\n';
    auto end = std::chrono::high_resolution_clock::now();
    b.addDuration(end-start);
  }
  std::cout<<"Min "<<b.min()<<" Max "<<b.max()
          <<" Average "<<b.average()
         <<" Median "<<b.median()<<'\n';
}
