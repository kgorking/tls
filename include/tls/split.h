#ifndef TLS_SPLIT_H
#define TLS_SPLIT_H

#include "collect.h"

namespace tls {
template <typename T, typename UnusedDifferentiaterType = void>
using split = collect<T, none, UnusedDifferentiaterType>;

template <typename T, auto U = [] {}>
using unique_split = split<T, decltype(U)>;

} // namespace tls

#endif // !TLS_SPLIT_H
