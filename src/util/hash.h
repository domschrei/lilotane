
#ifndef DOMPASCH_TREE_REXX_HASH_H
#define DOMPASCH_TREE_REXX_HASH_H

template <class T>
inline void hash_combine(std::size_t & s, const T & v)
{
  static robin_hood::hash<T> h;
  s ^= h(v) + 0x9e3779b9 + (s<< 6) + (s>> 2);
}

#endif