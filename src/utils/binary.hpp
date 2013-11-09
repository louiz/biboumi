#ifndef BINARY_INCLUDED
# define BINARY_INCLUDED

template<char FIRST, char... REST> struct binary
{
  static_assert(FIRST == '0' || FIRST == '1', "invalid binary digit" );
  enum { value = ((FIRST - '0') << sizeof...(REST)) + binary<REST...>::value };
};

template<> struct binary<'0'> { enum { value = 0 }; };
template<> struct binary<'1'> { enum { value = 1 }; };

template<char... LITERAL> inline
constexpr unsigned int operator "" _b() { return binary<LITERAL...>::value; }

#endif // BINARY_INCLUDED
