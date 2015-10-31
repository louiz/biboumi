#ifndef BIBOUMI_IO_TESTER_HPP
#define BIBOUMI_IO_TESTER_HPP

#include <ostream>
#include <sstream>

/**
 * Redirects a stream into a streambuf until the object is destroyed.
 */
template <typename StreamType>
class IoTester
{
public:
  IoTester(StreamType& ios):
    stream{},
    ios(ios),
    old_buf(ios.rdbuf())
  {
    // Redirect the given os into our stringstream’s buf
    this->ios.rdbuf(this->stream.rdbuf());
  }
  ~IoTester()
  {
    this->ios.rdbuf(this->old_buf);
  }
  IoTester& operator=(const IoTester&) = delete;
  IoTester& operator=(IoTester&&) = delete;
  IoTester(const IoTester&) = delete;
  IoTester(IoTester&&) = delete;

  std::string str() const
  {
    return this->stream.str();
  }

  void set_string(const std::string& s)
  {
    this->stream.str(s);
  }

private:
  std::stringstream stream;
  StreamType& ios;
  std::streambuf* const old_buf;
};

#endif //BIBOUMI_IO_TESTER_HPP
