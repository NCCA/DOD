#ifndef BENCHMARK_H_
#define BENCHMARK_H_
#include <chrono>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>
/// @brief simple Benchmark class for timing things

template<typename Clock=std::chrono::high_resolution_clock, typename Resolution=std::chrono::microseconds>
class Benchmark
{

public:

    Benchmark()=default;
    Benchmark(size_t _reserve)
    {
        m_durations.reserve(_reserve);
    }
    Benchmark(const Benchmark&)=delete;
    // add a time duration
    void addDuration(typename Clock::duration now)
    {
      m_durations.push_back(now);
      if (now < m_min) 
      {
          m_min = now;
      }

      if (now > m_max) 
      {
        m_max = now;
      }
    }

    auto average()
    {
      typename Clock::duration result{0};
      for (auto times : m_durations) 
      {
          result +=times;
      }
      result /= m_durations.size();
      return std::chrono::duration_cast<Resolution>(result).count();
    }


    auto mode()
    {
      // Map of tick and count
      std::unordered_map<size_t,size_t> histogram;
      for(auto d : m_durations)
      {
        auto key=std::chrono::duration_cast<Resolution>(d).count();
        auto it=histogram.find(key);
        // if we don't have this time add
        if(it ==std::end(histogram))
          histogram[key]=1; // first count
        else // found
          ++it->second; // otherwise increment
      }
      auto max=std::max_element(std::begin(histogram),std::end(histogram),
      [](const auto& p1, const auto& p2)
      {
        return p1.second < p2.second;
      });
      return max->first;
    }

    auto max()
    {
        return std::chrono::duration_cast<Resolution>(m_max).count();
    }

    auto min()
    {
        return std::chrono::duration_cast<Resolution>(m_min).count();
    }

    auto median()
    {
        std::sort(std::begin(m_durations), std::end(m_durations));
        return std::chrono::duration_cast<Resolution>(m_durations[m_durations.size() / 2]).count();
    }

  private :
    std::vector<typename Clock::duration> m_durations;
    typename Clock::duration m_min{Clock::duration::max()};
    typename Clock::duration m_max{Clock::duration::min()};


};

#endif
