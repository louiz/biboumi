#ifndef Stanza
# define Stanza

class Stanza
{
public:
  explicit Stanza();
  ~Stanza();
private:
  Stanza(const Stanza&) = delete;
  Stanza(Stanza&&) = delete;
  Stanza& operator=(const Stanza&) = delete;
  Stanza& operator=(Stanza&&) = delete;
};

#endif // Stanza


