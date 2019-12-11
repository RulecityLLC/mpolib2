//
// Created by mownby on 12/10/2019.
//

#ifndef MPO_MPO_MISC_INTERNAL_H
#define MPO_MPO_MISC_INTERNAL_H

// if we're on C++11, then unique_ptr is standard
#if __cplusplus >= 199711L

#include <memory>
#define SHARED_ARRAY(T) std::unique_ptr<T []>

#else

#include <boost/shared_array.hpp>
using namespace boost;

#define SHARED_ARRAY(T) boost::shared_array<T>

#endif

#endif //MPO_MPO_MISC_INTERNAL_H
