#ifndef XMPP_BODY_HPP_INCLUDED
#define XMPP_BODY_HPP_INCLUDED

namespace Xmpp
{
// Contains:
// - an XMPP-valid UTF-8 body
// - an XML node representing the XHTML-IM body, or null
  typedef std::tuple<const std::string, std::unique_ptr<XmlNode>> body;
}

#endif /* XMPP_BODY_HPP_INCLUDED */
